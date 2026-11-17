/* Host test: B2LACPM.h parsing against a synthetic SRAM laid out per Samsung's fw_header. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../xnu-overlay/iokit/Kernel/arm/B2LACPM.h"
static uint8_t sram[B2L_ACPM_SRAM_SIZE];
static void w32(uint32_t o, uint32_t v) { memcpy(sram + o, &v, 4); }
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static void build(void)
{
    memset(sram, 0, sizeof sram);
    uint32_t f = B2L_ACPM_INITDATA, plugins = 0x7400, chans = 0x7800, base = 0x10000;
    w32(f + 0, plugins); w32(f + 4, 3); w32(f + 8, chans); w32(f + 12, 8); w32(f + 24, 8);
    for (uint32_t i = 0; i < 8; i++) {
        uint32_t c = chans + 72 * i, ci = c + 16;
        w32(c, i); w32(c + 12, i == 5 ? 2 : 1); w32(c + 68, i == 5 ? 0 : 1);
        w32(ci + 0, 0x8000 + i * 0x100); w32(ci + 4, 0x8004 + i * 0x100); w32(ci + 8, 0x8010 + i * 0x100);
        w32(ci + 24, 0x8008 + i * 0x100); w32(ci + 28, 0x800c + i * 0x100); w32(ci + 32, 0x8080 + i * 0x100);
        w32(ci + 36, 4); w32(ci + 40, 16);
    }
    /* plugin 0: not resident; plugin 1: "tmu"; plugin 2: resident "DVFS" */
    const char *names[3] = { "flexpmu", "tmu", "acpm_DVFS" };
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t p = plugins + 48 * i, n = 0x7600 + 0x20 * i;
        strcpy((char *)sram + n, names[i]);
        w32(p, i); w32(p + 4, (base | 1)); w32(p + 44, n);
        sram[p + 32] = i ? 1 : 0; sram[p + 40] = 1;
    }
    uint32_t fv = base + B2L_ACPM_FVMAP_OFF;
    /* idx 4: 4 levels at o_ratevolt 0x200 */
    uint32_t h = fv + 24 * 4; sram[h + 1] = 4; sram[h + 20] = 0x00; sram[h + 21] = 0x02;
    uint32_t r[4] = { 2730000, 2080000, 1976000, 1898000 }, v[4] = { 1100000, 900000, 875000, 850000 };
    for (int i = 0; i < 4; i++) { w32(fv + 0x200 + 8 * i, r[i]); w32(fv + 0x200 + 8 * i + 4, v[i]); }
}
int main(void)
{
    struct b2l_acpm a = { .sram = sram };
    struct b2l_acpm_table t;
    build();
    CHECK(b2l_acpm_parse(&a) == 0);
    CHECK(a.id == 5 && a.elem == 16 && a.len == 4 && a.type == 2 && a.poll == 0);
    CHECK(a.tx_rear == 0x8500 && a.tx_front == 0x8504 && a.tx_base == 0x8510);
    CHECK(a.rx_rear == 0x8508 && a.rx_front == 0x850c && a.rx_base == 0x8580);
    CHECK(a.fvmap == 0x10000 + B2L_ACPM_FVMAP_OFF);
    CHECK(b2l_acpm_table(&a, 4, &t) == 0 && t.count == 4 && t.rate[0] == 2730000 && t.volt[3] == 850000);
    CHECK(b2l_acpm_pick(&t, 2080000) == 1976000);   /* strictly below DT max */
    CHECK(b2l_acpm_pick(&t, 3000000) == 2730000);
    CHECK(b2l_acpm_pick(&t, 1800000) == 0);
    CHECK(b2l_acpm_table(&a, 3, &t) == -2 && t.count == 0);   /* empty header */
    /* non-descending table and out-of-range voltage are rejected */
    w32(a.fvmap + 0x200 + 8, 2730000); CHECK(b2l_acpm_table(&a, 4, &t) == -4);
    build(); w32(a.fvmap + 0x200 + 4, 1500000); CHECK(b2l_acpm_table(&a, 4, &t) == -3);
    /* S10 firmware: DVFS ch5 is a one-slot TYPE_BUFFER channel */
    build(); w32(0x7800 + 72 * 5 + 16 + 36, 1); CHECK(b2l_acpm_parse(&a) == 0 && a.len == 1);
    build(); w32(0x7800 + 72 * 5 + 12, 3); CHECK(b2l_acpm_parse(&a) == -2);
    /* bad channel geometry */
    build(); w32(0x7800 + 72 * 5 + 16 + 40, 12); CHECK(b2l_acpm_parse(&a) == -2);
    build(); w32(B2L_ACPM_INITDATA + 24, 5); CHECK(b2l_acpm_parse(&a) == -1);
    build(); w32(0x7800 + 72 * 5 + 16 + 8, B2L_ACPM_SRAM_SIZE - 8); CHECK(b2l_acpm_parse(&a) == -2);
    /* no resident DVFS plugin -> fvmap absent, table refused */
    build(); sram[0x7400 + 48 * 2 + 32] = 0; CHECK(b2l_acpm_parse(&a) == 0 && a.fvmap == 0 && b2l_acpm_table(&a, 4, &t) == -1);
    printf(fails ? "acpm: %d failures\n" : "acpm: all checks passed\n", fails);
    return fails != 0;
}
