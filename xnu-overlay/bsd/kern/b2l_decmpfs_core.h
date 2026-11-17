/* SPDX-License-Identifier: BSD-2-Clause
 * HFS+ transparent-compression decoders that iOS normally gets from the
 * AppleFSCompression kext, which this kernel does not have. Without them every
 * read of a compressed file fails ("tried to access a compressed file of
 * unregistered type 4/9" in decmpfs.c) and installd/LaunchServices see empty
 * Info.plists, so the 50 staged system apps never install.
 *
 * Formats (checked against all 3343 compressed files of the S10 root):
 *   type 3  zlib, in the com.apple.decmpfs xattr after its 16-byte header;
 *           first byte with low nibble 0xF = stored, rest is the data.
 *   type 4  zlib, in the resource fork: BE u32 data offset at 0; at
 *           data+4 a LE u32 block count then {LE u32 offset, LE u32 size}
 *           per 64 KiB block (offsets from data+4); a block starting
 *           with 0xFF is stored, else a zlib stream.
 *   type 9  stored data in the xattr after a 0xCC marker byte (the only form
 *           present; any other marker is LZFSE and is refused, EINVAL).
 * Pure C: compiled into the kernel (decmpfs.c) and into tests/test_decmpfs.c.
 */
#ifndef B2L_DECMPFS_CORE_H
#define B2L_DECMPFS_CORE_H

#define B2L_CMP_ZLIB_XATTR   3u
#define B2L_CMP_ZLIB_RSRC    4u
#define B2L_CMP_STORED_XATTR 9u
#define B2L_CMP_BLOCK        65536u
#define B2L_CMP_BLOCK_MAX    (B2L_CMP_BLOCK + 1024u) /* a stored or incompressible block */

static inline uint32_t b2l_cmp_le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint32_t b2l_cmp_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

/* Inflate one zlib stream (or a stored chunk) into out[0..cap). *outn = bytes produced. */
static int
b2l_cmp_inflate(const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *outn,
    void *(*za)(void *, unsigned, unsigned), void (*zf)(void *, void *))
{
	z_stream zs;
	int zr;

	*outn = 0;
	if (n == 0) {
		return 22;
	}
	if ((in[0] & 0x0f) == 0x0f) {           /* stored */
		if (n - 1 > cap) {
			return 22;
		}
		memcpy(out, in + 1, n - 1);
		*outn = n - 1;
		return 0;
	}
	memset(&zs, 0, sizeof(zs));
	zs.zalloc = (alloc_func)za;
	zs.zfree = (free_func)zf;
	if (inflateInit(&zs) != Z_OK) {
		return 12;
	}
	zs.next_in = (Bytef *)(uintptr_t)in;
	zs.avail_in = (uInt)n;
	zs.next_out = out;
	zs.avail_out = (uInt)cap;
	zr = inflate(&zs, Z_FINISH);
	*outn = cap - zs.avail_out;
	inflateEnd(&zs);
	return (zr == Z_STREAM_END) ? 0 : 5;
}

/* Whole-file decode of an xattr-resident type (3, 9). payload = attr bytes after the header. */
static int
b2l_cmp_xattr_decode(uint32_t type, const uint8_t *payload, size_t n, uint8_t *out, uint64_t usize,
    void *(*za)(void *, unsigned, unsigned), void (*zf)(void *, void *))
{
	size_t got = 0;
	int err;

	if (type == B2L_CMP_STORED_XATTR) {
		if (n < 1 || payload[0] != 0xcc || n - 1 != usize) {
			return 22;
		}
		memcpy(out, payload + 1, n - 1);
		return 0;
	}
	if (type != B2L_CMP_ZLIB_XATTR) {
		return 45;
	}
	err = b2l_cmp_inflate(payload, n, out, (size_t)usize, &got, za, zf);
	if (err == 0 && got != usize) {
		err = 5;
	}
	return err;
}

/* Resource-fork reader: read len bytes at off, 0 or errno. */
typedef int (*b2l_cmp_read_fn)(void *ctx, uint64_t off, void *buf, size_t len);

/*
 * Type 4: write uncompressed bytes [offset, offset+size) of a file of usize bytes
 * to out (contiguous). work must hold 2 * B2L_CMP_BLOCK_MAX bytes. *done = bytes written.
 */
static int
b2l_cmp_rsrc_fetch(b2l_cmp_read_fn rd, void *rctx, uint64_t usize, uint64_t offset, uint64_t size,
    uint8_t *out, uint8_t *work, uint64_t *done,
    void *(*za)(void *, unsigned, unsigned), void (*zf)(void *, void *))
{
	uint8_t h[8], ent[8];
	uint8_t *cbuf = work, *ubuf = work + B2L_CMP_BLOCK_MAX;
	uint64_t base, end, pos;
	uint32_t count, want;
	int err;

	*done = 0;
	if (offset >= usize) {
		return 0;
	}
	end = offset + size;
	if (end > usize) {
		end = usize;
	}
	if ((err = rd(rctx, 0, h, 4)) != 0) {
		return err;
	}
	base = (uint64_t)b2l_cmp_be32(h) + 4;   /* table starts at data offset + 4 */
	if ((err = rd(rctx, base, h, 4)) != 0) {
		return err;
	}
	count = b2l_cmp_le32(h);
	want = (uint32_t)((usize + B2L_CMP_BLOCK - 1) / B2L_CMP_BLOCK);
	if (count != want) {
		return 22;
	}
	for (pos = offset; pos < end;) {
		uint32_t b = (uint32_t)(pos / B2L_CMP_BLOCK), coff, clen;
		uint64_t bstart = (uint64_t)b * B2L_CMP_BLOCK, blen = usize - bstart, skip, take;
		size_t got = 0;

		if (blen > B2L_CMP_BLOCK) {
			blen = B2L_CMP_BLOCK;
		}
		if ((err = rd(rctx, base + 4 + 8ull * b, ent, 8)) != 0) {
			return err;
		}
		coff = b2l_cmp_le32(ent);
		clen = b2l_cmp_le32(ent + 4);
		if (clen == 0 || clen > B2L_CMP_BLOCK_MAX) {
			return 22;
		}
		if ((err = rd(rctx, base + coff, cbuf, clen)) != 0) {
			return err;
		}
		if ((err = b2l_cmp_inflate(cbuf, clen, ubuf, B2L_CMP_BLOCK, &got, za, zf)) != 0) {
			return err;
		}
		if (got != blen) {
			return 5;
		}
		skip = pos - bstart;
		take = blen - skip;
		if (take > end - pos) {
			take = end - pos;
		}
		memcpy(out + (pos - offset), ubuf + skip, (size_t)take);
		pos += take;
		*done += take;
	}
	return 0;
}
#endif
