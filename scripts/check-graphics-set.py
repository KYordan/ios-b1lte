#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Check a built graphics set against what the kernel publishes from SYSTEM.

The kernel copies SYSTEM:/<set>/<file> to the root once, before launchd (b2l_gl_publish and
b2l_metal_publish in xnu-overlay/bsd/kern/b2l_userspace_seed.h); a missing file stops the
publication of everything after it (boot logs: "B2L METAL: publication error=2"). This script
reads the file list from that header, so the CI artifact and the kernel cannot drift apart.

  check-graphics-set.py --list            the SYSTEM paths the kernel expects
  check-graphics-set.py DIR               DIR holds b2l-metal-v5/, b2l-metal-v95/, b2l-gl-v9/ ...;
                                          every expected file present, non-empty, Mach-O; on macOS
                                          also each dylib/bundle's dependencies (otool -L) are system
                                          libraries or files of the set at their published path.
"""
import argparse, re, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SEED = ROOT / 'xnu-overlay/bsd/kern/b2l_userspace_seed.h'
MACHO = {b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'}


def expected(seed=SEED):
    text = seed.read_text()
    out = {}   # SYSTEM path -> published path
    for src, final in re.findall(r'b2l_gl_publish_one\(\s*"([^"%]+)"\s*,\s*"([^"]+)"', text):
        out[src] = final
    # loop over a name array: snprintf(src, ..., "<set>/%s", arr[i]) and snprintf(final, ..., "<dir>/%s", arr[i])
    for arr, body in re.findall(r'static const char \*const (\w+)\[\]\s*=\s*\{([^}]*)\}', text):
        names = re.findall(r'"([^"]+)"', body)
        src = re.search(r'snprintf\(src,[^"]*"([^"%]+)/%s",\s*' + arr + r'\[', text)
        final = re.search(r'snprintf\(final,[^"]*"([^"%]+)/%s",\s*' + arr + r'\[', text)
        if src and final:
            for n in names:
                out[f'{src.group(1)}/{n}'] = f'{final.group(1)}/{n}'
    if not out:
        sys.exit(f'{seed}: no published files found')
    return out


def deps(path):
    r = subprocess.run(['otool', '-L', str(path)], capture_output=True, text=True, check=True)
    lines = r.stdout.splitlines()[1:]
    return [l.strip().split(' (')[0] for l in lines if l.strip()]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('dir', nargs='?', type=Path)
    p.add_argument('--list', action='store_true')
    a = p.parse_args()
    files = expected()
    if a.list or a.dir is None:
        for src, final in files.items():
            print(f'{src} -> {final}')
        return
    published = set(files.values())
    errors = []
    for src in files:
        f = a.dir / src
        if not f.is_file() or f.stat().st_size == 0:
            errors.append(f'missing or empty: {src}')
        elif not src.endswith('.plist') and f.read_bytes()[:4] not in MACHO:
            errors.append(f'not Mach-O: {src}')
    if not errors and shutil.which('otool'):
        for src, final in files.items():
            if src.endswith('.plist'):
                continue
            for d in deps(a.dir / src):
                if d == final:
                    continue   # the dylib's own install name
                if d.startswith(('/System/Library/', '/usr/lib/')) or d in published:
                    continue
                errors.append(f'{src}: depends on {d}, which is neither a system library nor published')
    for e in errors:
        print('check-graphics-set:', e, file=sys.stderr)
    if errors:
        sys.exit(1)
    total = sum((a.dir / s).stat().st_size for s in files)
    print(f'check-graphics-set: {len(files)} files, {total} bytes, as the kernel publishes them'
          + ('' if shutil.which('otool') else ' (dependencies not checked: no otool)'))


if __name__ == '__main__':
    main()
