/* SPDX-License-Identifier: BSD-2-Clause
 * Host analysis for static ARM64e -> plain ARM64 translation of the iOS root.
 * Uses the device's own conversion rule (b2l_nc_native) and corecrypto
 * exclusion (b2l_cache_exclusion.h), so offline output matches what the
 * on-device native cache would learn. Read-only: never writes its inputs.
 *
 *   nc-static scan MAIN_CACHE [SUBCACHE...]   count convertible words in executable mappings
 *   nc-static macho FILE...                   same for standalone Mach-O instruction sections
 *   nc-static compare ORIGINAL TRANSLATED     every differing word must equal b2l_nc_native(original)
 *   nc-static apply OUTDIR MAIN SUB...        translated copies of the cache in OUTDIR
 *   nc-static macho-apply FILE...             translate arm64e instruction sections IN PLACE
 *                                             (only for files of a disposable root copy)
 *   nc-static nop-call MAIN SUB... -- VA TARGET   replace "bl TARGET" at VA with NOP, in place
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "b2l_cache_core.h"
#include "b2l_cache_exclusion.h"

enum { C_NONE, C_PACAUT, C_HINT, C_RET, C_BRANCH, C_LDAPR, C_PACGA, C_COUNT };
static const char *const names[C_COUNT] = {"none", "pac/aut/xpac reg", "pac/aut hint", "retaa/retab", "braa/blraa", "ldapr", "pacga"};

static int classify(uint32_t i)
{
	uint32_t out;
	if (!b2l_nc_native(i, &out)) return C_NONE;
	if ((i & 0xffe00000u) == 0xdac00000u) return C_PACAUT;
	if ((i & 0xfffff01fu) == 0xd503201fu) return C_HINT;
	if (i == 0xd65f0bffu || i == 0xd65f0fffu) return C_RET;
	if ((i & 0x3ffffc00u) == 0x38bfc000u) return C_LDAPR;
	if ((i & 0xffe0fc00u) == 0x9ac03000u) return C_PACGA;
	return C_BRANCH;
}

static int fd_read(void *ctx, uint64_t off, void *out, size_t n)
{
	return pread(*(int *)ctx, out, n, (off_t)off) == (ssize_t)n ? 0 : -1;
}

static int open_io(const char *path, int *fd, struct b2l_io *io)
{
	struct stat st;
	*fd = open(path, O_RDONLY);
	if (*fd < 0 || fstat(*fd, &st)) { perror(path); return -1; }
	memset(io, 0, sizeof(*io));
	io->read = fd_read; io->ctx = fd; io->bytes = (uint64_t)st.st_size;
	return 0;
}

static void scan_range(int fd, uint64_t off, uint64_t n, const struct b2l_nc_exclusion *x, uint64_t counts[C_COUNT], uint64_t *excluded)
{
	static uint8_t buf[1 << 20];
	for (uint64_t at = 0; at < n; at += sizeof(buf)) {
		size_t len = n - at < sizeof(buf) ? (size_t)(n - at) : sizeof(buf);
		if (pread(fd, buf, len, (off_t)(off + at)) != (ssize_t)len) { fprintf(stderr, "short read\n"); exit(2); }
		for (size_t k = 0; k + 4 <= len; k += 4) {
			uint64_t pos = off + at + k;
			int c = classify(b2l_nc_u32(buf + k));
			if (c == C_NONE) continue;
			if (x && x->bytes && pos >= x->offset && pos < x->offset + x->bytes) { (*excluded)++; continue; }
			counts[c]++;
		}
	}
}

static void report(const char *what, const uint64_t counts[C_COUNT], uint64_t excluded)
{
	uint64_t total = 0;
	for (int c = 1; c < C_COUNT; c++) total += counts[c];
	printf("%s: %llu convertible words (excluded corecrypto: %llu)\n", what, (unsigned long long)total, (unsigned long long)excluded);
	for (int c = 1; c < C_COUNT; c++) printf("  %-18s %llu\n", names[c], (unsigned long long)counts[c]);
}

static int scan_cache(int argc, char **argv)
{
	int mfd; struct b2l_io mio; struct b2l_nc_image image;
	if (open_io(argv[0], &mfd, &mio)) return 2;
	if (b2l_ximage(&mio, &image)) { fprintf(stderr, "%s: no unique corecrypto image\n", argv[0]); return 2; }
	uint64_t all[C_COUNT] = {0}, all_excluded = 0;
	for (int f = 0; f < argc; f++) {
		int fd; struct b2l_io io; uint8_t h[32], m[32];
		if (open_io(argv[f], &fd, &io)) return 2;
		if (pread(fd, h, sizeof(h), 0) != (ssize_t)sizeof(h) || memcmp(h, "dyld_v1", 7)) { fprintf(stderr, "%s: not a dyld cache\n", argv[f]); return 2; }
		struct b2l_nc_exclusion x = {0, 0};
		int r = b2l_xresolve(&io, &image, &x);
		if (r < 0) { fprintf(stderr, "%s: unsafe metadata for exclusion\n", argv[f]); return 2; }
		uint32_t maps = b2l_nc_u32(h + 16), count = b2l_nc_u32(h + 20);
		uint64_t counts[C_COUNT] = {0}, excluded = 0;
		for (uint32_t i = 0; i < count; i++) {
			if (pread(fd, m, 32, maps + (off_t)i * 32) != 32) return 2;
			uint64_t size = b2l_nc_u64(m + 8), off = b2l_nc_u64(m + 16);
			if (!(b2l_nc_u32(m + 28) & 4)) continue;          /* initProt: executable only */
			scan_range(fd, off, size, r == 1 ? &x : NULL, counts, &excluded);
		}
		report(argv[f], counts, excluded);
		for (int c = 0; c < C_COUNT; c++) all[c] += counts[c];
		all_excluded += excluded;
		close(fd);
	}
	report("TOTAL", all, all_excluded);
	return 0;
}

