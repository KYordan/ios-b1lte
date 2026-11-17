#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Derive the Galaxy S10 (beyond1lte) UEFI platform from the S10+ overlay.

Run on a disposable Mu-Silicium tree that already contains the beyond2lte
overlay. The hardware sources (memory map, UFS, HSI2C touch, keypad) are kept
unchanged: on DT revision 26 the S10 and S10+ share reserved memory, UFS,
HSI2C10/sec_ts, GPA keys, SPEEDY/PMIC and DECON nodes. Only the platform
identity, the appended DTB and the boot configuration name differ.

The upstream beyond1ltePkg is replaced: its DXE heap overlaps the 0xCA000000
display reservation, which the S10+ memory map already corrects.
"""
import argparse
from pathlib import Path
import shutil
import struct

# Upstream Mu-Silicium beyond1ltePkg identity, reused so the package name and
# GUID stay consistent with Mu-Silicium.
B1L_PLATFORM_GUID = '4E670446-C67E-4FEB-8D15-A0A05C824B80'
B2L_PLATFORM_GUID = '556BDA6E-EBC8-4D8F-8EA6-DAA2D7AD7A68'
SMBIOS = (('"Galaxy S10+"', '"Galaxy S10"'), ('"Galaxy_S10+_beyond2lte"', '"Galaxy_S10_beyond1lte"'))
TEXT_SUFFIXES = {'.c', '.h', '.S', '.inf', '.inc', '.dsc', '.fdf', '.dec', '.py'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mu', type=Path, required=True, help='disposable Mu-Silicium tree with the beyond2lte overlay')
    p.add_argument('--dtb', type=Path, required=True, help='merged S10 DTB (dtb.img base + dtbo revision 26)')
    a = p.parse_args()
    root = a.mu.resolve()
    if not (root / 'Mu_Basecore').is_dir():
        p.error('--mu must be a Mu-Silicium build tree')
    source = root / 'Platforms/Samsung/beyond2ltePkg'
    target = root / 'Platforms/Samsung/beyond1ltePkg'
    dsc = source / 'beyond2lte.dsc'
    if not dsc.is_file() or B2L_PLATFORM_GUID not in dsc.read_text():
        p.error('the beyond2lte overlay is not applied to this tree')
    dtb = a.dtb.read_bytes()
    if len(dtb) < 40 or struct.unpack_from('>I', dtb)[0] != 0xd00dfeed:
        p.error('--dtb is not a flattened device tree')
    if b'BEYOND1LTE' not in dtb:
        p.error('--dtb does not describe beyond1lte')

    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)
    for name in ('dsc', 'fdf', 'dec'):
        (target / f'beyond2lte.{name}').rename(target / f'beyond1lte.{name}')
    for path in target.rglob('*'):
        if not path.is_file() or path.suffix not in TEXT_SUFFIXES:
            continue
        text = path.read_text()
        new = text.replace('beyond2ltePkg', 'beyond1ltePkg').replace('samsung-beyond2lte', 'samsung-beyond1lte')
        # Package files renamed above (dsc, fdf, dec) are referenced by path.
        new = new.replace('beyond1ltePkg/beyond2lte.', 'beyond1ltePkg/beyond1lte.')
        if path.name == 'beyond1lte.dsc':
            new = new.replace(B2L_PLATFORM_GUID, B1L_PLATFORM_GUID)
            new = new.replace('PLATFORM_NAME                  = beyond2lte', 'PLATFORM_NAME                  = beyond1lte')
            new = new.replace('|"beyond2lte"', '|"beyond1lte"')
            for old, replacement in SMBIOS:
                new = new.replace(old, replacement)
        if path.name == 'DeviceBuild.py':
            new = new.replace("'beyond2lte'", "'beyond1lte'").replace('"beyond2lte"', '"beyond1lte"')
        if new != text:
            path.write_text(new)

    dsc_text = (target / 'beyond1lte.dsc').read_text()
    for needle in (B1L_PLATFORM_GUID, 'PLATFORM_NAME                  = beyond1lte',
                   'beyond1ltePkg/beyond1lte.fdf', '"Galaxy S10"', '"Galaxy_S10_beyond1lte"'):
        if needle not in dsc_text:
            p.error(f'beyond1lte.dsc is missing {needle!r}')
    leftovers = [str(path.relative_to(target)) for path in target.rglob('*')
                 if path.is_file() and path.suffix in {'.dsc', '.fdf', '.inf', '.inc', '.py'}
                 and 'beyond2lte' in path.read_text()]
    if leftovers:
        p.error('beyond2lte references remain in build files: ' + ', '.join(leftovers))

    configs = root / 'Resources/Configs'
    shutil.copyfile(configs / 'beyond2lte.toml', configs / 'beyond1lte.toml')
    (root / 'Resources/DTBs/beyond1lte.dtb').write_bytes(dtb)
    stale = root / 'Resources/DTBs/beyond1lte.dts'
    if stale.exists():
        stale.unlink()
    print(f'Prepared {target} from the beyond2lte overlay; DTB {len(dtb)} bytes')


if __name__ == '__main__':
    main()
