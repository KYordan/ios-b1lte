#!/usr/bin/env python3
# dscname.py DIR unslid_va... : nearest local symbol (from dyld_shared_cache_arm64e.symbols) at or below each address.
import bisect, mmap, os, struct, sys

d = sys.argv[1]
want = [int(x, 16) for x in sys.argv[2:]]
f = open(os.path.join(d, 'dyld_shared_cache_arm64e.symbols'), 'rb')
mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
lso, lss = struct.unpack_from('<QQ', mm, 0x48)
nlo, nlc, sto, sts, eno, enc = struct.unpack_from('<IIIIII', mm, lso)
lo, hi = min(want) - 0x200000, max(want) + 0x10
best = {w: (0, None) for w in want}
base = lso + nlo
strs = lso + sto
for i in range(nlc):
    strx, ntype, nsect, ndesc, nval = struct.unpack_from('<IBBHQ', mm, base + 16 * i)
    if not (lo <= nval <= hi) or (ntype & 0xe0):
        continue
    for w in want:
        if nval <= w and nval > best[w][0]:
            best[w] = (nval, strx)
for w in want:
    v, s = best[w]
    name = mm[strs + s: mm.find(b'\0', strs + s)].decode(errors='replace') if s is not None else '?'
    print(f'{w:#x}: {name} + {w - v:#x}')
