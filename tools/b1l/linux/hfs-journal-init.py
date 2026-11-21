#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Finish a Linux-made journaled HFS+ image the way macOS newfs_hfs leaves one.

hfsprogs' mkfs.hfsplus -J writes the journal info block with kJIJournalNeedInitMask and an
all-zero journal header, expecting macOS to create the journal on first mount. The B2L
firmware (common/cache/CacheHfs.c, b2l_hfs_journal_empty) accepts only an initialized, empty
journal and a JIB whose flags are exactly kJIJournalInFSMask. This writes the journal header
as XNU's journal_create does (little-endian, start == end == jhdr_size, calc_checksum over the
first 44 bytes), clears NeedInit, and copies the primary volume header to the alternate one
(Linux leaves the alternate stale). The image must not be mounted.

  hfs-journal-init.py IMAGE [--jhdr-size 4096]
"""
import argparse, os, struct, sys

JNL_MAGIC, ENDIAN_MAGIC = 0x4a4e4c78, 0x12345678
IN_FS, NEED_INIT = 0x1, 0x4
VOL_UNMOUNTED, VOL_INCONSISTENT, VOL_JOURNALED = 1 << 8, 1 << 11, 1 << 13


def checksum(data):
    s = 0
    for b in data:
        s = ((s << 8) ^ (s + b)) & 0xffffffff
    return ~s & 0xffffffff


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image')
    p.add_argument('--jhdr-size', type=int, default=4096, help='journal header block (UFS sector: 4096)')
    a = p.parse_args()
    with open(a.image, 'r+b') as f:
        f.seek(1024); vh = bytearray(f.read(512))
        sig, version = vh[:2], struct.unpack_from('>H', vh, 2)[0]
        if (sig, version) not in ((b'H+', 4), (b'HX', 5)):
            sys.exit('not an HFS+/HFSX volume')
        attrs, = struct.unpack_from('>I', vh, 4)
        jib, = struct.unpack_from('>I', vh, 12)
        bs, blocks = struct.unpack_from('>II', vh, 40)
        if not attrs & VOL_JOURNALED or not jib:
            sys.exit('volume is not journaled')
        if not attrs & VOL_UNMOUNTED or attrs & VOL_INCONSISTENT:
            sys.exit('volume was not cleanly unmounted; run fsck.hfsplus first')
        f.seek(jib * bs); ji = bytearray(f.read(52))
        flags, = struct.unpack_from('>I', ji, 0)
        off, size = struct.unpack_from('>QQ', ji, 36)
        if not flags & IN_FS or off % a.jhdr_size or size < 4 * a.jhdr_size or off + size > bs * blocks:
            sys.exit(f'unexpected journal info block: flags={flags:#x} offset={off} size={size}')
        f.seek(off); old = f.read(64)
        if struct.unpack_from('<I', old)[0] == JNL_MAGIC and not flags & NEED_INIT:
            print('journal already initialized; nothing to do'); return
        tbuffer = 131072
        blhdr = max(a.jhdr_size, (tbuffer // a.jhdr_size) * 16)
        seq = int.from_bytes(os.urandom(3), 'little')
        hdr = bytearray(a.jhdr_size)
        struct.pack_into('<IIqqqiIiI', hdr, 0, JNL_MAGIC, ENDIAN_MAGIC, a.jhdr_size, a.jhdr_size,
                         size, blhdr, 0, a.jhdr_size, seq)
        struct.pack_into('<I', hdr, 36, checksum(hdr[:44]))
        f.seek(off); f.write(hdr)
        struct.pack_into('>I', ji, 0, flags & ~NEED_INIT)
        f.seek(jib * bs); f.write(ji)
        # Alternate volume header: 1024 bytes before the end of the volume, same content.
        f.seek(bs * blocks - 1024); f.write(vh)
        f.flush(); os.fsync(f.fileno())
    print(f'journal initialized: offset={off} size={size} jhdr_size={a.jhdr_size} blhdr_size={blhdr}; '
          f'JIB flags {flags:#x} -> {flags & ~NEED_INIT:#x}; alternate header synced')


if __name__ == '__main__':
    main()