static int scan_macho(int argc, char **argv)
{
	uint64_t all[C_COUNT] = {0}, none = 0;
	for (int f = 0; f < argc; f++) {
		int fd; struct b2l_io io; uint8_t h[32], lc[72], sec[80];
		if (open_io(argv[f], &fd, &io)) continue;
		uint64_t base = 0;
		if (pread(fd, h, 8, 0) != 8) { close(fd); continue; }
		if (b2l_nc_u32(h) == 0xbebafecau) {                 /* fat: pick the arm64e slice */
			uint32_t n = __builtin_bswap32(b2l_nc_u32(h + 4)); uint8_t a[20]; base = UINT64_MAX;
			for (uint32_t i = 0; i < n && i < 16; i++) {
				if (pread(fd, a, 20, 8 + (off_t)i * 20) != 20) break;
				if (__builtin_bswap32(b2l_nc_u32(a)) == 0x100000cu && (__builtin_bswap32(b2l_nc_u32(a + 4)) & 0xffffff) == 2)
					base = __builtin_bswap32(b2l_nc_u32(a + 8));
			}
			if (base == UINT64_MAX) { close(fd); continue; }
		}
		if (pread(fd, h, 32, (off_t)base) != 32 || b2l_nc_u32(h) != 0xfeedfacfu || b2l_nc_u32(h + 4) != 0x100000cu) { close(fd); continue; }
		uint32_t ncmds = b2l_nc_u32(h + 16); uint64_t pos = base + 32; uint64_t counts[C_COUNT] = {0}, excluded = 0;
		for (uint32_t i = 0; i < ncmds && i < 4096; i++) {
			if (pread(fd, lc, 8, (off_t)pos) != 8) break;
			uint32_t cmd = b2l_nc_u32(lc), size = b2l_nc_u32(lc + 4);
			if (size < 8) break;
			if (cmd == 0x19 && pread(fd, lc, 72, (off_t)pos) == 72) {
				uint32_t ns = b2l_nc_u32(lc + 64);
				for (uint32_t j = 0; j < ns && j < 256; j++) {
					if (pread(fd, sec, 80, (off_t)(pos + 72 + j * 80)) != 80) break;
					uint64_t ssize = b2l_nc_u64(sec + 40); uint32_t soff = b2l_nc_u32(sec + 48), flags = b2l_nc_u32(sec + 64);
					if ((flags & 0x80000400u) && soff && ssize) scan_range(fd, base + soff, ssize, NULL, counts, &excluded);
				}
			}
			pos += size;
		}
		uint64_t t = 0; for (int c = 1; c < C_COUNT; c++) { all[c] += counts[c]; t += counts[c]; }
		if (!t) none++;
		close(fd);
	}
	printf("%d Mach-O files, %llu without convertible words\n", argc, (unsigned long long)none);
	report("MACHO TOTAL", all, 0);
	return 0;
}

