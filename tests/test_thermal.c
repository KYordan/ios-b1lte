/* Host test for xnu-overlay/iokit/Kernel/arm/B2LThermal.h.
 *   cc -O1 -Wall -Wextra tests/test_thermal.c -o test_thermal && ./test_thermal */
#include <stdio.h>
#include "../xnu-overlay/iokit/Kernel/arm/B2LThermal.h"
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
	/* Exynos 9820 map (Samsung exynos_tmu.c) */
	for (unsigned s = 0; s < 16; s++) CHECK(b2l_tmu_triminfo(s) == s * 4, "trim %u", s);
	CHECK(b2l_tmu_temp_reg(0) == 0x84 && b2l_tmu_temp_shift(0) == 0, "s0");
	CHECK(b2l_tmu_temp_reg(1) == 0x84 && b2l_tmu_temp_shift(1) == 16, "s1");
	CHECK(b2l_tmu_temp_reg(4) == 0x8c && b2l_tmu_temp_shift(4) == 0, "s4");
	CHECK(b2l_tmu_temp_reg(5) == 0x8c && b2l_tmu_temp_shift(5) == 16, "s5");
	CHECK(b2l_tmu_temp_reg(10) == 0x98 && b2l_tmu_temp_shift(10) == 0, "s10");
	CHECK(B2L_TMU_CONTROL == 0x50, "control");
	/* one point, empty fuse: code 55 = 25 C, 145 = 115 C */
	CHECK(b2l_tmu_code_to_mc(55, 0, 0) == 25000, "1pt 25");
	CHECK(b2l_tmu_code_to_mc(145, 0, 0) == 115000, "1pt 115");
	/* two point: 55 at 25, 115 at 85 -> 1 C per code */
	uint32_t tri = 55 | (115u << 9);
	CHECK(b2l_tmu_code_to_mc(95, tri, 1u << 23) == 65000, "2pt 65 got %d", b2l_tmu_code_to_mc(95, tri, 1u << 23));
	/* two point with empty second fuse falls back to one point */
	CHECK(b2l_tmu_code_to_mc(95, 55, 1u << 23) == 65000, "2pt fallback");
	CHECK(!b2l_tmu_reading_ok(0, 30000) && !b2l_tmu_reading_ok(0x1ff, 30000) && b2l_tmu_reading_ok(80, 50000), "ok");
	/* governor: base 1, floor 6, hi 65 lo 60 crit 72 */
	uint32_t lv = 1;
	lv = b2l_thr_step(lv, 1, 6, 66000, 65000, 60000, 72000); CHECK(lv == 2, "hot step %u", lv);
	lv = b2l_thr_step(lv, 1, 6, 63000, 65000, 60000, 72000); CHECK(lv == 2, "hold %u", lv);
	lv = b2l_thr_step(lv, 1, 6, 59000, 65000, 60000, 72000); CHECK(lv == 1, "cool %u", lv);
	lv = b2l_thr_step(lv, 1, 6, 50000, 65000, 60000, 72000); CHECK(lv == 1, "not past base %u", lv);
	lv = b2l_thr_step(lv, 1, 6, 76000, 65000, 60000, 72000); CHECK(lv == 6, "crit %u", lv);
	lv = b2l_thr_step(lv, 1, 6, 70000, 65000, 60000, 72000); CHECK(lv == 6, "floor hold %u", lv);
	for (int i = 0; i < 10; i++) lv = b2l_thr_step(lv, 1, 6, 55000, 65000, 60000, 72000);
	CHECK(lv == 1, "back to base %u", lv);
	/* load limit: an A55-like table, base 1 (1846 MHz) */
	static const uint32_t r[8] = { 2002000, 1846000, 1534000, 1248000, 1053000, 832000, 650000, 455000 };
	uint32_t slow = b2l_load_slowest(r, 8, 400000);
	CHECK(slow == 7, "slowest %u", slow);
	CHECK(b2l_load_slowest(r, 8, 600000) == 6, "slowest 600");
	CHECK(b2l_load_slowest(r, 8, 3000000) == 0, "slowest above top");
	CHECK(b2l_load_level(r, 8, 1, 0, 1, slow, 800, 900) == 7, "idle -> slowest");
	CHECK(b2l_load_level(r, 8, 1, 950, 1, slow, 800, 900) == 1, "jump -> base");
	CHECK(b2l_load_level(r, 8, 7, 950, 1, slow, 800, 900) == 1, "jump from bottom");
	/* 1846 MHz at 50 % busy = 1154 MHz of work at 80 %: slowest covering level is 1248 MHz (3) */
	CHECK(b2l_load_level(r, 8, 1, 500, 1, slow, 800, 900) == 3, "half load %u", b2l_load_level(r, 8, 1, 500, 1, slow, 800, 900));
	/* at 455 MHz and 85 % busy: 483 MHz needed -> 650 MHz (6) */
	CHECK(b2l_load_level(r, 8, 7, 850, 1, slow, 800, 900) == 6, "creep up");
	CHECK(b2l_load_level(r, 8, 1, 0, 1, 4, 800, 900) == 4, "bounded by slowest");
	CHECK(b2l_load_level(r, 8, 1, 0, 9, slow, 800, 900) == 7, "base clamped");
	CHECK(b2l_load_level(r, 0, 1, 0, 1, slow, 800, 900) == 0, "empty table");
	uint32_t n = 0, now = 1;
	now = b2l_load_settle(now, 7, &n, 3); CHECK(now == 1 && n == 1, "settle 1");
	now = b2l_load_settle(now, 7, &n, 3); CHECK(now == 1 && n == 2, "settle 2");
	now = b2l_load_settle(now, 7, &n, 3); CHECK(now == 7 && n == 0, "settle 3 -> down");
	now = b2l_load_settle(now, 6, &n, 3); CHECK(now == 6 && n == 0, "up at once");
	now = b2l_load_settle(now, 7, &n, 3); now = b2l_load_settle(now, 3, &n, 3);
	CHECK(now == 3 && n == 0, "up resets the count");
	printf("checks=%d fails=%d\n", checks, fails);
	return fails != 0;
}
