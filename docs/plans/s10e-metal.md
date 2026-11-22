# Plan: Metal (GPU) on the Galaxy S10e

Agreed 2026-11-22. Its precondition is met: power off, battery and display sleep work since build 17, so this is
the next large step (after the small storage-size fix). The S10's history is in
[docs/gpu/metal-progress.md](../gpu/metal-progress.md); the S10's speed plan in [speed-and-m6.md](speed-and-m6.md).

## Where the S10e stands (boot 12 log)

| Point | State |
| --- | --- |
| Mali-G76 hardware | **Works.** `HakiosMali` brings it up: supply, clock, reset, `GPU_ID 0x72110000`, 12 cores powered, a job in identity mode and one through page tables both complete. The log line says "Mali-G72": a label left from the S9 code, the ID is the G76's |
| iOS side | **Absent.** `B2L METAL: publication error=2`: the SYSTEM image has no Metal set, the kernel looks for `b2l-metal-v95`. `mali: … 0 clients, 0 pictures shown` |
| PAC emulation | **No cost any more**: `B2L EMU PERF: samples=0`. The dyld cache is translated on Linux; on the S10 the traps (20,000–80,000 a second) were the main brake |
| Presentation | CPU path: backboardd composites on the CPU and `B2LFramebuffer` scales 880 × 1912 to 1080 × 2280 with a CPU copy each frame (about 10 MB written). Zero copy is off because the canvas is scaled |
| Speed today | 7–20 frames/s, about 7 on average (`B2L PROF` swaps) |

What the same code reached on the S10: the whole interface drawn by the Mali, **Liquid Glass correct** (B1L-0128),
zero copy, an automatic return to CPU rendering if backboardd crashes twice. Still open there: Clock/Calendar icons,
widgets and some glyphs drawn by RenderBox (SwiftUI) inside SpringBoard.

## Steps

### 1. Metal userland built on GitHub Actions

- A job on the macOS runner (Xcode and the iOS SDK are on it) builds Mesa (panfrost, Bifrost compiler), the Scion
  Metal driver and the Metal plug-in (`scripts/build-mesa.sh`, `scripts/build-metal-scion.sh`, `userspace/metal`).
- Output: the `b2l-metal-v95` set the kernel publishes (`b2l_exec_graphics_diag.h`). Our code and Mesa only, no Apple
  binary; still in the encrypted artifact, like the kernel.
- Done when: the set builds in CI and its file list matches what the kernel publishes.
- **Written (build 18):** [build-graphics.yml](../../.github/workflows/build-graphics.yml) builds Mesa (release,
  Scion patches at `a6f4bc38`), `b2l-metal-v5` (B2LMetalV3), `b2l-metal-v95` (B2LMetalv95) and `b2l-gl-v9` with the S10's
  scripts, then [check-graphics-set.py](../../scripts/check-graphics-set.py) compares the output with the 12 files
  `b2l_userspace_seed.h` publishes and checks every `otool -L` dependency. Artifact `b2l-graphics-N`, encrypted.
  Not run yet. The build 17 log shows why it matters: `B2LMetalV3` is the first file the kernel looks for, so the
  whole publication stops there (`error=2`).

### 2. The set on SYSTEM, backboardd on the GPU

- `tools/b1l/linux/build-system-image.sh` copies the set into SYSTEM next to the native cache
  (`GRAPHICS_DIR=<decrypted b2l-graphics artifact>`, with `WIFI_DIR` as before, since SYSTEM is rebuilt whole).
- SYSTEM (`sda25`) rewritten once; USERDATA (the iOS data) unchanged.
- Nothing to switch on in the kernel: `b2l_metal_ui` is already on by default (`B2LMetalAccel.cpp` announces
  `B2LMetalv95`); the plug-in is simply missing today. `b2l_metal_ui=0` stays the kill switch, and backboardd falls
  back to CPU rendering by itself after two crashes.
- Done when: `mali: … N clients … pictures shown` grows, the lock and home screens are drawn by the GPU.
- Expected: about 10–20 frames/s, but the CPU freed, Liquid Glass and blur correct. Known risk: RenderBox icons and
  widgets as on the S10.

### 3. Scaling in the display hardware (S10e only)

- The Exynos 9820 display pre-processor (DPP) has a scaler: program it to stretch the 880 × 1912 surface to the
  1080 × 2280 panel, so the GPU's frame is shown without any CPU copy (zero copy with scaling). Apple does the same
  on the iPhone mini (renders larger, the display pipe scales).
- Registers from Samsung's Exynos 9820 DPP driver (LineageOS kernel, hardware reference only), as for ACPM and PMIC.
- The CPU-scaled copy stays as the fallback.
- Done when: `zero copy` active with `fScaled`, no `B2L_PERF_BLIT` time per frame.
- Expected: 25–40 frames/s while scrolling.

### 4. Speed

- On-disk shader cache (no compilation stutter), fewer waits between frames, per-draw overhead (the S10's
  0.03–0.05 ms per draw is the reference).

## Limits

- 60/120 frames/s are not expected: CoreAnimation relies on Apple GPU features the Mali lacks (tile memory,
  imageblocks), and every shader goes through AIR → NIR → Bifrost.
- RenderBox (SwiftUI) drawing in SpringBoard was not solved on the S10 (B1L-0185 to 0238): some icons and widgets may
  stay wrong.
- Each step keeps a way back: CPU rendering, the CPU-scaled copy, and the SYSTEM image of the previous step.
