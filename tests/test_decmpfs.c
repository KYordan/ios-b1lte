/* Host test for xnu-overlay/bsd/kern/b2l_decmpfs_core.h: decode every HFS-compressed
 * file listed on stdin from its raw xattr / resource fork and compare with what macOS
 * returns from a plain read. Also exercises unaligned sub-range fetches for type 4.
 *   cc -O2 -o test_decmpfs tests/test_decmpfs.c -lz && find ROOT -flags +compressed -type f | ./test_decmpfs
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>
#include <zlib.h>
#include "../xnu-overlay/bsd/kern/b2l_decmpfs_core.h"

static void *za(void *o, unsigned n, unsigned s) { (void)o; return calloc(n, s); }
static void zf(void *o, void *p) { (void)o; free(p); }
struct rs { const char *path; };
static int rd(void *c, uint64_t off, void *buf, size_t len)
{
	ssize_t n = getxattr(((struct rs *)c)->path, XATTR_RESOURCEFORK_NAME, buf, len, (uint32_t)off, XATTR_SHOWCOMPRESSION);
	return n == (ssize_t)len ? 0 : 5;
}
static uint8_t *slurp(const char *p, size_t *n)
{
	FILE *f = fopen(p, "rb"); uint8_t *b; long sz;
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
	b = malloc(sz ? sz : 1); *n = fread(b, 1, sz, f); fclose(f); return b;
}

/* Synthetic 3-block resource fork: zlib, stored (0xFF), zlib tail of 1000 bytes. */
static uint8_t synfork[400000];
static int synrd(void *c, uint64_t off, void *buf, size_t len)
{
	(void)c; if (off + len > sizeof synfork) return 5; memcpy(buf, synfork + off, len); return 0;
}
static int synthetic(uint8_t *work)
{
	enum { U = 2 * 65536 + 1000 };
	static uint8_t data[U], out[U];
	uint64_t done; uint32_t pos, base = 0x100 + 4, nb = 3, tab = base + 4 + 8 * nb, i;
	for (i = 0; i < U; i++) data[i] = (uint8_t)((i * 2654435761u) >> 13) & (i < 65536 ? 7 : 255);
	synfork[0] = 0; synfork[1] = 0; synfork[2] = 1; synfork[3] = 0;      /* data offset 0x100, BE */
	synfork[base] = nb; pos = tab - base;
	for (i = 0; i < nb; i++) {
		uint32_t ulen = i < 2 ? 65536 : 1000; uLongf clen = sizeof synfork - base - pos - 1;
		if (i == 1) { synfork[base + pos] = 0xff; memcpy(synfork + base + pos + 1, data + 65536, ulen); clen = ulen + 1; }
		else if (compress2(synfork + base + pos, &clen, data + i * 65536, ulen, 9) != Z_OK) return 1;
		memcpy(synfork + base + 4 + 8 * i, &pos, 4); memcpy(synfork + base + 8 + 8 * i, &clen, 4);
		pos += clen;
	}
	if (b2l_cmp_rsrc_fetch(synrd, 0, U, 0, U, out, work, &done, za, zf) || done != U || memcmp(out, data, U)) return 2;
	if (b2l_cmp_rsrc_fetch(synrd, 0, U, 65000, 70000, out, work, &done, za, zf) || done != U - 65000 ||
	    memcmp(out, data + 65000, done)) return 3;                               /* clipped at EOF, 3 blocks */
	if (b2l_cmp_rsrc_fetch(synrd, 0, U, U, 10, out, work, &done, za, zf) || done) return 4;
	return 0;
}
int main(void)
{
	static uint8_t x[1 << 16], work[2 * B2L_CMP_BLOCK_MAX];
	char path[4096];
	int syn = synthetic(work);
	printf("synthetic=%d\n", syn);
	if (syn) return 1;
	int ok = 0, bad = 0, sub = 0;
	while (fgets(path, sizeof path, stdin)) {
		path[strcspn(path, "\n")] = 0;
		ssize_t n = getxattr(path, "com.apple.decmpfs", x, sizeof x, 0, XATTR_SHOWCOMPRESSION);
		if (n < 16) continue;
		uint32_t type = b2l_cmp_le32(x + 4);
		uint64_t usize = b2l_cmp_le32(x + 8) | (uint64_t)b2l_cmp_le32(x + 12) << 32;
		size_t rn; uint8_t *real = slurp(path, &rn), *out = malloc(usize + 1);
		int err; uint64_t done = 0;
		struct rs c = { path };
		if (type == 4) err = b2l_cmp_rsrc_fetch(rd, &c, usize, 0, usize, out, work, &done, za, zf);
		else { err = b2l_cmp_xattr_decode(type, x + 16, n - 16, out, usize, za, zf); done = usize; }
		if (err || done != usize || rn != usize || memcmp(out, real, usize)) {
			printf("BAD type %u err %d %s\n", type, err, path); bad++;
		} else ok++;
		if (type == 4 && usize > 3000 && !err) {        /* unaligned range; straddles a block boundary when > 64 KiB */
			uint64_t off = usize > 70000 ? 65536 - 123 : usize / 3 + 1, len = usize > 70000 ? 5000 : usize / 3;
			if (b2l_cmp_rsrc_fetch(rd, &c, usize, off, len, out, work, &done, za, zf) || done != len ||
			    memcmp(out, real + off, len)) { printf("BAD subrange %s\n", path); bad++; } else sub++;
		}
		free(real); free(out);
	}
	printf("ok=%d bad=%d subrange_ok=%d\n", ok, bad, sub);
	return bad != 0;
}
