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
	printf("checks=%d fails=%d\n", checks, fails);
	return fails != 0;
}
