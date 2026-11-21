#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Make a live /sys/firmware/fdt safe to publish: zero the per-boot /chosen data.

S-Boot writes /chosen (bootargs with androidboot.serialno, ap_serial, em.did,
the initrd range) into the DTB it hands to the kernel, so a dump of the running
tree carries the phone's identifiers. S-Boot rewrites /chosen on every boot, so
the UEFI platform does not need them. Values are zero-filled in place: the FDT
layout and every other node stay byte-identical. The identifier values are then
searched for anywhere in the output, and the tool fails if one remains.

  sanitize-dtb.py LIVE.dtb OUT.dtb
"""
import re
import struct
import sys

CHOSEN_PROPS = {'bootargs', 'linux,initrd-start', 'linux,initrd-end'}
ID_KEYS = ('androidboot.serialno', 'androidboot.ap_serial', 'androidboot.em.did',
           'androidboot.sn.param.offset', 'android.nonce')


def props(data):
    """Yield (path, name, value_offset, length) for every property."""
    magic, _, off_struct, off_strings = struct.unpack_from('>IIII', data, 0)
    if magic != 0xd00dfeed:
        sys.exit('not a flattened device tree')
    strings = data[off_strings:]
    path, pos = [], off_struct
    while True:
        token, = struct.unpack_from('>I', data, pos); pos += 4
        if token == 1:
            end = data.index(b'\0', pos)
            path.append(data[pos:end].decode()); pos = (end + 4) & ~3
        elif token == 2:
            path.pop()
        elif token == 3:
            length, nameoff = struct.unpack_from('>II', data, pos); pos += 8
            name = strings[nameoff:strings.index(b'\0', nameoff)].decode()
            yield '/' + '/'.join(p for p in path if p), name, pos, length
            pos = (pos + length + 3) & ~3
        elif token == 4:
            continue
        elif token == 9:
            return
        else:
            sys.exit(f'bad FDT token {token}')


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = bytearray(open(sys.argv[1], 'rb').read())
    secrets = []
    for path, name, off, length in list(props(data)):
        if path != '/chosen' or name not in CHOSEN_PROPS:
            continue
        if name == 'bootargs':
            args = bytes(data[off:off + length]).rstrip(b'\0').decode('ascii', 'replace')
            for key in ID_KEYS:
                m = re.search(r'(?:^|\s)' + re.escape(key) + r'=(\S+)', args)
                if m and len(m.group(1)) >= 6:
                    secrets.append(m.group(1).encode())
        data[off:off + length] = bytes(length)
    for value in secrets:
        if value in data:
            sys.exit('an identifier from bootargs is still present elsewhere in the tree; not written')
    open(sys.argv[2], 'wb').write(data)
    print(f'zeroed /chosen {sorted(CHOSEN_PROPS)}; {len(secrets)} identifier values checked absent; {len(data)} bytes')


if __name__ == '__main__':
    main()
