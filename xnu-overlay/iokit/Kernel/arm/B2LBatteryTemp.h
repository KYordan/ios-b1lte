/* SPDX-License-Identifier: BSD-2-Clause
 * S10e build 30: thermistor arithmetic and the charge permission of B2LBattery.h, kept apart for the host test
 * (tests/test_battery_temp.py).
 *
 * Tables: the S10e's own DTB (boards/beyond0lte, node /battery): battery,temp_table_adc (battery, ADC channel 4),
 * battery,chg_temp_table_adc (charger, channel 3), battery,usb_temp_table_adc (USB port, channel 1); all three with
 * temp_table_data 90.0 C down to -20.0 C in 5 C steps (the ADC rises as the NTC cools). Same channels as
 * Jesvi-iOS's S20 (MIT/APSL, github.com/jesvi-eulah/Jesvi-iOS, redwood_max77705.c).
 *
 * Charge permission (0.1 C, with hysteresis): charging is allowed only while every reading is known and inside
 * these limits. Samsung's own DTB values are the reference; ours are stricter where Samsung lowers the current
 * instead (we never change currents, so we stop):
 *   battery   stop >= 45.0, again <= 42.0   (Samsung: 1050 mA from 41.0, stop at 50.0)
 *   battery   stop <=  5.0, again >=  7.0   (Samsung: 300 mA below 5.0, stop at 0.0)
 *   charger   stop >= 46.0, again <= 44.0   (Samsung battery,chg_high_temp 46.0 / 44.0: input limit 1000 mA)
 *   USB port  stop >= 50.0, again <= 45.0
 * Readings of 85 C or more, or below -20 C (a short, an open thermistor, a failed conversion), are unknown: no charge.
 */
#ifndef B2L_BATTERY_TEMP_H
#define B2L_BATTERY_TEMP_H
#include <stdint.h>

#define B2L_TEMP_UNKNOWN   (-32768)
#define B2L_TEMP_TOP       900
#define B2L_TEMP_STEPS     23

static const uint16_t b2l_temp_bat_adc[B2L_TEMP_STEPS] = { 0x130, 0x165, 0x19e, 0x1d4, 0x231, 0x289, 0x2ed, 0x368, 0x3ff,
	0x49f, 0x554, 0x618, 0x6eb, 0x7cf, 0x8af, 0x998, 0xa7d, 0xb5a, 0xc1f, 0xcd2, 0xd64, 0xde3, 0xe4e };
static const uint16_t b2l_temp_chg_adc[B2L_TEMP_STEPS] = { 0xff, 0x138, 0x171, 0x19a, 0x206, 0x260, 0x2c6, 0x342, 0x3d9,
	0x47c, 0x532, 0x5f8, 0x6ca, 0x7b3, 0x898, 0x97d, 0xa64, 0xb3f, 0xc0f, 0xcb8, 0xd50, 0xdcd, 0xe37 };
/* (the DTB's first USB entry, 0x171, is above its second; entry 0 is never used, see below) */
static const uint16_t b2l_temp_usb_adc[B2L_TEMP_STEPS] = { 0x171, 0x144, 0x180, 0x1bf, 0x219, 0x276, 0x2e2, 0x35e, 0x3fa,
	0x49d, 0x552, 0x615, 0x6e7, 0x7c3, 0x89c, 0x976, 0xa4d, 0xb1c, 0xbd0, 0xc72, 0xcf8, 0xd67, 0xdc9 };

/* 12-bit ADC value -> 0.1 C, linear between the 5 C entries from 85 C (entry 1) to -20 C; else B2L_TEMP_UNKNOWN. */
static inline int
b2l_temp_from_adc(int raw, const uint16_t *t)
{
	if (raw < 0 || raw > 0xfff || raw < t[1]) {
		return B2L_TEMP_UNKNOWN;
	}
	for (int i = 2; i < B2L_TEMP_STEPS; i++) {
		if (raw <= t[i]) {
			int lo = t[i - 1], hi = t[i];
			return B2L_TEMP_TOP - 50 * (i - 1) - 50 * (raw - lo) / (hi - lo);
		}
	}
	return B2L_TEMP_UNKNOWN;
}

struct b2l_charge_gate {
	int allowed;                        /* 1: the last decision allowed charging */
	const char *why;                    /* why not, while not allowed */
};

/* One decision from three readings (0.1 C). Once stopped, every limit must be back on its "again" side. */
static inline int
b2l_charge_allowed(struct b2l_charge_gate *g, int bat, int chg, int usb)
{
	if (bat == B2L_TEMP_UNKNOWN || chg == B2L_TEMP_UNKNOWN || usb == B2L_TEMP_UNKNOWN) {
		g->allowed = 0;
		g->why = "a thermistor reads nothing usable";
		return 0;
	}
	if (g->allowed) {
		g->why = bat >= 450 ? "battery at 45 C" : bat <= 50 ? "battery at 5 C" : chg >= 460 ? "charger at 46 C" :
		    usb >= 500 ? "USB port at 50 C" : "";
		g->allowed = !g->why[0];
	} else {
		g->why = bat > 420 ? "battery above 42 C" : bat < 70 ? "battery below 7 C" : chg > 440 ? "charger above 44 C" :
		    usb > 450 ? "USB port above 45 C" : "";
		g->allowed = !g->why[0];
	}
	return g->allowed;
}
#endif
