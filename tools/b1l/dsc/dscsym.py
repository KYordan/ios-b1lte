#!/usr/bin/env python3
# dscsym.py DIR pc... : find the shared cache slide from a backtrace (return addresses follow a BL/BLR), then name the
# image (and, from LC_SYMTAB of that image if found, the nearest symbol) of each pc.
import glob, mmap, os, struct, sys

d = sys.argv[1]
pcs = [int(x, 16) for x in sys.argv[2:]]
files = sorted(f for f in glob.glob(os.path.join(d, 'dyld_shared_cache_arm64e*'))
               if not f.endswith('.symbols') and not f.endswith('.map') and '.dylddata' not in f)
maps = []  # (addr, size, fileoff, mm)
mms = {}
main = None
for f in files:
    fh = open(f, 'rb')
    mm = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
    mms[f] = mm
    if mm[:7] != b'dyld_v1':
        continue
    off, cnt = struct.unpack_from('<II', mm, 0x10)
    for i in range(cnt):
        a, sz, fo, mp, ip = struct.unpack_from('<QQQII', mm, off + 32 * i)
        maps.append((a, sz, fo, mm, mp))
    if f.endswith('arm64e'):
        main = mm

def word(va):
    for a, sz, fo, mm, mp in maps:
        if a <= va < a + sz and (mp & 4):
            o = fo + va - a
            if o + 4 <= len(mm):
                return struct.unpack_from('<I', mm, o)[0]
    return None

def is_call(w):
    return w is not None and ((w & 0xfc000000) == 0x94000000 or (w & 0xfffffc1f) == 0xd63f0000 or
                              (w & 0xfefff800) == 0xd63f0800)

frames = pcs[1:]
best = []
for s in range(0, 0x40000000, 0x4000):
    ok = sum(1 for p in frames if 0x180000000 <= p - s < 0x300000000 and is_call(word(p - 4 - s)))
    if ok == len(frames):
        best.append(s)
        if len(best) > 3:
            break
print('slide candidates:', [hex(x) for x in best])
if not best:
    sys.exit()
slide = best[0]
# image table (imagesText: uuid16, addr8, size4, path4) at header 0x88/0x90
at, n = struct.unpack_from('<QQ', main, 0x88)
imgs = []
for i in range(n):
    e = main[at + 32 * i: at + 32 * i + 32]
    a = struct.unpack_from('<Q', e, 16)[0]; sz = struct.unpack_from('<I', e, 24)[0]; po = struct.unpack_from('<I', e, 28)[0]
    path = main[po:main.find(b'\0', po)].decode()
    imgs.append((a, sz, path))
for p in pcs:
    u = p - slide
    name = '?'
    for a, sz, path in imgs:
        if a <= u < a + sz:
            name = f'{os.path.basename(path)}+{u - a:#x}'
            break
    print(f'{p:#x} -> {u:#x} {name}')
