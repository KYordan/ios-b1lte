#!/usr/bin/env python3
# iomfbexp.py DIR: the exported functions of IOMobileFramebuffer.framework in the cache and the selector each passes
# to IOConnectCall* (`mov w1, #N`), from its export trie and a disassembly of its first instructions.
import os, re, struct, subprocess, sys
DIR = sys.argv[1]
os.chdir(DIR)
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'rd.py')).read())
main = [mm for a, sz, fo, mm in M if mm[:7] == b'dyld_v1'][0]
for f in sorted(glob.glob('dyld_shared_cache_arm64e')):
    pass
mfile = open('dyld_shared_cache_arm64e', 'rb').read(0x400)
at, n = struct.unpack_from('<QQ', mfile, 0x88)
hdr = open('dyld_shared_cache_arm64e', 'rb')
hdr.seek(at)
tab = hdr.read(32 * n)
base = None
for i in range(n):
    e = tab[32 * i: 32 * i + 32]
    a = struct.unpack_from('<Q', e, 16)[0]
    po = struct.unpack_from('<I', e, 28)[0]
    hdr.seek(po)
    path = hdr.read(200).split(b'\0')[0].decode()
    if path.endswith('IOMobileFramebuffer.framework/IOMobileFramebuffer'):
        base = a
        break
print('image', hex(base))
mh = rd(base, 0x4000)
ncmds = struct.unpack_from('<I', mh, 16)[0]
off = 32
le = None
trie = None
for _ in range(ncmds):
    cmd, sz = struct.unpack_from('<II', mh, off)
    if cmd == 0x19:
        name = mh[off + 8: off + 24].split(b'\0')[0]
        vm, vs, fo, fs = struct.unpack_from('<QQQQ', mh, off + 24)
        if name == b'__LINKEDIT':
            le = (vm, fo)
    if cmd == 0x80000033:
        trie = struct.unpack_from('<II', mh, off + 8)
    if cmd == 0x80000022:
        trie = struct.unpack_from('<II', mh, off + 40)
    off += sz
print('linkedit', [hex(x) for x in le], 'trie', trie)
tva = le[0] + (trie[0] - le[1])
data = rd(tva, trie[1])

def uleb(b, i):
    r = s = 0
    while True:
        x = b[i]; i += 1
        r |= (x & 0x7f) << s; s += 7
        if x < 0x80:
            return r, i

out = {}
def walk(node, prefix):
    tsize, i = uleb(data, node)
    if tsize:
        flags, j = uleb(data, i)
        addr, j = uleb(data, j)
        out[prefix] = addr
    i += tsize
    nch = data[i]; i += 1
    for _ in range(nch):
        e = data.index(b'\0', i)
        s = data[i:e].decode(); i = e + 1
        child, i = uleb(data, i)
        walk(child, prefix + s)
walk(0, '')
mc = os.path.join(os.environ.get('PY_ENV', os.path.expanduser('~/Developer/tools/mamba/envs/s10')), 'bin/llvm-mc')
for name, a in sorted(out.items(), key=lambda x: x[1]):
    b = rd(base + a, 4 * 64)
    if not b:
        continue
    hexs = ' '.join('0x%02x' % x for x in b)
    txt = subprocess.run([mc, '--disassemble', '-triple=arm64e-apple-ios'], input=hexs, capture_output=True,
                         text=True).stdout
    sels = re.findall(r'mov\s+w1, #(\d+)', txt)
    print(f'{name}: {",".join(sels[:3]) if sels else "-"}')
