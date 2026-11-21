#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Derive a Galaxy S10 (beyond1lte) or S10e (beyond0lte) UEFI platform from
the S10+ overlay.

Run on a disposable Mu-Silicium tree that already contains the beyond2lte
overlay. The hardware sources (memory map, UFS, HSI2C touch, keypad) are kept
unchanged: the S10 and S10e share reserved memory layout, UFS, HSI2C10/sec_ts,
GPA keys, SPEEDY/PMIC and DECON nodes with the S10+. The platform identity,
the appended DTB and the boot configuration name differ; on the S10e the
frame buffer is also 1080x2280 and only 6 GiB of DDR exist (the missing upper
2 GiB is reserved by scripts/prepare-boot-firmware.py, see b2l_board.py).

The upstream beyond1ltePkg/beyond0ltePkg is replaced: the S10+ memory map
already keeps the DXE heap off the 0xCA000000 display reservation.
"""
import argparse
from pathlib import Path
import re
import shutil
import struct

import b2l_board

B2L_PLATFORM_GUID = '556BDA6E-EBC8-4D8F-8EA6-DAA2D7AD7A68'
TARGETS = {
    # Upstream Mu-Silicium beyond1ltePkg identity, reused so the package name
    # and GUID stay consistent with Mu-Silicium.
    'beyond1lte': {'guid': '4E670446-C67E-4FEB-8D15-A0A05C824B80', 'dtb_model': b'BEYOND1LTE'},
    # Generated for this port; the upstream beyond0ltePkg is not reused.
    'beyond0lte': {'guid': 'B0E10D5E-6C3A-4F0E-9A1D-5E10E0970F00', 'dtb_model': b'BEYOND0LTE'},
}
TEXT_SUFFIXES = {'.c', '.h', '.S', '.inf', '.inc', '.dsc', '.fdf', '.dec', '.py'}
SOURCE_PANEL = (1440, 3040)


def retarget_framebuffer(text, panel):
    """Rewrite every frame-buffer width/height PCD from the S10+ panel."""
    if panel == SOURCE_PANEL:
        return text, 0
    count = 0
    for axis, old, new in (('Width', SOURCE_PANEL[0], panel[0]), ('Height', SOURCE_PANEL[1], panel[1])):
        text, n = re.subn(r'(FrameBuffer%s\s*\|\s*)%d\b' % (axis, old), r'\g<1>%d' % new, text)
        count += n
    return text, count


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mu', type=Path, required=True, help='disposable Mu-Silicium tree with the beyond2lte overlay')
    p.add_argument('--dtb', type=Path, required=True, help="the target phone's merged DTB (live /sys/firmware/fdt)")
    p.add_argument('--board', choices=sorted(TARGETS), default='beyond1lte')
    a = p.parse_args()
    board = a.board
    target_info = TARGETS[board]
    profile = b2l_board.profile(board)
    root = a.mu.resolve()
    if not (root / 'Mu_Basecore').is_dir():
        p.error('--mu must be a Mu-Silicium build tree')
    source = root / 'Platforms/Samsung/beyond2ltePkg'
    target = root / f'Platforms/Samsung/{board}Pkg'
    dsc = source / 'beyond2lte.dsc'
    if not dsc.is_file() or B2L_PLATFORM_GUID not in dsc.read_text():
        p.error('the beyond2lte overlay is not applied to this tree')
    dtb = a.dtb.read_bytes()
    if len(dtb) < 40 or struct.unpack_from('>I', dtb)[0] != 0xd00dfeed:
        p.error('--dtb is not a flattened device tree')
    if target_info['dtb_model'] not in dtb:
        p.error(f'--dtb does not describe {board}')

    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)
    for name in ('dsc', 'fdf', 'dec'):
        (target / f'beyond2lte.{name}').rename(target / f'{board}.{name}')
    smbios = (('"Galaxy S10+"', '"%s"' % profile['name']),
              ('"Galaxy_S10+_beyond2lte"', '"%s_%s"' % (profile['name'].replace(' ', '_'), board)))
    framebuffer_edits = 0
    for path in target.rglob('*'):
        if not path.is_file() or path.suffix not in TEXT_SUFFIXES:
            continue
        text = path.read_text()
        new = text.replace('beyond2ltePkg', f'{board}Pkg').replace('samsung-beyond2lte', f'samsung-{board}')
        # Package files renamed above (dsc, fdf, dec) are referenced by path.
        new = new.replace(f'{board}Pkg/beyond2lte.', f'{board}Pkg/{board}.')
        if path.name == f'{board}.dsc':
            new = new.replace(B2L_PLATFORM_GUID, target_info['guid'])
            new = new.replace('PLATFORM_NAME                  = beyond2lte', f'PLATFORM_NAME                  = {board}')
            new = new.replace('|"beyond2lte"', f'|"{board}"')
            for old, replacement in smbios:
                new = new.replace(old, replacement)
        if path.name == 'DeviceBuild.py':
            new = new.replace("'beyond2lte'", f"'{board}'").replace('"beyond2lte"', f'"{board}"')
        if path.suffix in {'.dsc', '.inc', '.fdf'}:
            new, n = retarget_framebuffer(new, profile['panel'])
            framebuffer_edits += n
        if new != text:
            path.write_text(new)

    dsc_text = (target / f'{board}.dsc').read_text()
    for needle in (target_info['guid'], f'PLATFORM_NAME                  = {board}',
                   f'{board}Pkg/{board}.fdf') + tuple(new for _, new in smbios):
        if needle not in dsc_text:
            p.error(f'{board}.dsc is missing {needle!r}')
    if profile['panel'] != SOURCE_PANEL and framebuffer_edits < 2:
        p.error('frame-buffer width/height PCDs not found; set the %dx%d panel by hand' % profile['panel'])
    leftovers = [str(path.relative_to(target)) for path in target.rglob('*')
                 if path.is_file() and path.suffix in {'.dsc', '.fdf', '.inf', '.inc', '.py'}
                 and 'beyond2lte' in path.read_text()]
    if leftovers:
        p.error('beyond2lte references remain in build files: ' + ', '.join(leftovers))

    configs = root / 'Resources/Configs'
    shutil.copyfile(configs / 'beyond2lte.toml', configs / f'{board}.toml')
    (root / f'Resources/DTBs/{board}.dtb').write_bytes(dtb)
    stale = root / f'Resources/DTBs/{board}.dts'
    if stale.exists():
        stale.unlink()
    print(f'Prepared {target} from the beyond2lte overlay; DTB {len(dtb)} bytes; '
          f'{framebuffer_edits} frame-buffer PCD edits')


if __name__ == '__main__':
    main()
