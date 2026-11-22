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

/* AOR for a luminance at the bootloader's gamma: emission share = nits / 420 of the brightest entry's. */
static inline uint32_t
b2l_bl_aor_for_nits(uint32_t nits)
{
	if (nits >= B2L_BL_MAX_NITS) {
		return B2L_BL_AOR_BRIGHTEST;
	}
	uint32_t emit = (B2L_BL_VTOTAL - B2L_BL_AOR_BRIGHTEST) * nits / B2L_BL_MAX_NITS;   /* lines lit */
	uint32_t aor = B2L_BL_VTOTAL - emit;
	if (aor < B2L_BL_AOR_BRIGHTEST) {
		aor = B2L_BL_AOR_BRIGHTEST;
	}
	if (aor > B2L_BL_AOR_DARKEST) {
		aor = B2L_BL_AOR_DARKEST;
	}
	return aor;
}

/* "brightness-nits": nits (<= 2000), 16.16 nits (low 16 bits 0, or > 1 000 000) or millinits. */
static inline uint32_t
b2l_bl_nits_from_raw(uint64_t v)
{
	if (v <= 2000) {
		return (uint32_t)v;
	}
	if ((v & 0xffff) == 0 || v > 1000000) {
		return (uint32_t)(v >> 16);
	}
	return (uint32_t)(v / 1000);
}
#endif
