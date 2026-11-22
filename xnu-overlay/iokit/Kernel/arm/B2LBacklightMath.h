/* SPDX-License-Identifier: BSD-2-Clause
 * S10e build 25: the arithmetic of B2LBacklight.h, kept apart for the host test (tests/test_backlight.py).
 * S6E3FA7 beyond0 (Samsung's s6e3fa7_beyond0_a3_s0 tables): vtotal 2304 lines, AOR 0x0014 at its 420-nit entry,
 * 0x08D4 at its darkest.
 */
#ifndef B2L_BACKLIGHT_MATH_H
#define B2L_BACKLIGHT_MATH_H
#include <stdint.h>

#define B2L_BL_VTOTAL                2304u
#define B2L_BL_AOR_BRIGHTEST         0x0014u
#define B2L_BL_AOR_DARKEST           0x08D4u
#define B2L_BL_MAX_NITS              420u
#define B2L_BL_RANGE                 65536u

#define B2L_BL_MAX_NITS16            (B2L_BL_MAX_NITS << 16)

/* AOR for a luminance (16.16 nits) at the bootloader's gamma: emission share = nits / 420 of the brightest entry's. */
static inline uint32_t
b2l_bl_aor_for_nits16(uint32_t n16)
{
	if (n16 >= B2L_BL_MAX_NITS16) {
		return B2L_BL_AOR_BRIGHTEST;
	}
	uint32_t emit = (uint32_t)((uint64_t)(B2L_BL_VTOTAL - B2L_BL_AOR_BRIGHTEST) * n16 / B2L_BL_MAX_NITS16);   /* lines lit */
	uint32_t aor = B2L_BL_VTOTAL - emit;
	if (aor < B2L_BL_AOR_BRIGHTEST) {
		aor = B2L_BL_AOR_BRIGHTEST;
	}
	if (aor > B2L_BL_AOR_DARKEST) {
		aor = B2L_BL_AOR_DARKEST;
	}
	return aor;
}

static inline uint32_t
b2l_bl_aor_for_nits(uint32_t nits)
{
	return b2l_bl_aor_for_nits16(nits >= B2L_BL_MAX_NITS ? B2L_BL_MAX_NITS16 : nits << 16);
}

/* "brightness-nits" from backboardd is 16.16 nits (build 26 log: 11365869 = 173.4 nits, 672751 = 10.3 nits; build 26
 * read the small ones as millinits, so the bottom of the slider was full brightness). Plain nits (<= 2000) are
 * kept for hand tests. Result: 16.16 nits, at most 420. */
static inline uint32_t
b2l_bl_nits16_from_raw(uint64_t v)
{
	if (v <= 2000) {
		v <<= 16;
	}
	return v > B2L_BL_MAX_NITS16 ? B2L_BL_MAX_NITS16 : (uint32_t)v;
}
#endif
