#!/usr/bin/env python3
"""Flatten a binary FDT into {"/path": {"prop": "hex"}} JSON, like
beyond2lte-ios/common/memory/beyond2lte-8g-dt.json. Usage: fdt2json.py IN.dtb OUT.json"""
import json, struct, sys

data = open(sys.argv[1], 'rb').read()
magic, total, off_struct, off_strings = struct.unpack_from('>IIII', data, 0)
if magic != 0xd00dfeed:
    sys.exit('not an FDT')
strings = data[off_strings:]

def name_at(off):
    return strings[off:strings.index(b'\0', off)].decode()

tree, path, pos = {}, [], off_struct
while True:
    token, = struct.unpack_from('>I', data, pos); pos += 4
    if token == 1:  # BEGIN_NODE
        end = data.index(b'\0', pos)
        name = data[pos:end].decode(); pos = (end + 4) & ~3
        path.append(name)
        tree.setdefault('/' + '/'.join(p for p in path if p), {})
    elif token == 2:  # END_NODE
        path.pop()
    elif token == 3:  # PROP
        length, nameoff = struct.unpack_from('>II', data, pos); pos += 8
        value = data[pos:pos + length]; pos = (pos + length + 3) & ~3
        tree['/' + '/'.join(p for p in path if p)][name_at(nameoff)] = value.hex()
    elif token == 4:  # NOP
        continue
    elif token == 9:  # END
        break
    else:
        sys.exit('bad token %d at %d' % (token, pos))
json.dump(tree, open(sys.argv[2], 'w'), indent=1, sort_keys=True)
print(len(tree), 'nodes')
