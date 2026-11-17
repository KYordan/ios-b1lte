#!/usr/bin/env python3
# hash-image.py IMAGE [BLOCK...]: one "index sha256" line per 4 MiB block of a local image, in order, 6 processes.
# With BLOCK numbers, only those blocks. Read-only.
import hashlib, os, sys
from multiprocessing import Pool

BS = 4 * 1024 * 1024
img = sys.argv[1]

def h(i):
    fd = os.open(img, os.O_RDONLY)
    try:
        return i, hashlib.sha256(os.pread(fd, BS, i * BS)).hexdigest()
    finally:
        os.close(fd)

if __name__ == "__main__":
    blocks = [int(b) for b in sys.argv[2:]] or range(os.path.getsize(img) // BS)
    with Pool(6) as p:
        for i, d in p.imap(h, blocks, chunksize=16):
            print(i, d)
