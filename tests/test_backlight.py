#!/usr/bin/env python3
"""S10e build 25: B2LBacklightMath.h (nits -> S6E3FA7 AOR, iOS value -> nits), ASan/UBSan."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[1]
code = r'''
#include <cassert>
#include <cstdio>
#include "B2LBacklightMath.h"
int main() {
  assert(b2l_bl_aor_for_nits(420) == 0x0014 && b2l_bl_aor_for_nits(5000) == 0x0014);   // Samsung's 420-nit entry
  assert(b2l_bl_aor_for_nits(0) == 0x08D4 && b2l_bl_aor_for_nits(2) == 0x08D4);         // clamped at its darkest
  uint32_t last = 0;
  for (uint32_t n = 420; n-- > 0;) {                                                     // darker never brighter
    uint32_t a = b2l_bl_aor_for_nits(n);
    assert(a >= 0x0014 && a <= 0x08D4 && a >= last);
    last = a;
  }
  // half the luminance: about half the lines of the frame lit
  uint32_t half = 2304 - b2l_bl_aor_for_nits(210);
  assert(half >= 1135 && half <= 1146);
  assert(b2l_bl_nits16_from_raw(300) == 300u << 16);       // plain nits
  assert(b2l_bl_nits16_from_raw(11365869) == 11365869);    // build 26: 173.4 nits
  assert(b2l_bl_nits16_from_raw(672751) == 672751);        // build 26: 10.3 nits (was misread as 672 millinits->nits)
  assert(b2l_bl_nits16_from_raw(27525120) == 420u << 16);  // the cap
  assert(b2l_bl_nits16_from_raw(1ull << 40) == 420u << 16);
  // 16.16: every iOS step below 420 nits maps monotonically, low end darker than 15 nits
  uint32_t prev = 0;
  for (uint64_t raw = 27525120; raw > 2000; raw -= 4099) {
    uint32_t a = b2l_bl_aor_for_nits16(b2l_bl_nits16_from_raw(raw));
    assert(a >= prev);
    prev = a;
  }
  assert(b2l_bl_aor_for_nits16(672751) > b2l_bl_aor_for_nits(15));
  assert(b2l_bl_aor_for_nits16(10u << 16) == b2l_bl_aor_for_nits(10));
  puts("backlight math: AOR range, monotonic, half luminance, 16.16 nits passed");
}
'''
with tempfile.TemporaryDirectory() as td:
    c = Path(td) / 't.cpp'; c.write_text(code); exe = Path(td) / 't'
    subprocess.run(['clang++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I', str(root / 'xnu-overlay/iokit/Kernel/arm'), str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
