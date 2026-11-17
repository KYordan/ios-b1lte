#!/usr/bin/env python3
# B1L-0087: rebuild the AIR modules backboardd's Metal driver sent through the kernel log (libhakiosair,
# b2l_dump_module): lines "B2LMOD <kind>-<name> <i>/<n> <fnv32> <base64 chunk>", each module sent twice. A chunk is
# kept when its FNV-1a checksum matches (the log can interleave lines from several cores); a module is written when
# every chunk has a good copy.
#   python3 extract-b2lmod.py LOG OUTDIR
import base64
import os
import re
import sys

log, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
pat = re.compile(rb'B2LMOD (\S+) (\d+)/(\d+) ([0-9a-f]{8}) ([A-Za-z0-9+/=]+)')
mods = {}


def fnv(b):
    h = 2166136261
    for x in b:
        h = ((h ^ x) * 16777619) & 0xffffffff
    return h


for m in pat.finditer(open(log, 'rb').read()):
    name, i, n, h, chunk = m.group(1).decode(), int(m.group(2)), int(m.group(3)), int(m.group(4), 16), m.group(5)
    if fnv(chunk) != h:
        continue
    mods.setdefault(name, [n, {}])[1].setdefault(i, chunk)
ok = bad = 0
for name, (n, chunks) in sorted(mods.items()):
    missing = [i for i in range(n) if i not in chunks]
    if missing:
        print(f'{name}: {len(missing)} of {n} chunks missing ({missing[:8]}...)')
        bad += 1
        continue
    data = base64.b64decode(b''.join(chunks[i] for i in range(n)))
    open(os.path.join(out, name + '.air'), 'wb').write(data)
    print(f'{name}: {len(data)} bytes')
    ok += 1
print(f'{ok} modules rebuilt, {bad} incomplete')
