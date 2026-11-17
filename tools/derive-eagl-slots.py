#!/usr/bin/env python3
"""Derive the EAGL dispatch-table offsets of an OpenGLES binary (iOS 26) from its gl* stubs:
   mrs x8, TPIDRRO_EL0; ldr x8, [x8, #0xf0]; ...; ldr xN, [x8, #OFF]; ldr x0, [x8, #0x20]; braaz xN
The table base is the context + 0x20 (what the stub passes as the first argument), so a slot is OFF - 0x20,
which is how HakiOS's eagl_slots.txt (iOS 16.5) counts. Output: 'offset name' per line.
  derive-eagl-slots.py OPENGLES > slots-ios26.txt"""
import re, subprocess, sys
out = subprocess.run(['otool', '-tV', sys.argv[1]], capture_output=True, text=True, check=True).stdout
cur, loads, slots, base = None, [], {}, None
for line in out.splitlines():
    m = re.match(r'^(_\w+):$', line)
    if m:
        cur, loads, base = m.group(1)[1:], [], None
        continue
    if cur is None or not cur.startswith('gl'):
        continue
    m = re.search(r'ldr\s+x(\d+), \[x\1, #0xf0\]', line)
    if m and base is None:
        base = int(m.group(1))
        continue
    m = re.search(r'ldr\s+x(\d+), \[x(\d+), #0x([0-9a-f]+)\]', line)
    if m and base is not None and int(m.group(2)) == base:
        loads.append((int(m.group(1)), int(m.group(3), 16)))
    m = re.search(r'\b(?:blraaz|blraa|braaz|braa|blr|br)\s+x(\d+)', line)
    if m:
        reg = int(m.group(1))
        offs = [o for r, o in loads if r == reg]
        if offs and cur not in slots:
            slots[cur] = offs[-1] - 0x20
for name, off in sorted(slots.items(), key=lambda x: (x[1], x[0])):
    print('0x%04x %s' % (off, name))
