# Metal on the Mali-G76: the plan

Date: 2026-11-06. Decided by the owner after B1L-0042, see the memory `metal-plan-decisions`.

## Why leave GLES

- B1L-0041 and B1L-0042 do render iOS on the Mali, through Mesa panfrost behind the EAGL bridge of HakiOS. The result is slower and more broken than CPU rendering: wrong colors, no Liquid Glass, stutters.
- In QuartzCore 26, Apple no longer runs the GLES path. An oversight of Apple's (`push_surface`) had to be fixed, and others will follow.
- The iOS 26 display server and Liquid Glass are written for Metal.
- **GLES stays as a fallback:** disabled by default since B1L-0043, `b2l_gl=1` re-enables it.

## What HakiOS does (reference: `~/Developer/s10-port/ref-scion/hakios-xnu-kernel-oos`)

On an Adreno (Snapdragon 7c+), the display server runs on Metal at about 100 frames/s, according to `docs/metal-plan.md` of their repository. Their driver has four parts:

| Part | At HakiOS (Adreno) | License | For us |
| --- | --- | --- | --- |
| A. Compiler | AIR (bitcode of the `.metallib`) → NIR: `tools/mesa/air/*.c` + `air-frontend.patch`, for **Mesa 25.0.7**, then ir3 | MIT | The AIR→NIR front-end is reused as is (same Mesa as ours). The NIR goes to the **Bifrost compiler** instead of ir3. |
| B. The device | `userland/src/HakiosMetalCA.m`: subclass of `_MTLDevice` and all the Metal objects, on top of the C API `hakios_metal.h` (about 130 functions used) | MIT | Reused almost as is. |
| C. The GPU core | `hakios-metal/` (C++): PM4 packets, A6XX registers, msm memory | MIT | **To replace:** same C API `hakios_metal.h`, but on top of **Gallium panfrost** (Mesa), which already builds Mali jobs, compiles to Bifrost and manages memory through HakiosMali. |
| D. Kernel | `IOAcceleratorES` with `MetalPluginName` / `MetalPluginClassName` (boot-arg `metal=1`), GPU requests | APSL | Add these properties to a service in our kernel. HakiosMali already serves Mesa (panfrost DRM interface). |

**Choice for part C: Metal on top of Gallium rather than hand-written Mali jobs.**
- The NIR coming from the AIR goes to `pipe_context->create_fs_state/create_vs_state/create_compute_state` (`PIPE_SHADER_IR_NIR`).
- Metal commands become Gallium calls: `draw_vbo`, `launch_grid`, `blit`, `clear`, `set_framebuffer_state`, sampler views, etc.
- Gallium panfrost already runs on the phone (B1L-0041/0042: 9184 job chains without failure).

## Steps (test first, display server afterwards)

| # | Step | Verified by |
| --- | --- | --- |
| M1 | The kernel publishes `MetalPluginName=B2LMetal` and `MetalPluginClassName=B2LMetalDevice` on an `IOAcceleratorES` service (disabled by default, `b2l_metal=1`). The `B2LMetal.bundle` bundle (HakiOS plug-in) is on SYSTEM. | `metal_test` on the phone: `MTLCreateSystemDefaultDevice()` returns our class. |
| M2 | Buffers: `hakios_metal_buffer_*` on top of `pipe_resource` (panfrost) | Write then read back of a buffer |
| M3 | Compiler: HakiOS's AIR front-end in our Mesa build; compute through `launch_grid` | A compute kernel gives the expected values |
| M4 | Rendering: render pipeline and encoder, a triangle in a texture | Pixel readback |
| M5 | IOSurface: texture on the memory of a surface (request `0x7e` of HakiosMali, framebuffer included since B1L-0042) | Triangle visible in an IOSurface |
| M6 | Display server: QuartzCore chooses Metal, with the shaders of its `default.metallib` (to be checked in 23G90, see below) | Lock screen, then measured smoothness |

## Open questions

- **Does QuartzCore 23G90 have its `default.metallib`?** HakiOS (iOS 16.5) had to make it (`tools/ca-metallib.py`). To be checked in `ipsw-23G90/OS.dmg`.
- **Metal and `pacibsp`/PAC:** the plug-in is compiled as arm64e, like our current dylibs. Same treatment as `libb2l-gl`.
- **Speed:** Bifrost shader compilation at first use. A shader cache on disk (Mesa's disk cache) will probably be needed.

## Flashes

For this project, the owner authorized BOOT and SYSTEM flashes without prior request, with all the usual safeguards.
