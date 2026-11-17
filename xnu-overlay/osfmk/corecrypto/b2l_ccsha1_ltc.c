/* B1L-0202: SHA-1 for the kernel. No corecrypto kext here (g_crypto_funcs == NULL),
 * and IPv6 CGA link-local (in6_cga.c), MPTCP and code signing call SHA1Init. */
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccdigest_priv.h>
#include "cc_internal.h"
#include "ccdigest_internal.h"

static const uint32_t b2l_sha1_initial_state[5] = {
	0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
};

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void
b2l_sha1_compress(ccdigest_state_t state, size_t nblocks, const void *in)
{
	uint32_t *s = ccdigest_u32(state);
	const unsigned char *buf = in;
	uint32_t W[80], a, b, c, d, e, f, k, t;
	int i;

	while (nblocks--) {
		for (i = 0; i < 16; i++) {
			W[i] = cc_load32_be(buf + 4 * i);
		}
		for (; i < 80; i++) {
			W[i] = ROL(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);
		}
		a = s[0]; b = s[1]; c = s[2]; d = s[3]; e = s[4];
		for (i = 0; i < 80; i++) {
			if (i < 20) {
				f = (b & c) | (~b & d); k = 0x5a827999;
			} else if (i < 40) {
				f = b ^ c ^ d; k = 0x6ed9eba1;
			} else if (i < 60) {
				f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc;
			} else {
				f = b ^ c ^ d; k = 0xca62c1d6;
			}
			t = ROL(a, 5) + f + e + k + W[i];
			e = d; d = c; c = ROL(b, 30); b = a; a = t;
		}
		s[0] += a; s[1] += b; s[2] += c; s[3] += d; s[4] += e;
		buf += CCSHA1_BLOCK_SIZE;
	}
}

const struct ccdigest_info ccsha1_ltc_di = {
	.output_size = CCSHA1_OUTPUT_SIZE,
	.state_size = CCSHA1_STATE_SIZE,
	.block_size = CCSHA1_BLOCK_SIZE,
	.oid_size = ccoid_sha1_len,
	.oid = CC_DIGEST_OID_SHA1,
	.initial_state = b2l_sha1_initial_state,
	.compress = b2l_sha1_compress,
	.final = ccdigest_final_64be,
	.impl = CC_IMPL_SHA1_LTC,
};
