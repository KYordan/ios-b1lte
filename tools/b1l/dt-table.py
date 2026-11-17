#!/usr/bin/env python3
"""List or extract entries of an Android DT table image (dtb.img / dtbo.img).
  dt-table.py IMG                 list entries (index, size, id, rev, custom)
  dt-table.py IMG INDEX OUT.dtb   extract one entry"""
import struct, sys

data = open(sys.argv[1], 'rb').read()
magic, total, hdr, esize, count, eoff, page, ver = struct.unpack_from('>8I', data, 0)
if magic != 0xd7b7ab1e:
    sys.exit('not a DT table image')
entries = []
for i in range(count):
    size, off, ident, rev, *custom = struct.unpack_from('>8I', data, eoff + i * esize)
    entries.append((size, off, ident, rev, custom))
if len(sys.argv) == 2:
    print(f'version {ver}, {count} entries, page {page}')
    for i, (size, off, ident, rev, custom) in enumerate(entries):
        print(i, f'size={size}', f'id=0x{ident:x}', f'rev=0x{rev:x} ({rev})', 'custom=' + ','.join(hex(c) for c in custom))
else:
    size, off, *_ = entries[int(sys.argv[2])]
    blob = data[off:off + size]
    if struct.unpack_from('>I', blob)[0] != 0xd00dfeed:
        sys.exit('entry is not an FDT')
    open(sys.argv[3], 'wb').write(blob)
    print('wrote', sys.argv[3], size, 'bytes')
