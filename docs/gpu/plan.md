# GPU plan (Mali-G76 MP12): reference document

Created 2026-11-05, after B1L-0027 (apps, Control Center and the provisional background work on CPU rendering).
This file is the GPU roadmap. Each GPU build refers back to it, and its checkboxes are updated as tests are run.
Owner's decision: **stop investing in CPU rendering for Liquid Glass and the effects**; everything goes through the GPU.

## 1. Where we are (already done)

| Step | Build | Result |
| --- | --- | --- |
| G0: choice of route | doc `docs/gpu/g0.md` | iOS 26's CoreAnimation still has `CA::OGL::GLESContext` (OpenGL ES engine) → EAGL route + Mesa Panfrost |
| G1: reading the ID | B1L-0014 | `GPU_ID 0x72110000` (G76), `SHADER_PRESENT 0x770077` (12 cores), AS 0xff, JS 7 |
| G2a: reset and power | B1L-0016 | soft reset, L2 / tiler / shaders switched on and off, `pd-embedded_g3d` power domain already on at boot |
| G2b: first job | B1L-0017 | WRITE_VALUE job through an AArch64 4K page table on AS0: `result=b21cafe5` |

Code: `xnu-overlay/iokit/Kernel/arm/B2LCPU.cpp` (`b2l_gpu_probe`, `b2l_mali_first_job`) and `B2LMali.h`.
Kill switches: `b2l_gpu`, `b2l_gpu_job`.

## 2. The target chain

```
CoreAnimation (backboardd, CA::OGL::GLESContext)            ← current rendering: CA::OGL::SWContext (CPU)
  └─ EAGL bridge in backboardd (libb2l-gl.dylib)            ← HakiOS userland/src/eagl.c (MIT)
       └─ libOSMesa + Mesa 25.0.7 Panfrost, Bifrost compiler   ← HakiOS tools/mesa (darwin patches)
            └─ replacement libdrm → IOKit               ← HakiOS tools/mesa/hakios-drm
                 └─ kernel service B2LMali: panfrost 1.2 DRM interface   ← HakiOS drivers/gpu/hakios.mali.{c,cpp}
                      └─ Mali-G76: MMU AS0, job slots 0/1, IRQ or polling
```

Local references (`~/Developer/s10-port/ref-scion`):
- **HakiOS** (`hackios-xnu-kernel` and `hakios-starlte/GPU.md`). Written for the S9's Mali-G72, from the same Bifrost JM family as our G76. Never tested on their phone, but the interface was checked against Mesa.
- **herolte** (`ios-a5xelte`, S7, Mali-T880 Midgard, iOS 16.7). This chain **actually runs** there, at 35-38 frames/s. It is the best proof that the method works.

## 3. Liquid Glass: what we know

- The filters `CA::OGL::GlassBackgroundFilter` / `GlassForegroundFilter` and the nodes `SDFNode` / `SDFGaussianNode` are in the **common** layer `CA::OGL`, shared by the Metal, GLES and software engines.
- `GlassBackgroundFilter::can_render` (disassembled) does **not** check the engine type. It only tests one field of the graphics state (`[gstate+0x10] == 0`).
- **Working hypothesis:** the GLES engine can draw Liquid Glass. To be confirmed at step G6. The S7 (iOS 16) had the Control Center without blur, but Liquid Glass did not exist yet.
- **Plan B if G6 shows that the glass does not draw in GLES:** Metal through Gallium (HakiOS `hakiosmetal_air.m` + `tools/mesa/air`, AIR → NIR → Bifrost compiler). This is a long project, not before GLES works.

## 4. Steps

One step = one build and one flash (BOOT), with its kill switches. The userspace steps also need files in the root (see §5).

| # | Step | Content | Success criterion (TWRP log) | Status |
| --- | --- | --- | --- | --- |
| G3 | Kernel service B2LMali | Port of `hakios.mali.c/.cpp`: GPU address allocator 32 MiB–4 GiB (`hakios.mali_va.h`), BO create / mmap / close, IOSurface import (0x7e), SUBMIT, WAIT_BO, GET_PARAM from our G76 registers. MMU faults read and logged. Kill switch `b2l_mali=0` | `B2LMali started`, service visible in the IORegistry, the kernel self-test passes through the BO path | **done (B1L-0029): 2 jobs `0xc0ffee01`, HakiosMali started** |
| G3b | Interrupts | Route the Mali SPIs through the GIC (job, MMU, GPU), instead of polling | jobs finish by IRQ, no more polling delay | to do |
| G4 | GPU frequency | G3D DVFS through ACPM, like for the CPUs (index and fvmap table of the G3D). Proposed default: one step below the max (see question 3) | measured frequency stable, no fault | **done (B1L-0030): 676 MHz via ACPM, jobs OK** |
| G5 | Mesa for iOS | Mesa 25.0.7, `-Dgallium-drivers=panfrost`, HakiOS darwin patches, hakios-drm, arm64e ad hoc signed: `libOSMesa` + `libb2l-drm` | a test program (`mali_test`: buffer, job, `0xc0ffee01` written by the GPU; then a triangle) | to do |
| G6 | CoreAnimation on the GPU | EAGL bridge injected into backboardd by the kernel (same mechanism as the SpringBoard library: `DYLD_INSERT_LIBRARIES` + AMFI 90 response). Table `eagl_slots.txt` to be **re-derived for iOS 26's OpenGLES** | home screen drawn by the Mali; "hakios-gl:" in the logs; frames/s measured; Liquid Glass visible or not | to do |
| G7 | Display without copy | Display server surfaces drawn directly into scanned memory (zero copy), frame synchronization | smoothness, low CPU at rest | to do |
| G8 | Liquid Glass | If absent at G6: diagnose the refused shader functions; otherwise Metal plan B (§3) | glass effects rendered | to do |

