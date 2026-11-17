#!/usr/bin/env python3
# B1L-0078: turn the authenticated branches of one of OUR arm64e Mach-O images into plain ones, in every section that
# holds instructions (__auth_stubs, __objc_stubs, __text...):
#   BRAA/BRAB/BRAAZ/BRABZ Xn[, Xm]   -> BR Xn
#   BLRAA/BLRAB/BLRAAZ/BLRABZ Xn[, Xm] -> BLR Xn
#   RETAA/RETAB                      -> RET
#   LDAPR/LDAPRB/LDAPRH Rt, [Xn]     -> LDAR/LDARB/LDARH (B1L-0102: ARMv8.3 RCpc, which the Exynos M4 lacks:
#                                       Mesa's atomics in libhakiosair trapped ~700 000 times a boot in backboardd;
#                                       LDAR orders at least as strongly)
# Why: the Exynos 9820 has no PAC, so each of these is a trap the kernel emulates (~11 us). B1L-0077 measured 48 a
# Metal draw, every one of them a BRAA in the linker's stubs (__auth_stubs to libsystem, __objc_stubs to
# objc_msgSend). This kernel's arm64e emulation is the stripped-pointer ABI (b2l_arm64e_compat.c): no pointer is ever
# really signed, so a BRAA through a GOT entry and a BR through it go to the same place.
# Only for images we build; re-sign them afterwards (codesign --force --sign -).
#   python3 unsign-branches.py FILE...
import struct
import sys

MH_MAGIC_64 = 0xfeedfacf
LC_SEGMENT_64 = 0x19
S_ATTR_PURE_INSTRUCTIONS = 0x80000000
S_ATTR_SOME_INSTRUCTIONS = 0x00000400


def patch(path):
    data = bytearray(open(path, 'rb').read())
    magic, cputype, _, _, ncmds, _, _, _ = struct.unpack_from('<IiiIIIII', data, 0)
    if magic != MH_MAGIC_64 or cputype != 0x0100000c:
        sys.exit(f'{path}: not a thin arm64 Mach-O')
    off = 32
    counts = {}
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, off)
        if cmd == LC_SEGMENT_64:
            nsects = struct.unpack_from('<I', data, off + 64)[0]
            s = off + 72
            for _ in range(nsects):
                sect = data[s:s + 16].split(b'\0')[0].decode()
                seg = data[s + 16:s + 32].split(b'\0')[0].decode()
                ssize, foff = struct.unpack_from('<QI', data, s + 40)
                flags = struct.unpack_from('<I', data, s + 64)[0]
                if flags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS) and foff:
                    n = 0
                    for p in range(foff, foff + ssize - 3, 4):
                        i = struct.unpack_from('<I', data, p)[0]
                        new = None
                        if (i & 0xfefff800) == 0xd61f0800:      # BRAA, BRAB, BRAAZ, BRABZ
                            new = 0xd61f0000 | (i & 0x3e0)
                        elif (i & 0xfefff800) == 0xd63f0800:    # BLRAA, BLRAB, BLRAAZ, BLRABZ
                            new = 0xd63f0000 | (i & 0x3e0)
                        elif i in (0xd65f0bff, 0xd65f0fff):     # RETAA, RETAB
                            new = 0xd65f03c0
                        elif (i & 0x3ffffc00) == 0x38bfc000:    # LDAPR(B/H) -> LDAR(B/H), B1L-0102
                            new = (i & 0xc00003ff) | 0x08dffc00
                        if new is not None:
                            struct.pack_into('<I', data, p, new)
                            n += 1
                    if n:
                        counts[f'{seg},{sect}'] = n
                s += 80
        off += size
    open(path, 'wb').write(data)
    print(f'unsign-branches: {path.split("/")[-1]}: ' +
          (', '.join(f'{k} {v}' for k, v in counts.items()) or 'nothing to change'))


for f in sys.argv[1:]:
    patch(f)
