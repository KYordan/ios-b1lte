/* SPDX-License-Identifier: BSD-2-Clause
 * Exynos 9820 ACPM DVFS client (CPU clusters only), after Samsung's
 * drivers/soc/samsung/acpm/acpm_ipc.c and cal-if/acpm_dvfs.c.
 *
 * The APM firmware loaded by S-Boot owns PLLs, PMIC rails and voltages. The AP
 * only queues {vclk index | seq<<16, rate kHz, FREQ_REQ/FREQ_GET, 0} on the DVFS
 * IPC channel and polls the reply. Rates are only ever chosen from the
 * firmware's own frequency/voltage map (fvmap) and below the DT max-freq.
 * Addresses come from the S10 DTB: acpm_ipc reg 0x15900000 (mailbox) and
 * 0x02039000+0x30000 (SRAM), initdata-base 0x7000, acpm fvmap_offset 0xa400,
 * acpm_dvfs acpm-ipc-channel 5, cpufreq cal-id 0x0b040002/3/4.
 */
#ifndef B2L_ACPM_H
#define B2L_ACPM_H
#include <stdint.h>

#define B2L_ACPM_MBOX_PA      0x15900000ULL
#define B2L_ACPM_MBOX_SIZE    0x1000u
#define B2L_ACPM_SRAM_PA      0x02039000ULL
#define B2L_ACPM_SRAM_SIZE    0x30000u
#define B2L_ACPM_INITDATA     0x7000u
#define B2L_ACPM_FVMAP_OFF    0xa400u
#define B2L_ACPM_DVFS_CH      5u
#define B2L_ACPM_INTGR0       0x08u
#define B2L_ACPM_INTCR1       0x20u
#define B2L_ACPM_FREQ_REQ     0u
#define B2L_ACPM_FREQ_GET     1u
#define B2L_ACPM_MAX_LV       48u

struct b2l_acpm {
    volatile uint8_t *sram, *mbox;
    uint32_t id, elem, len, type, poll;
    uint32_t reply;                        /* poll the reply even on an interrupt channel (B1L-0207, RTC) */
    uint32_t tx_base, tx_front, tx_rear;   /* AP -> APM (firmware "rx") */
    uint32_t rx_base, rx_front, rx_rear;   /* APM -> AP (firmware "tx") */
    uint32_t seq;
    uint32_t fvmap;                        /* SRAM offset, 0 if absent */
};

struct b2l_acpm_table { uint32_t count, rate[B2L_ACPM_MAX_LV], volt[B2L_ACPM_MAX_LV]; };

static inline uint32_t b2l_acpm_rd(const struct b2l_acpm *a, uint32_t off)
{ return *(volatile const uint32_t *)(a->sram + off); }
static inline void b2l_acpm_wr(struct b2l_acpm *a, uint32_t off, uint32_t v)
{ *(volatile uint32_t *)(a->sram + off) = v; }
static inline bool b2l_acpm_in(uint32_t off, uint32_t bytes)
{ return off < B2L_ACPM_SRAM_SIZE && bytes <= B2L_ACPM_SRAM_SIZE - off && !(off & 3); }

/* Parse the framework header and channel descriptor. Every offset is
 * bounds-checked against the mapped SRAM; nothing is written here. */
