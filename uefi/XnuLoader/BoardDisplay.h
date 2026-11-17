/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef B2L_BOARD_DISPLAY_H
#define B2L_BOARD_DISPLAY_H
/* Board display profile, selected at build time (B2L_FB_LOGICAL=WxH and
 * B2L_DISPLAY_SCALE in scripts/build-efi.sh). The default keeps the S10+
 * behaviour: the panel is presented 1:1 at scale 3. A logical size makes the
 * kernel framebuffer present a smaller CPU canvas scaled to the panel. */
#ifndef B2L_DISPLAY_SCALE
#define B2L_DISPLAY_SCALE 3
#endif
#define B2L_STR2(x) #x
#define B2L_STR(x) B2L_STR2(x)
#if defined(B2L_FB_LOGICAL_W) && defined(B2L_FB_LOGICAL_H) && B2L_FB_LOGICAL_W > 0 && B2L_FB_LOGICAL_H > 0
#define B2L_FB_LOGICAL_ARGS " b2l_fb_lw=" B2L_STR(B2L_FB_LOGICAL_W) " b2l_fb_lh=" B2L_STR(B2L_FB_LOGICAL_H)
#else
#define B2L_FB_LOGICAL_ARGS ""
#endif
#endif
