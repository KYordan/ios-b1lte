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
  assert(b2l_bl_nits_from_raw(300) == 300);                // nits
  assert(b2l_bl_nits_from_raw(300u << 16) == 300);         // 16.16
  assert(b2l_bl_nits_from_raw(250000) == 250);             // millinits
  assert(b2l_bl_nits_from_raw(5000000) == 76);             // large: 16.16 (5000000 >> 16)
  puts("backlight math: AOR range, monotonic, half luminance, nits/16.16/millinits passed");
}
'''
with tempfile.TemporaryDirectory() as td:
    c = Path(td) / 't.cpp'; c.write_text(code); exe = Path(td) / 't'
    subprocess.run(['clang++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I', str(root / 'xnu-overlay/iokit/Kernel/arm'), str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