static inline int b2l_acpm_parse_ch(struct b2l_acpm *a, uint32_t ch)
{
    const uint32_t f = B2L_ACPM_INITDATA;
    uint32_t plugins = b2l_acpm_rd(a, f + 0), nplugins = b2l_acpm_rd(a, f + 4);
    uint32_t chans = b2l_acpm_rd(a, f + 8), ap_max = b2l_acpm_rd(a, f + 24);
    if (ap_max <= ch || ap_max > 32 || !b2l_acpm_in(chans, 72u * ap_max)) return -1;
    uint32_t c = chans + 72u * ch, ci = c + 16;
    a->id = b2l_acpm_rd(a, c);
    a->type = b2l_acpm_rd(a, c + 12);              /* 1 TYPE_QUEUE, 2 TYPE_BUFFER */
    a->poll = b2l_acpm_rd(a, c + 68);              /* ap_poll */
    a->len = b2l_acpm_rd(a, ci + 36);
    a->elem = b2l_acpm_rd(a, ci + 40);
    a->tx_rear = b2l_acpm_rd(a, ci + 0); a->tx_front = b2l_acpm_rd(a, ci + 4); a->tx_base = b2l_acpm_rd(a, ci + 8);
    a->rx_rear = b2l_acpm_rd(a, ci + 24); a->rx_front = b2l_acpm_rd(a, ci + 28); a->rx_base = b2l_acpm_rd(a, ci + 32);
    if (a->id >= 16 || a->elem < 16 || a->elem > 64 || (a->elem & 3) || a->len < 1 || a->len > 64 ||
        (a->type != 1 && a->type != 2) ||
        !b2l_acpm_in(a->tx_rear, 4) || !b2l_acpm_in(a->tx_front, 4) || !b2l_acpm_in(a->tx_base, a->elem * a->len) ||
        !b2l_acpm_in(a->rx_rear, 4) || !b2l_acpm_in(a->rx_front, 4) || !b2l_acpm_in(a->rx_base, a->elem * a->len))
        return -2;
    /* Resident DVFS plugin (is_attached == 1, stay_attached == 1): fvmap lives at
     * its base + fvmap_offset, exactly as acpm.c computes it. */
    a->fvmap = 0;
    if (nplugins && nplugins <= 32 && b2l_acpm_in(plugins, 48u * nplugins)) {
        for (uint32_t i = 0; i < nplugins; i++) {
            uint32_t p = plugins + 48u * i, base = b2l_acpm_rd(a, p + 4) & ~1u, name = b2l_acpm_rd(a, p + 44);
            uint8_t att = a->sram[p + 32], stay = a->sram[p + 40];
            if (att != 1 || stay != 1 || !name || name >= B2L_ACPM_SRAM_SIZE - 16) continue;
            bool dvfs = false;
            for (uint32_t k = 0; k + 4 <= 32 && name + k + 4 <= B2L_ACPM_SRAM_SIZE && a->sram[name + k]; k++) {
                char c0 = (char)(a->sram[name + k] | 0x20), c1 = (char)(a->sram[name + k + 1] | 0x20),
                     c2 = (char)(a->sram[name + k + 2] | 0x20), c3 = (char)(a->sram[name + k + 3] | 0x20);
                if (c0 == 'd' && c1 == 'v' && c2 == 'f' && c3 == 's') { dvfs = true; break; }
            }
            if (dvfs && base < B2L_ACPM_SRAM_SIZE && b2l_acpm_in(base + B2L_ACPM_FVMAP_OFF, 24u * 8u)) {
                a->fvmap = base + B2L_ACPM_FVMAP_OFF; break;
            }
        }
    }
    return 0;
}

static inline int b2l_acpm_parse(struct b2l_acpm *a) { return b2l_acpm_parse_ch(a, B2L_ACPM_DVFS_CH); }

/* fvmap_header (24 bytes): num_of_lv at +1, o_ratevolt (u16) at +20; then
 * {rate kHz, volt uV} pairs, highest level first. Returns 0 if plausible. */
static inline int b2l_acpm_table(const struct b2l_acpm *a, uint32_t idx, struct b2l_acpm_table *t)
{
    t->count = 0;
    if (!a->fvmap || idx >= 32 || !b2l_acpm_in(a->fvmap + 24u * idx, 24)) return -1;
    uint32_t h = a->fvmap + 24u * idx, n = a->sram[h + 1];
    uint32_t o = (uint32_t)a->sram[h + 20] | ((uint32_t)a->sram[h + 21] << 8);
    if (n < 2 || n > B2L_ACPM_MAX_LV || !b2l_acpm_in(a->fvmap + o, 8u * n)) return -2;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t r = b2l_acpm_rd(a, a->fvmap + o + 8 * i), v = b2l_acpm_rd(a, a->fvmap + o + 8 * i + 4);
        if (r < 100000 || r > 3500000 || v < 400000 || v > 1300000) return -3;
        if (i && r >= t->rate[i - 1]) return -4;   /* strictly descending */
        t->rate[i] = r; t->volt[i] = v;
    }
    t->count = n;
    return 0;
}

/* Highest firmware level strictly below the device's DT max-freq ("almost max"). */
static inline uint32_t b2l_acpm_pick(const struct b2l_acpm_table *t, uint32_t dt_max_khz)
{
    for (uint32_t i = 0; i < t->count; i++) if (t->rate[i] < dt_max_khz) return t->rate[i];
    return 0;
}
#endif
