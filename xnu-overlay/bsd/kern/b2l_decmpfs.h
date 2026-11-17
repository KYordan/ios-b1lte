/* SPDX-License-Identifier: BSD-2-Clause
 * Kernel glue for b2l_decmpfs_core.h, included by decmpfs.c (patch 0102) so the
 * decoders are registered next to the built-in Type1 one in decmpfs_init().
 * Read-only: decmpfs decompress-in-place (decmpfs_decompress_file) still works
 * because it goes through the same fetch callback.
 * Kill switch: boot-arg b2l_decmpfs=0.
 */
#ifndef B2L_DECMPFS_H
#define B2L_DECMPFS_H
#if defined(ARM64_BOARD_CONFIG_B2L)
#include <libkern/zlib.h>
#include <pexpert/pexpert.h>
#include "b2l_decmpfs_core.h"

#define B2L_CMP_MAX_USIZE (64ull << 20)         /* refuse absurd headers */

static void *
b2l_cmp_za(void *o, unsigned n, unsigned s)
{
	uint8_t *p;
	size_t len;

	(void)o;
	if (n && s > (SIZE_MAX - 16) / n) {
		return NULL;
	}
	len = (size_t)n * s + 16;
	p = kalloc_data(len, Z_WAITOK | Z_ZERO);
	if (p == NULL) {
		return NULL;
	}
	*(size_t *)(void *)p = len;
	return p + 16;
}

static void
b2l_cmp_zf(void *o, void *q)
{
	uint8_t *p;

	(void)o;
	if (q == NULL) {
		return;
	}
	p = (uint8_t *)q - 16;
	kfree_data(p, *(size_t *)(void *)p);
}

struct b2l_cmp_rctx {
	vnode_t vp;
	vfs_context_t ctx;
};

/* The resource fork is visible to decmpfs_ctx only (decmpfs_hides_xattr). */
static int
b2l_cmp_rsrc_read(void *c, uint64_t off, void *buf, size_t len)
{
	struct b2l_cmp_rctx *r = c;
	size_t sz = 0;
	uio_t uio;
	int err;

	uio = uio_create(1, (off_t)off, UIO_SYSSPACE, UIO_READ);
	if (uio == NULL) {
		return ENOMEM;
	}
	uio_addiov(uio, CAST_USER_ADDR_T(buf), (user_size_t)len);
	err = vn_getxattr(r->vp, XATTR_RESOURCEFORK_NAME, uio, &sz, XATTR_NOSECURITY, r->ctx);
	if (err == 0 && uio_resid(uio) != 0) {
		err = EIO;
	}
	uio_free(uio);
	return err;
}

static uint32_t b2l_cmp_logged;                 /* one line per type and outcome */

static void
b2l_cmp_log(uint32_t type, int err, uint64_t usize)
{
	uint32_t bit = (type & 15u) << 1 | (err != 0);

	if ((b2l_cmp_logged & (1u << bit)) == 0) {
		b2l_cmp_logged |= 1u << bit;
		printf("b2l_decmpfs: type %u %s (size %llu, err %d)\n", type,
		    err ? "FAILED" : "served", (unsigned long long)usize, err);
	}
}

static int
b2l_cmp_validate(__unused vnode_t vp, __unused vfs_context_t ctx, decmpfs_header *hdr)
{
	size_t n = hdr->attr_size > sizeof(decmpfs_disk_header) ? hdr->attr_size - sizeof(decmpfs_disk_header) : 0;

	if (hdr->uncompressed_size > B2L_CMP_MAX_USIZE) {
		return EINVAL;
	}
	switch (hdr->compression_type) {
	case B2L_CMP_STORED_XATTR:
		return (n >= 1 && (uint8_t)hdr->attr_bytes[0] == 0xcc && n - 1 == hdr->uncompressed_size) ? 0 : EINVAL;
	case B2L_CMP_ZLIB_XATTR:
		return n >= 1 ? 0 : EINVAL;
	case B2L_CMP_ZLIB_RSRC:
		return 0;
	default:
		return ENOTSUP;
	}
}

