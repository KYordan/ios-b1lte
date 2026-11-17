# tv3.py FILE... -- print the decompressed bytes of each tracev3 chunkset (tag 0x600d, LZ4 bv41) to stdout
import ctypes, struct, sys
lib = ctypes.CDLL('/usr/lib/libcompression.dylib')
lib.compression_decode_buffer.restype = ctypes.c_size_t
for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    o = 0
    while o + 16 <= len(d):
        tag, sub, size = struct.unpack_from('<IIQ', d, o)
        body = d[o + 16:o + 16 + size]
        if tag == 0x600d and body[:4] == b'bv41':
            need = struct.unpack_from('<I', body, 4)[0] + 64
            dst = ctypes.create_string_buffer(need)
            n = lib.compression_decode_buffer(dst, need, body, len(body), None, 0x100)
            sys.stdout.buffer.write(dst.raw[:n])
        o += 16 + size
        o = (o + 7) & ~7
