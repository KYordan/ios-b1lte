/* SPDX-License-Identifier: BSD-2-Clause
 * Boot menu screen, drawn into a 32-bit B,G,R,X pixel buffer (0x00RRGGBB as a little-endian uint32). No UEFI
 * dependency: tests/boot_menu_draw_test.c renders it on the host. BootMenuEfi.c blits the buffer to the GOP.
 */
#ifndef B2L_BOOT_MENU_DRAW_H
#define B2L_BOOT_MENU_DRAW_H

#include <stdint.h>

struct b2l_menu_view {
    unsigned selected;          /* enum b2l_menu_item */
    unsigned seconds_left;      /* 0: no countdown running */
    unsigned timeout_s;         /* full countdown, for the bar */
    const char *status;         /* replaces the countdown line when not NULL (e.g. "Rebooting...") */
    const char *note;           /* amber message under the menu, may be NULL; printable ASCII, wrapped */
};

void b2l_menu_render(uint32_t *pixels, uint32_t stride, uint32_t width, uint32_t height,
                     const struct b2l_menu_view *view);

#endif