static int
b2l_cmp_fetch(vnode_t vp, vfs_context_t ctx, decmpfs_header *hdr, off_t offset, user_ssize_t size,
    int nvec, decmpfs_vector *vec, uint64_t *bytes_read)
{
	uint64_t usize = hdr->uncompressed_size, done = 0, cap = 0, end;
	uint8_t *dst = NULL, *tmp = NULL, *work = NULL;
	size_t tmplen = 0;
	int err = 0, i;

	if (bytes_read) {
		*bytes_read = 0;
	}
	if (offset < 0 || size < 0 || usize > B2L_CMP_MAX_USIZE) {
		return EINVAL;
	}
	for (i = 0; i < nvec; i++) {
		cap += (uint64_t)vec[i].size;
	}
	if ((uint64_t)size > cap) {
		size = (user_ssize_t)cap;
	}
	if ((uint64_t)offset >= usize || size == 0) {
		return 0;
	}
	end = MIN(usize, (uint64_t)offset + (uint64_t)size);

	if (hdr->compression_type == B2L_CMP_ZLIB_RSRC) {
		struct b2l_cmp_rctx rc = { vp, ctx };

		work = kalloc_data(2 * B2L_CMP_BLOCK_MAX, Z_WAITOK);
		if (work == NULL) {
			return ENOMEM;
		}
		if (nvec == 1) {
			dst = vec[0].buf;
		} else {
			tmplen = (size_t)(end - (uint64_t)offset);
			dst = tmp = kalloc_data(tmplen, Z_WAITOK);
		}
		if (dst == NULL) {
			err = ENOMEM;
		} else {
			err = b2l_cmp_rsrc_fetch(b2l_cmp_rsrc_read, &rc, usize, (uint64_t)offset,
			    end - (uint64_t)offset, dst, work, &done, b2l_cmp_za, b2l_cmp_zf);
		}
	} else {
		size_t n = hdr->attr_size > sizeof(decmpfs_disk_header) ? hdr->attr_size - sizeof(decmpfs_disk_header) : 0;

		tmplen = (size_t)usize;
		tmp = kalloc_data(tmplen ? tmplen : 1, Z_WAITOK);
		if (tmp == NULL) {
			return ENOMEM;
		}
		err = b2l_cmp_xattr_decode(hdr->compression_type, (const uint8_t *)hdr->attr_bytes, n, tmp, usize,
		    b2l_cmp_za, b2l_cmp_zf);
		if (err == 0) {
			memmove(tmp, tmp + offset, (size_t)(end - (uint64_t)offset));
			done = end - (uint64_t)offset;
		}
	}
	if (err == 0 && tmp != NULL) {                 /* scatter into the caller's vectors */
		uint64_t pos = 0;
		for (i = 0; i < nvec && pos < done; i++) {
			size_t c = (size_t)MIN((uint64_t)vec[i].size, done - pos);
			memcpy(vec[i].buf, tmp + pos, c);
			pos += c;
		}
	}
	if (err == 0 && bytes_read) {
		*bytes_read = done;
	}
	b2l_cmp_log(hdr->compression_type, err, usize);
	if (tmp) {
		kfree_data(tmp, tmplen ? tmplen : 1);
	}
	if (work) {
		kfree_data(work, 2 * B2L_CMP_BLOCK_MAX);
	}
	return err;
}

SECURITY_READ_ONLY_EARLY(static decmpfs_registration) b2l_cmp_reg = {
	.decmpfs_registration = DECMPFS_REGISTRATION_VERSION,
	.validate          = b2l_cmp_validate,
	.adjust_fetch      = NULL,
	.fetch             = b2l_cmp_fetch,
	.free_data         = NULL,
	.get_flags         = NULL
};

static void
b2l_decmpfs_register(void)
{
	int on = 1;

	PE_parse_boot_argn("b2l_decmpfs", &on, sizeof(on));
	if (!on) {
		printf("b2l_decmpfs: disabled by boot-arg\n");
		return;
	}
	register_decmpfs_decompressor(B2L_CMP_ZLIB_XATTR, &b2l_cmp_reg);
	register_decmpfs_decompressor(B2L_CMP_ZLIB_RSRC, &b2l_cmp_reg);
	register_decmpfs_decompressor(B2L_CMP_STORED_XATTR, &b2l_cmp_reg);
	printf("b2l_decmpfs: registered HFS compression types 3, 4, 9\n");
}
#endif
#endif
