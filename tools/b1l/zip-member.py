#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Locate one stored member of an IPSW (zip/zip64) on disk or behind an HTTP URL.

Only the central directory and the member's local header are read (HTTP Range
requests for a URL), so a CI job can then fetch a single image with
`curl -r START-END` instead of the whole 10 GiB IPSW.

  zip-member.py IPSW-or-URL --manifest-key OS    -> "NAME START SIZE"
  zip-member.py IPSW-or-URL --name 094-...dmg.aea  -> "NAME START SIZE"

START/SIZE are the raw member bytes (the member must be stored, not deflated).
"""
import argparse
import io
import plistlib
import struct
import sys
import urllib.request
import zipfile


class RangeFile(io.RawIOBase):
    """Seekable read-only view of an HTTP resource through Range requests."""

    def __init__(self, url):
        self.url, self.pos = url, 0
        req = urllib.request.Request(url, method='HEAD', headers={'User-Agent': 'b2l-zip-member'})
        with urllib.request.urlopen(req, timeout=60) as r:
            self.size = int(r.headers['Content-Length'])

    def readable(self): return True
    def seekable(self): return True
    def tell(self): return self.pos

    def seek(self, offset, whence=0):
        self.pos = {0: offset, 1: self.pos + offset, 2: self.size + offset}[whence]
        return self.pos

    def readinto(self, buf):
        if self.pos >= self.size or not len(buf):
            return 0
        end = min(self.pos + len(buf), self.size) - 1
        req = urllib.request.Request(self.url, headers={'Range': f'bytes={self.pos}-{end}',
                                                        'User-Agent': 'b2l-zip-member'})
        with urllib.request.urlopen(req, timeout=120) as r:
            data = r.read()
        buf[:len(data)] = data
        self.pos += len(data)
        return len(data)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source')
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument('--name')
    g.add_argument('--manifest-key', help='BuildManifest image key, e.g. OS')
    a = p.parse_args()
    raw = RangeFile(a.source) if a.source.startswith(('http://', 'https://')) else open(a.source, 'rb')
    f = io.BufferedReader(raw, 1 << 20) if isinstance(raw, RangeFile) else raw
    z = zipfile.ZipFile(f)
    name = a.name
    if a.manifest_key:
        manifest = plistlib.loads(z.read('BuildManifest.plist'))
        name = manifest['BuildIdentities'][0]['Manifest'][a.manifest_key]['Info']['Path']
    info = z.getinfo(name)
    if info.compress_type != zipfile.ZIP_STORED:
        sys.exit(f'{name} is compressed; fetch the whole IPSW instead')
    f.seek(info.header_offset)
    sig, = struct.unpack('<I', f.read(4))
    if sig != 0x04034b50:
        sys.exit('bad local header')
    f.seek(info.header_offset + 26)
    name_len, extra_len = struct.unpack('<HH', f.read(4))
    start = info.header_offset + 30 + name_len + extra_len
    print(name, start, info.compress_size)


if __name__ == '__main__':
    main()
