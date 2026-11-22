#!/usr/bin/env python3
"""Check b2l_blit_scaled against a per-pixel nearest-neighbour reference (ASan/UBSan)."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[1]
code = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "B2LBlit.h"
static uint32_t pick(uint32_t p, uint32_t l, uint32_t d, uint32_t lo, uint32_t n) {
  uint32_t v = (uint32_t)((uint64_t)p * l / d);
  return v < lo ? lo : (v > lo + n - 1 ? lo + n - 1 : v);
}
static void run(uint32_t dw, uint32_t dh, uint32_t lw, uint32_t lh,
                int32_t sx, int32_t sy, int32_t dx, int32_t dy, int32_t w, int32_t h) {
  uint32_t sw = lw + 16, sh = lh + 8, ss = sw * 4 + 64, soff = 128, ds = dw * 4 + 32;
  std::vector<uint8_t> src(soff + (size_t)ss * sh), dst((size_t)ds * dh, 0xAB), ref(dst);
  for (uint32_t y = 0; y < sh; ++y) for (uint32_t x = 0; x < sw; ++x) {
    uint32_t v = 0xff000000u | (y << 12) | x; memcpy(&src[soff + (size_t)y * ss + x * 4], &v, 4);
  }
  assert(b2l_blit_scaled(dst.data(), dst.size(), dw, dh, ds, src.data(), src.size(), sw, sh, ss, soff,
                         lw, lh, sx, sy, dx, dy, w, h));
  uint32_t px0 = (uint64_t)dx * dw / lw, py0 = (uint64_t)dy * dh / lh;
  uint32_t px1 = ((uint64_t)(dx + w) * dw + lw - 1) / lw, py1 = ((uint64_t)(dy + h) * dh + lh - 1) / lh;
  for (uint32_t py = py0; py < py1; ++py) for (uint32_t px = px0; px < px1; ++px) {
    uint32_t ly = pick(py, lh, dh, dy, h), lx = pick(px, lw, dw, dx, w);
    memcpy(&ref[(size_t)py * ds + px * 4], &src[soff + (size_t)(sy + ly - dy) * ss + (sx + lx - dx) * 4], 4);
  }
  assert(dst == ref);
  // S10e build 24: the scratch path (row read once, column table, no panel read-back) gives the same pixels,
  // and a scratch buffer one word short falls back to the path above.
  uint64_t words = (uint64_t)w + 2ull * (px1 - px0);
  for (uint64_t have : {words, words - 1, words + 7}) {
    std::vector<uint8_t> d2((size_t)ds * dh, 0xAB);
    std::vector<uint32_t> scratch((size_t)have + 1, 0xdeadbeef);
    assert(b2l_blit_scaled_scratch(d2.data(), d2.size(), dw, dh, ds, src.data(), src.size(), sw, sh, ss, soff,
                                   lw, lh, sx, sy, dx, dy, w, h, scratch.data(), have));
    assert(d2 == ref);
    assert(scratch[(size_t)have] == 0xdeadbeef);   // nothing written past the words it was given
  }
}
int main() {
  run(1080, 2280, 880, 1912, 0, 0, 0, 0, 880, 1912);    // S10e full frame
  run(1080, 2280, 880, 1912, 7, 9, 40, 600, 500, 300);  // S10e partial layer
  run(1440, 3040, 880, 1912, 0, 0, 0, 0, 880, 1912);    // B1L full frame
  run(1440, 3040, 960, 2026, 0, 0, 0, 0, 960, 2026);    // 2/3 alternative
  run(1440, 3040, 880, 1912, 3, 5, 100, 200, 300, 400); // partial layer
  run(1440, 3040, 1440, 3040, 0, 0, 0, 0, 1440, 3040);  // identity
  run(37, 53, 11, 29, 1, 2, 4, 7, 6, 20);               // odd ratios
  std::vector<uint8_t> s(4096), d(4096);
  // Rejected: logical larger than panel, destination outside, short allocations.
  assert(!b2l_blit_scaled(d.data(), d.size(), 8, 8, 32, s.data(), s.size(), 8, 8, 32, 0, 9, 8, 0, 0, 0, 0, 8, 8));
  assert(!b2l_blit_scaled(d.data(), d.size(), 8, 8, 32, s.data(), s.size(), 8, 8, 32, 0, 4, 4, 0, 0, 4, 0, 1, 1));
  assert(!b2l_blit_scaled(d.data(), 100, 8, 8, 32, s.data(), s.size(), 8, 8, 32, 0, 4, 4, 0, 0, 0, 0, 4, 4));
  assert(!b2l_blit_scaled(d.data(), d.size(), 8, 8, 32, s.data(), 100, 8, 8, 32, 0, 4, 4, 0, 0, 0, 0, 4, 4));
  puts("Scaled CPU presentation: full frame, 2/3, partial, identity, odd ratios and bounds rejection passed");
}
'''
with tempfile.TemporaryDirectory() as td:
    c = Path(td) / 't.cpp'; c.write_text(code); exe = Path(td) / 't'
    subprocess.run(['clang++', '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I', str(root / 'xnu-overlay/iokit/Kernel/arm'), str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