static int compare(const char *a, const char *b)
{
	int fa, fb; struct b2l_io ia, ib;
	if (open_io(a, &fa, &ia) || open_io(b, &fb, &ib)) return 2;
	if (ia.bytes != ib.bytes) { fprintf(stderr, "size differs\n"); return 2; }
	static uint8_t x[1 << 20], y[1 << 20];
	uint64_t same = 0, translated = 0, other = 0, counts[C_COUNT] = {0};
	for (uint64_t at = 0; at < ia.bytes; at += sizeof(x)) {
		size_t len = ia.bytes - at < sizeof(x) ? (size_t)(ia.bytes - at) : sizeof(x);
		if (pread(fa, x, len, (off_t)at) != (ssize_t)len || pread(fb, y, len, (off_t)at) != (ssize_t)len) return 2;
		if (!memcmp(x, y, len)) { same += len / 4; continue; }
		for (size_t k = 0; k + 4 <= len; k += 4) {
			uint32_t o = b2l_nc_u32(x + k), t = b2l_nc_u32(y + k), want;
			if (o == t) { same++; continue; }
			if (b2l_nc_native(o, &want) && want == t) { translated++; counts[classify(o)]++; continue; }
			if (other++ < 10) printf("  unexpected change at 0x%llx: %08x -> %08x\n", (unsigned long long)(at + k), o, t);
		}
	}
	printf("compare: %llu translated words, %llu unexpected changes\n", (unsigned long long)translated, (unsigned long long)other);
	for (int c = 1; c < C_COUNT; c++) if (counts[c]) printf("  %-18s %llu\n", names[c], (unsigned long long)counts[c]);
	return other ? 1 : 0;
}

/* ---- apply: translate instruction sections of every cache image ---- */
#define MAX_FILES 256
#define MAX_MAPS 4096
struct cmap { int file; uint64_t addr, size, off; uint32_t prot; };
static struct cmap maps[MAX_MAPS];
static unsigned nmaps;
static int fds[MAX_FILES];

static int va_to_file(uint64_t va, uint64_t n, int *file, uint64_t *off)
{
	for (unsigned i = 0; i < nmaps; i++)
		if (va >= maps[i].addr && va - maps[i].addr < maps[i].size && n <= maps[i].size - (va - maps[i].addr)) {
			*file = maps[i].file; *off = maps[i].off + (va - maps[i].addr); return 0;
		}
	return -1;
}