## 4b. Verified for G6 (2026-11-05)

- **The OpenGLES dispatch table did not change between iOS 16.5 and iOS 26**:
  - tool `tools/derive-eagl-slots.py` (in beyond2lte-ios), which reads the `gl*` routines of `OpenGLES.framework` 23G90: `TPIDRRO_EL0+0xf0` → context, entry at `[ctx+off]`, first argument `[ctx+0x20]`;
  - **the 457 positions are identical** to HakiOS's `eagl_slots.txt` (iOS 16.5), including the 73 used by CoreAnimation;
  - record kept in `tools/eagl-slots-ios26-23G90.txt`.
- HakiOS's `eagl.c` bridge therefore applies without re-deriving the table.

## 4c. Finding B1L-0031 (2026-11-05): no GLES fallback in iOS 26's display server

- On the kernel side, everything worked: copy from SYSTEM, injection into backboardd (pid 62), AMFI 90 granted. But `HakiosMali` has **0 clients**: CoreAnimation never called EAGL.
- `CA::WindowServer::AccelServer::renderer()` (iOS 26, in place of `EAGLServer`): `CAMetalContextCreate()` → `new_metal_context`, and **returns NULL without Metal**. No more `[EAGLContext initWithAPI:]` branch.
- `CA_ENABLE_OGL` is read in the `AccelServer` constructor, but its value is thrown away (vestige).
- `GLESContext::GLESContext(void*)` and `gles_new_context` still exist, but only serve the apps' `CARenderer`s (`context_changed`).
- **Two routes:**
  1. replace `AccelServer::renderer()` in backboardd so that it builds a `GLESContext` on the bridge (23G90 addresses, as HakiOS did in 16.5);
  2. a real Metal device above Mesa (Gallium + AIR, HakiOS), long.
- Owner's choice: to be recorded here.

## 5. Known constraints

- **Library size.** Mesa Panfrost weighs several tens of MB. The kernel only publishes files of 4 MiB at most (`b2l_apps_sync_payload`), and BOOT has only about 31 MiB free. Where to put them is question 1.
- **The kernel never replaces an existing file that differs**: each new library version takes a new name (`-vN`).
- **arm64e**: all userspace libraries are compiled as arm64e, like the launcher and `b2l-springboard`. The kernel translates and emulates PAC.
- **Possible freezes**: MMU fault, `JOB_READ_FAULT` (seen on the S7). There must always be a kill switch, a return to CPU rendering without reflashing, and fault logging.
- **Licenses**: HakiOS's kernel drivers are derived from XNU (APSL); the EAGL bridge is under MIT license. We keep their headers and cite the source in each file taken over.

## 6. Owner's decisions (2026-11-05)

1. **Libraries (Mesa, EAGL bridge): SYSTEM partition** (sda25, which already contains the native HFS cache and the journal). The kernel makes them visible to iOS from SYSTEM, without flashing USERDATA.
2. **Liquid Glass: GLES first**, Metal only if the glass does not display at G6.
3. **GPU frequency: almost max, safer** (one step below 702 MHz, ~650 MHz).
4. **HakiOS and herolte code reused directly**, with their licenses and headers.
5. **Requested addition: CPU temperature limit with throttle.** The CPU rises to 76 °C; 55–60 °C is acceptable, 76 °C is not. This is step **T1**, to be done before G3: it also protects all the GPU tests.

## 7. Step T1: CPU thermal throttle (priority)

- Read the TMU sensors of the Exynos 9820 (addresses and calibration in the DTB and the Samsung kernel: zones of the BIG/MID/LITTLE clusters, and G3D `TMU@100A0000`).
- Regulator in the kernel, on the same ACPM path as the current DVFS (B1L-0015):
  - above 65 °C, the frequency of the cluster concerned drops one step per second;
  - below 60 °C, it goes back up one step;
  - at 72 °C, it falls to the floor immediately.
- The same sensors will serve for the GPU at G4.
- Log: `B2L THERMAL: zone=… temp=… cluster=… khz=…` every 10 s and at each change.
- Kill switch: `b2l_thermal=0`. Thresholds adjustable through boot-args.
- Success criterion: under load, the temperature caps around 65 °C instead of 76 °C.
