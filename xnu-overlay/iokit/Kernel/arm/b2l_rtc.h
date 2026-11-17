/* SPDX-License-Identifier: BSD-2-Clause
 * S2MPS19 RTC counter (Samsung rtc-s2mps19.c, CONFIG_RTC_DRV_S2MP unset): registers SEC..YEAR at 0x05..0x0B of
 * the RTC block (speedy type 0x02), binary, 24 h, year = 2000 + (YEAR & 0x7f), kept in UTC by Android. */
#ifndef B2L_RTC_H
#define B2L_RTC_H
#include <stdint.h>
#define B2L_RTC_TYPE        0x02u
#define B2L_RTC_REG_UPDATE  0x02u
#define B2L_RTC_REG_SEC     0x05u
#define B2L_RTC_NREGS       7u
#define B2L_RTC_RUDR        0x01u   /* latch the counters into the read buffer */
#define B2L_RTC_UDR_BITS    0x17u   /* WUDR | FREEZE | RUDR | AUDR, cleared before RUDR as the driver does */

/* d = {sec, min, hour, weekday, date, month, year}; seconds since 1970 UTC, 0 if not a plausible date. */
static inline int64_t b2l_rtc_to_epoch(const uint8_t d[B2L_RTC_NREGS])
{
    int64_t s = d[0] & 0x7f, mi = d[1] & 0x7f, h = d[2] & 0x1f, day = d[4] & 0x1f, mon = d[5] & 0x0f;
    int64_t y = 2000 + (d[6] & 0x7f);
    if (s > 59 || mi > 59 || h > 23 || day < 1 || day > 31 || mon < 1 || mon > 12) return 0;
    /* days from civil (Howard Hinnant) */
    y -= mon <= 2;
    int64_t era = y / 400, yoe = y - era * 400;
    int64_t doy = (153 * (mon + (mon > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int64_t days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return days * 86400 + h * 3600 + mi * 60 + s;
}
#endif