static int apply(int argc, char **argv)
{
	/* argv: OUTDIR MAIN SUB... ; inputs are read-only, outputs are copies in OUTDIR. */
	const char *outdir = argv[0]; argv++; argc--;
	if (argc > MAX_FILES) return 2;
	int mfd; struct b2l_io mio; struct b2l_nc_image cc;
	if (open_io(argv[0], &mfd, &mio) || b2l_ximage(&mio, &cc)) { fprintf(stderr, "main cache unreadable\n"); return 2; }
	struct b2l_nc_exclusion excl[MAX_FILES] = {{0, 0}};
	for (int f = 0; f < argc; f++) {
		char out[4096]; const char *base = strrchr(argv[f], '/'); base = base ? base + 1 : argv[f];
		snprintf(out, sizeof(out), "%s/%s", outdir, base);
		char cmd[9000]; snprintf(cmd, sizeof(cmd), "cp -c '%s' '%s' 2>/dev/null || cp '%s' '%s'", argv[f], out, argv[f], out);
		if (system(cmd)) { fprintf(stderr, "copy failed: %s\n", out); return 2; }
		fds[f] = open(out, O_RDWR);
		if (fds[f] < 0) { perror(out); return 2; }
		struct b2l_io io; memset(&io, 0, sizeof(io)); struct stat st; fstat(fds[f], &st);
		io.read = fd_read; io.ctx = &fds[f]; io.bytes = (uint64_t)st.st_size;
		int r = b2l_xresolve(&io, &cc, &excl[f]);
		if (r < 0) { fprintf(stderr, "%s: unsafe exclusion metadata\n", base); return 2; }
		if (r == 0) excl[f].bytes = 0;
		uint8_t h[32], m[32];
		if (pread(fds[f], h, 32, 0) != 32 || memcmp(h, "dyld_v1", 7)) return 2;
		uint32_t mo = b2l_nc_u32(h + 16), mc = b2l_nc_u32(h + 20);
		for (uint32_t i = 0; i < mc && nmaps < MAX_MAPS; i++) {
			if (pread(fds[f], m, 32, mo + (off_t)i * 32) != 32) return 2;
			maps[nmaps++] = (struct cmap){f, b2l_nc_u64(m), b2l_nc_u64(m + 8), b2l_nc_u64(m + 16), b2l_nc_u32(m + 28)};
		}
	}
	uint8_t h[152], entry[32], mh[32], lc[72], sec[80];
	if (pread(fds[0], h, sizeof(h), 0) != (ssize_t)sizeof(h)) return 2;
	uint64_t at = b2l_nc_u64(h + 136), count = b2l_nc_u64(h + 144);
	uint64_t counts[C_COUNT] = {0}, excluded = 0, images = 0, sections = 0, skipped = 0;
	static uint8_t buf[1 << 20];
	for (uint64_t i = 0; i < count; i++) {
		if (pread(fds[0], entry, 32, (off_t)(at + i * 32)) != 32) return 2;
		uint64_t addr = b2l_nc_u64(entry + 16); int hf; uint64_t hoff;
		if (va_to_file(addr, 32, &hf, &hoff) || pread(fds[hf], mh, 32, (off_t)hoff) != 32 ||
		    b2l_nc_u32(mh) != 0xfeedfacfu || b2l_nc_u32(mh + 4) != 0x100000cu) { skipped++; continue; }
		images++;
		uint32_t ncmds = b2l_nc_u32(mh + 16), sizeofcmds = b2l_nc_u32(mh + 20); uint64_t pos = 32;
		for (uint32_t c = 0; c < ncmds && pos + 8 <= 32 + (uint64_t)sizeofcmds; c++) {
			if (pread(fds[hf], lc, 8, (off_t)(hoff + pos)) != 8) break;
			uint32_t cmd = b2l_nc_u32(lc), size = b2l_nc_u32(lc + 4);
			if (size < 8) break;
			if (cmd == 0x19 && pread(fds[hf], lc, 72, (off_t)(hoff + pos)) == 72) {
				uint32_t ns = b2l_nc_u32(lc + 64);
				for (uint32_t j = 0; j < ns && j < 256; j++) {
					if (pread(fds[hf], sec, 80, (off_t)(hoff + pos + 72 + j * 80)) != 80) break;
					uint64_t va = b2l_nc_u64(sec + 32), n = b2l_nc_u64(sec + 40); uint32_t flags = b2l_nc_u32(sec + 64);
					/* Only sections the linker marks as instructions; never __const, strings or unwind. */
					if (!(flags & 0x80000400u) || !n || (va & 3) || (n & 3)) continue;
					int f; uint64_t off;
					if (va_to_file(va, n, &f, &off)) { skipped++; continue; }
					sections++;
					for (uint64_t done = 0; done < n; done += sizeof(buf)) {
						size_t len = n - done < sizeof(buf) ? (size_t)(n - done) : sizeof(buf);
						if (pread(fds[f], buf, len, (off_t)(off + done)) != (ssize_t)len) return 2;
						int changed = 0;
						for (size_t k = 0; k + 4 <= len; k += 4) {
							uint64_t p = off + done + k; uint32_t w = b2l_nc_u32(buf + k), t; int cl = classify(w);
							if (cl == C_NONE || !b2l_nc_native(w, &t)) continue;
							if (excl[f].bytes && p >= excl[f].offset && p < excl[f].offset + excl[f].bytes) { excluded++; continue; }
							b2l_nc_p32(buf + k, t); counts[cl]++; changed = 1;
						}
						if (changed && pwrite(fds[f], buf, len, (off_t)(off + done)) != (ssize_t)len) return 2;
					}
				}
			}
			pos += size;
		}
	}
	/* Stub islands: iOS 26 caches gather auth stubs outside every image section.
	 * Rewrite only the exact pair "ldr x16, [x17]; braa x16, x17" in executable
	 * mappings (two fixed words, never data by accident). */
	uint64_t stubs = 0;
	for (unsigned mi = 0; mi < nmaps; mi++) {
		if (!(maps[mi].prot & 4)) continue;
		int f = maps[mi].file; uint32_t prev = 0;
		for (uint64_t done = 0; done < maps[mi].size; done += sizeof(buf)) {
			size_t len = maps[mi].size - done < sizeof(buf) ? (size_t)(maps[mi].size - done) : sizeof(buf);
			if (pread(fds[f], buf, len, (off_t)(maps[mi].off + done)) != (ssize_t)len) return 2;
			int changed = 0;
			for (size_t k = 0; k + 4 <= len; k += 4) {
				uint32_t w = b2l_nc_u32(buf + k), t;
				uint64_t p = maps[mi].off + done + k;
				if (w == 0xd71f0a11u && prev == 0xf9400230u && b2l_nc_native(w, &t) &&
				    !(excl[f].bytes && p >= excl[f].offset && p < excl[f].offset + excl[f].bytes)) {
					b2l_nc_p32(buf + k, t); stubs++; counts[C_BRANCH]++; changed = 1;
				}
				prev = w;
			}
			if (changed && pwrite(fds[f], buf, len, (off_t)(maps[mi].off + done)) != (ssize_t)len) return 2;
		}
	}
	printf("apply: %llu stub-island braa rewritten\n", (unsigned long long)stubs);
	for (int f = 0; f < argc; f++) { fsync(fds[f]); close(fds[f]); }
	printf("apply: %llu images, %llu instruction sections, %llu skipped entries\n",
	    (unsigned long long)images, (unsigned long long)sections, (unsigned long long)skipped);
	report("TRANSLATED", counts, excluded);
	return 0;
}

