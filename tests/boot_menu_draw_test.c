/* Renders the boot menu screen on the host. With an argument, also writes it as a PPM for a look. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../uefi/XnuLoader/BootMenu.h"
#include "../uefi/XnuLoader/BootMenuDraw.h"

int main(int argc, char **argv)
{
    enum { W = 1440, H = 3040, STRIDE = 1472 };
    uint32_t *px = calloc((size_t)STRIDE * H + 1, 4);
    px[(size_t)STRIDE * H] = 0xdeadbeef;    /* guard after the last row */
    struct b2l_menu_view v = { B2L_MENU_BOOT, 4, 5, NULL,
        "The last boot failed (see the previous message). Automatic boot is off until you choose." };
    b2l_menu_render(px, STRIDE, W, H, &v);
    assert(px[(size_t)STRIDE * H] == 0xdeadbeef);
    assert(px[W] == 0 && px[STRIDE - 1] == 0);  /* padding past the width is untouched */
    unsigned accent = 0;
    for (size_t i = 0; i < (size_t)STRIDE * H; i++) accent += px[i] == 0x0a84ff;
    assert(accent > 100000);                    /* selected card drawn */
    for (unsigned sel = 0; sel < B2L_MENU_COUNT; sel++) {   /* every state, and a tiny screen, stays in bounds */
        struct b2l_menu_view o = { sel, 0, 5, sel == 3 ? "Rebooting..." : NULL, NULL };
        b2l_menu_render(px, STRIDE, W, H, &o);
        b2l_menu_render(px, 100, 100, 100, &o);
    }
    {   /* S10e panel (1080 x 2280): drawn, and nothing past the width or the last row */
        enum { W2 = 1080, H2 = 2280, S2 = 1088 };
        uint32_t *q = calloc((size_t)S2 * H2 + 1, 4);
        q[(size_t)S2 * H2] = 0xdeadbeef;
        b2l_menu_render(q, S2, W2, H2, &v);
        assert(q[(size_t)S2 * H2] == 0xdeadbeef);
        unsigned acc2 = 0;
        for (size_t y = 0; y < H2; y++) {
            for (size_t x = W2; x < S2; x++) assert(q[y * S2 + x] == 0);
            for (size_t x = 0; x < W2; x++) acc2 += q[y * S2 + x] == 0x0a84ff;
        }
        assert(acc2 > 50000);
        free(q);
    }
    b2l_menu_render(px, STRIDE, W, H, &v);
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        fprintf(f, "P6 %d %d 255\n", W, H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                uint32_t p = px[(size_t)y * STRIDE + x];
                fputc(p >> 16 & 255, f); fputc(p >> 8 & 255, f); fputc(p & 255, f);
            }
        fclose(f);
    }
    free(px);
    puts("boot_menu_draw_test: ok");
    return 0;
}
