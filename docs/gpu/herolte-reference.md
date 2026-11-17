# Reference: GPU of the Galaxy S7 port (`wardevsa/ios-a5xelte`, branch `herolte`)

Local clone: `~/Developer/s10-port/ios-herolte` (commit `23669fa`).
Read on 2026-11-03. `[VERIFY]` = to be proven on iOS 26 / S10.

## What the S7 does

iOS 16.7.16 (iPhone X), XNU 8796, Mali-T880 MP12 (Midgard), GPU at 650 MHz,
~35-38 fps. No Metal.

Rendering chain:

```
CoreAnimation (OpenGL ES renderer, CA::OGL::GLESContext)
  -> EAGL bridge (gpu/hakios-gl, gpu/eagl) + hooks in the dyld shared cache (ca_hooks.c)
  -> OSMesa + Mesa Panfrost 25.0.7 (gpu/mesa, cross-compiled iPhoneOS arm64)
  -> replacement libdrm (gpu/mesa/herodrm.c): panfrost ioctls -> IOKit user client
  -> HeroGPU kernel service (patches 0225-0267): power domain, clocks, MMU/AS, job slots
```

GPU kernel patches, in the order they were found:
- 0225 probe (PMU G3D, CMU, GPU ID); 0226-0234 first jobs, from clear to textured quad;
- 0235-0237 presentation via GPU and scaling;
- 0239 user client; 0247-0249 barriers, surface BO, VA fault recovery;
- 0256/0264-0267 factory frequency and voltage (the major gain);
- 0258/0266/0273 recovery of `JOB_READ_FAULT`.

Other measured gains that can be reused as is:
- Moving `BTAudioHALPlugin`: idle CPU goes from >300 % to <25 % (out of 800 %).
- Reduced logical resolution (750×1334 @2x, scaled up by the GPU): 72 % fewer pixels.
- Apps copied into the root image, greedy daemons sent to the small cores.

## What transfers to the S10 (Mali-G76 MP12, Exynos 9820)

| Element | Verdict |
| --- | --- |
| Kernel job service (Job Manager model) | **Transferable.** The G76 (Bifrost) is still a "JM" GPU, not CSF: same job slots, MMU/AS and GPU/JOB/MMU IRQs. The addresses change. |
| Mesa Panfrost | **Transferable.** Panfrost handles Bifrost v7 (G72/G76). The Bifrost compiler must be used instead of Midgard in `build-mesa.sh`. |
| `herodrm.c` (libdrm → IOKit) | **Reusable** almost as is, with an equivalent user client in the S10 kernel. |
| T880 quirks, Midgard shaders (`herolte-midgard-shader.py`) | Specific to Midgard, to be thrown away. |
| EAGL bridge + `ca_hooks.c` | **Blocking point** `[VERIFY]`: depends on CoreAnimation's OpenGL ES renderer in iOS 16. Check in the iOS 26 shared cache whether `CA::OGL::GLESContext` still exists. If not, the only path is Metal → Vulkan (dada). The cache addresses are specific to 16.7.16. |
| "GPU scaling" step (0235/0236) | **The most realistic in the short term**: useful even without OpenGL ES or Metal on the iOS side. |
| Factory frequency/voltage | The method applies; the Exynos 9820 tables are to be taken from the DTB and the Samsung kernel. |

## Exynos 9820 addresses (base DTB, `beyond2lte-uefi/dtb-analiz`)

- `mali@18500000`, size 0x5000, SPI IRQ 0x120 / 0x11f / 0x121 (job / mmu / gpu `[VERIFY]` order).
- `g3d_genpd_name = "pd-embedded_g3d"`, `g3d_cmu_cal_id = 0xb04000a`.
- To compare with the S7: PMU `0x105c0000`, CMU `0x14aa0000`, GPU `0x14ac0000` (Exynos 8890).

## Mapping to the plan (phase 6)

1. Power the G76 and read its ID: adapt 0225 (power domain `pd-embedded_g3d`).
2. Kernel test job: adapt 0226-0234 with Bifrost descriptors.
3. GPU scaling: adapt 0235/0236.
4. Mesa for iOS: `build-mesa.sh` with `-Dgallium-drivers=panfrost`, Bifrost compiler.
5. iOS rendering: EAGL if iOS 26 still has CA's GLES renderer, otherwise Metal → Vulkan (PanVK).