/* ---- macho-apply: in-place translation of standalone arm64e Mach-O files ---- */
static int translate_range(int fd, uint64_t off, uint64_t n, uint64_t counts[C_COUNT])
{
	static uint8_t buf[1 << 20];
	for (uint64_t done = 0; done < n; done += sizeof(buf)) {
		size_t len = n - done < sizeof(buf) ? (size_t)(n - done) : sizeof(buf);
		if (pread(fd, buf, len, (off_t)(off + done)) != (ssize_t)len) return -1;
		int changed = 0;
		for (size_t k = 0; k + 4 <= len; k += 4) {
			uint32_t w = b2l_nc_u32(buf + k), t; int cl = classify(w);
			if (cl == C_NONE || !b2l_nc_native(w, &t)) continue;
			b2l_nc_p32(buf + k, t); counts[cl]++; changed = 1;
		}
		if (changed && pwrite(fd, buf, len, (off_t)(off + done)) != (ssize_t)len) return -1;
	}
	return 0;
}

static int macho_slice(int fd, uint64_t base, uint64_t fsize, uint64_t counts[C_COUNT])
{
	uint8_t h[32], lc[72], sec[80];
	if (pread(fd, h, 32, (off_t)base) != 32 || b2l_nc_u32(h) != 0xfeedfacfu || b2l_nc_u32(h + 4) != 0x100000cu ||
	    (b2l_nc_u32(h + 8) & 0xffffff) != 2) return 0;                    /* arm64e only */
	uint32_t ncmds = b2l_nc_u32(h + 16), sizeofcmds = b2l_nc_u32(h + 20); uint64_t pos = 32;
	int sections = 0;
	for (uint32_t i = 0; i < ncmds && i < 4096 && pos + 8 <= 32 + (uint64_t)sizeofcmds; i++) {
		if (pread(fd, lc, 8, (off_t)(base + pos)) != 8) return -1;
		uint32_t cmd = b2l_nc_u32(lc), size = b2l_nc_u32(lc + 4);
		if (size < 8 || pos + size > 32 + (uint64_t)sizeofcmds) return -1;
		if (cmd == 0x19 && size >= 72 && pread(fd, lc, 72, (off_t)(base + pos)) == 72) {
			uint32_t ns = b2l_nc_u32(lc + 64);
			if (72 + (uint64_t)ns * 80 > size) return -1;
			for (uint32_t j = 0; j < ns; j++) {
				if (pread(fd, sec, 80, (off_t)(base + pos + 72 + j * 80)) != 80) return -1;
				uint64_t ssize = b2l_nc_u64(sec + 40); uint32_t soff = b2l_nc_u32(sec + 48), flags = b2l_nc_u32(sec + 64);
				if (!(flags & 0x80000400u) || !soff || !ssize || (soff & 3) || (ssize & 3)) continue;
				if (base + soff + ssize > fsize) return -1;
				if (translate_range(fd, base + soff, ssize, counts)) return -1;
				sections++;
			}
		}
		pos += size;
	}
	return sections;
}

