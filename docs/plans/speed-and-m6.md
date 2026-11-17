# Plan: Metal driver speed, then M6 (interface on the GPU)

Date: 2026-11-07. Decided with the owner, after B1L-0075.

## Decisions

| Question | Choice |
| --- | --- |
| Priority after B1L-0075 | Finish driver speed before touching the interface |
| Cost of Apple's PAC code | Patch at load time by the kernel, **after** M6 (corecrypto excluded) |
| Entry into M6 | A test app with a `CAMetalLayer` first, backboardd afterwards |
| Success criteria | 40 frames/s while scrolling on the home screen, correct Liquid Glass, faster than CPU rendering |

The on-disk shader cache is not a success criterion. It remains planned in M6 (step 6), because it avoids stutters.

## Step 1: driver speed (target: ~0.2 ms per draw)

Reference measurement: the `M5 bench` line (200 draws in one pass, image verified against the Mac).

| Build | Change | Expected measurement |
| --- | --- | --- |
| B1L-0075 (flashed) | Mesa and driver without signed returns (`-fno-ptrauth-returns`, 0 `retab`) | Time per draw (1.2 to 3.4 ms before) |
| B1L-0076 | Without signed calls (`-fno-ptrauth-calls`): removes the 3067 `blraaz`. Safe, because the kernel already accepts unsigned pointers | What the signed calls cost |
| B1L-0077 | Stubs toward system functions (`__auth_stubs`): check whether the linker can produce unsigned stubs | What remains of calls to libSystem |
| B1L-0078 | No useless relinking: a shader, constants or buffers unchanged since the previous draw are not relinked | The cost specific to Mesa and panfrost |
| (if needed) | Profile of one draw: process PAC trap counters during measurement, time per function | The next slow point |

**Criterion to move to step 2:** about 0.2 to 0.3 ms per draw, or a quantified explanation of what remains, if it is in Apple's code.

## Step 2: M6a, a test app on screen

- A small app with a full-screen `CAMetalLayer`, displayed by the current display server (CPU rendering):
  - it draws a simple animated image with the QuartzCore shaders;
  - it counts its frames per second.
- **What it validates:**
  - the real on-screen presentation (`presentDrawable` → CoreAnimation image queue → compositor → screen);
  - the kernel's wait for the end of the GPU before display (`HakiosMaliDrawn`, `maliSeq`);
  - the real frames per second of a Metal app.
- **Low risk:** the interface stays on CPU rendering.

## Step 3: M6b, backboardd on the GPU, behind a switch

- A boot setting, disabled by default, lets backboardd's QuartzCore see the Mali. Normal boot stays on CPU rendering.
- **To handle, based on what we have already seen:**
  - the 189 QuartzCore shaders, including `fixed_frag_lph_cpf` and its sixty or so *function constants*;
  - Apple's precompiled pipeline archive, to be refused cleanly;
  - `newTextureWithDescriptor:iosurface:plane:slice:` (still an Apple stub, which aborts);
  - the formats the compositor requests, for example YCbCr 420 in two planes for video.
- Then: lock screen, then home screen.

## Step 4: Liquid Glass

- Blurs and glass (`downsample`, `upsample` and `variable_blur` passes of QuartzCore).
- Visual check: Control Center, dock and bars.

## Step 5: measurement against the criteria

- Frames per second while scrolling on the home screen (target: 40).
- Comparison on the same screens between GPU and CPU (switching is done with the step 3 switch).

## Step 6: PAC patch of Apple's code at load time (after M6)

- The kernel replaces, in the code pages, at the moment they are loaded:
  - `retab` → `ret`;
  - `blraaz` → `blr`;
  - `aut*` → NOP.

  This extends the existing native cache, whose rejections ("mismatch") must be corrected.
- Nothing is modified on disk, and corecrypto is excluded (it verifies the integrity of its own code).
- **Expected effect:** a strong drop in the system's ~51,000 traps per second, for all of iOS and not only the GPU.
- On-disk shader cache, against stutters from compilation (0.5 to 1 s per new shader).

## Unchanged rules

- One change per build, with its notes `docs/journal/build-b1l-NNNN.md`.
- `metal-progress.md` updated after each build and each test.
- Flash without asking, for Metal work, with all the guards (TWRP root, model, size, hash, re-read).