static int macho_apply(int argc, char **argv)
{
	uint64_t all[C_COUNT] = {0}, files = 0, touched = 0, errors = 0;
	for (int f = 0; f < argc; f++) {
		int fd = open(argv[f], O_RDWR); struct stat st; uint8_t h[8];
		if (fd < 0) { errors++; continue; }
		if (fstat(fd, &st) || pread(fd, h, 8, 0) != 8) { close(fd); continue; }
		uint32_t magic = b2l_nc_u32(h); uint64_t counts[C_COUNT] = {0}; int r = 0;
		if (magic == 0xfeedfacfu) r = macho_slice(fd, 0, (uint64_t)st.st_size, counts);
		else if (magic == 0xbebafecau) {
			uint32_t n = __builtin_bswap32(b2l_nc_u32(h + 4)); uint8_t a[20];
			for (uint32_t i = 0; i < n && i < 16 && r >= 0; i++) {
				if (pread(fd, a, 20, 8 + (off_t)i * 20) != 20) { r = -1; break; }
				if (__builtin_bswap32(b2l_nc_u32(a)) != 0x100000cu || (__builtin_bswap32(b2l_nc_u32(a + 4)) & 0xffffff) != 2) continue;
				int s2 = macho_slice(fd, __builtin_bswap32(b2l_nc_u32(a + 8)), (uint64_t)st.st_size, counts);
				r = s2 < 0 ? -1 : r + s2;
			}
		} else { close(fd); continue; }
		if (r < 0) { fprintf(stderr, "malformed Mach-O, left partially or not translated: %s\n", argv[f]); errors++; }
		uint64_t t = 0; for (int c = 1; c < C_COUNT; c++) { all[c] += counts[c]; t += counts[c]; }
		files++; if (t) { touched++; fsync(fd); }
		close(fd);
	}
	printf("macho-apply: %llu Mach-O files, %llu changed, %llu errors\n",
	    (unsigned long long)files, (unsigned long long)touched, (unsigned long long)errors);
	report("MACHO TRANSLATED", all, 0);
	return errors ? 1 : 0;
}

/* ---- nop-call: verified one-word patch inside cache copies ---- */
static int nop_call(int argc, char **argv)
{
	int sep = -1;
	for (int i = 0; i < argc; i++) if (!strcmp(argv[i], "--")) sep = i;
	if (sep < 1 || argc - sep != 3) return 2;
	uint64_t va = strtoull(argv[sep + 1], NULL, 0), target = strtoull(argv[sep + 2], NULL, 0);
	for (int f = 0; f < sep; f++) {
		fds[f] = open(argv[f], O_RDWR); uint8_t h[32], m[32];
		if (fds[f] < 0 || pread(fds[f], h, 32, 0) != 32 || memcmp(h, "dyld_v1", 7)) { fprintf(stderr, "%s: not a cache\n", argv[f]); return 2; }
		uint32_t mo = b2l_nc_u32(h + 16), mc = b2l_nc_u32(h + 20);
		for (uint32_t i = 0; i < mc && nmaps < MAX_MAPS; i++) {
			if (pread(fds[f], m, 32, mo + (off_t)i * 32) != 32) return 2;
			maps[nmaps++] = (struct cmap){f, b2l_nc_u64(m), b2l_nc_u64(m + 8), b2l_nc_u64(m + 16), b2l_nc_u32(m + 28)};
		}
	}
	int f; uint64_t off; uint8_t w[4];
	if (va_to_file(va, 4, &f, &off) || pread(fds[f], w, 4, (off_t)off) != 4) { fprintf(stderr, "VA not mapped\n"); return 2; }
	uint32_t word = b2l_nc_u32(w);
	if (word == 0xd503201fu) { printf("nop-call: 0x%llx already NOP\n", (unsigned long long)va); return 0; }
	int64_t imm = (int64_t)((word & 0x03ffffffu) << 6) >> 4;
	if ((word & 0xfc000000u) != 0x94000000u || va + (uint64_t)imm != target) {
		fprintf(stderr, "nop-call: 0x%llx is %08x, not bl 0x%llx\n", (unsigned long long)va, word, (unsigned long long)target); return 1;
	}
	b2l_nc_p32(w, 0xd503201fu);
	if (pwrite(fds[f], w, 4, (off_t)off) != 4 || fsync(fds[f])) return 2;
	printf("nop-call: 0x%llx bl 0x%llx -> nop (%s+0x%llx)\n", (unsigned long long)va, (unsigned long long)target, argv[f], (unsigned long long)off);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 4 && !strcmp(argv[1], "apply")) return apply(argc - 2, argv + 2);
	if (argc >= 3 && !strcmp(argv[1], "scan")) return scan_cache(argc - 2, argv + 2);
	if (argc >= 3 && !strcmp(argv[1], "macho")) return scan_macho(argc - 2, argv + 2);
	if (argc == 4 && !strcmp(argv[1], "compare")) return compare(argv[2], argv[3]);
	if (argc >= 3 && !strcmp(argv[1], "macho-apply")) return macho_apply(argc - 2, argv + 2);
	if (argc >= 5 && !strcmp(argv[1], "nop-call")) return nop_call(argc - 2, argv + 2);
	fprintf(stderr, "usage: nc-static scan MAIN [SUB...] | macho FILE... | compare ORIGINAL TRANSLATED\n");
	return 2;
}
