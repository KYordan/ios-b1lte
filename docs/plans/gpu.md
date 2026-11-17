# GPU acceleration plan — Mali-G76 MP12 (Exynos 9820), iOS 26 on Galaxy S10

Status 2026-11-04: **G0 decided (GLES renderer present, see docs/gpu-g0.md); G1 passed in B1L-0015** (G3D domain already on, GPU_ID 0x72110000 Mali-G76, 12 shader cores, 8 AS, 3 job slots). All rendering is CPU
(QuartzCore software → IOSurface → `B2LFramebuffer` copy to scanout). `[VERIFY]` = to prove on the device or in the image.

Rule: each stage is a separate build (`docs/journal/build-b1l-XXXX.md`), is useful on its own, and has a pass/fail gate.
Stages G1–G4 are porting work with known references. G6–G7 are research with no guarantee.

## Known facts

| Item | Value | Source |
| --- | --- | --- |
| GPU node | `mali@18500000`, reg size `0x5000`, `compatible = "arm,mali"` | Exynos 9820 base DTB (`beyond2lte-uefi/dtb-analiz`), same on S10 |
| Interrupts | SPI `0x120`, `0x11f`, `0x121` (job/mmu/gpu order `[VERIFY]`) | DTB |
| Power / clock | `g3d_genpd_name = "pd-embedded_g3d"`, `g3d_cmu_cal_id = 0xb04000a` | DTB |
| Architecture | Bifrost v7, **Job Manager** GPU (not CSF): same submission model as the S7's Midgard T880 | ARM / Mesa |
| iOS 26 GLES traces | `CA::OGL::GLESImage`, `…GLESContext` typeinfo, `OpenGLES.framework`, `CAEAGLLayer` still in the `23G90` cache | `grep` of `static-work/orig` |
| S7 reference | Kernel job service, Panfrost + EAGL bridge, ~35–38 fps at 650 MHz | `ios-herolte` patches 0225–0267, `gpu/` |

The CPU stays the main limit (arm64e emulation and the iOS code itself). GPU work does not replace the static
translation (`b1l-tools/static-translate`), which has priority for overall fluidity.

## G0 — Decisions before writing code (no device change)

1. **Is CoreAnimation's GLES renderer still usable in iOS 26?** Extract QuartzCore from the cache and look for the
   `CA::OGL::GLESContext` vtable, its renderer-selection code (what makes CA pick OGL over Metal), and the EAGL entry points.
   - Present and selectable: the **EAGL path** (S7 method) is the main route.
   - Absent or dead: only the Metal route remains (G6–G7).
2. **Does Mesa support Bifrost v7 well enough?** Check the current Mesa release for Panfrost (Gallium, GLES 3.1 on G76)
   and PanVK (Vulkan on Bifrost). Write down the exact Mesa version and its feature level.
3. Record the S10's GPU DVFS table and voltages (Samsung kernel source and DTB) for G4. Never guess a voltage.

Gate: a written decision with evidence, in `docs/gpu-g0.md`.

## G1 — Power the G76 and read its ID (kernel, BOOT only)

- New B2L kernel service, ported from `0225-herolte-mali-t880-probe.patch`: map `0x18500000`, bring up `pd-embedded_g3d`
  and the G3D clock using only DTB values, then read `GPU_ID`, `L2_FEATURES`, `SHADER_PRESENT`, `TILER_PRESENT`, `AS_PRESENT`, `JS_PRESENT`.
- Power down cleanly at the end of the probe. No userspace yet.
- Gate: the log shows a G76 product ID and 12 shader cores, with no panic and no hang. Rollback: previous BOOT.

## G2 — First kernel test job (BOOT only)

- Address space (MMU/AS) setup, a page-table walker for one GPU VA range, one job slot, and IRQ handling.
- Jobs as in the S7 sequence (0226–0234), rewritten for **Bifrost descriptors**: clear, then fragment fill, then a copy into a buffer.
  Midgard descriptors and the T880 quirks do not apply.
- Fault recovery from day one: `JOB_READ_FAULT`, MMU faults, soft and hard reset, as in S7 0258/0266/0273.
- Gate: the result buffer matches an expected checksum. 100 jobs in a row without a fault.

## G3 — GPU scaling and composition (BOOT only, useful without any iOS change)

- Replace `b2l_blit_scaled` (CPU nearest-neighbour from B1L-0005) with a GPU blit: logical 880×1912 to the 1440×3040 panel,
  bilinear. The CPU path stays as a fallback (S7 `sweetfb_gpu`).
- Then compose the swap layers (up to 4) on the GPU instead of copying each one.
- Gate: same image as the CPU path (checksum on a test pattern). Measure CPU time per frame and fps before and after.
  This is the first stage with a **real user-visible gain**.

## G4 — Clocks, voltages and stability (BOOT only)

- Factory GPU frequencies at factory voltages from G0.3, plus basic DVFS and a temperature reading. This was the main GPU gain on the S7.
- Gate: 30 minutes of G3 composition without faults; the temperature is logged.

## G5 — GPU user client and Mesa for iOS (BOOT + root files)

- Kernel IOKit user client equivalent to the S7 `HeroGPU`: create, map and free buffer objects, submit, wait, synchronize surfaces.
  Every new kernel buffer path gets bounds checks, as for `b2l_blit`.
- Port `gpu/mesa/herodrm.c` (libdrm → IOKit) and `build-mesa.sh`, with the **Bifrost** compiler, built for the iPhoneOS 26 SDK as arm64e.
- Install the `.dylib` files into the live root copy (owner-preserving script), and write back only the changed chunks.
- Gate: a host-built `osmesa-test` run on the phone renders a known image through the G76.

## G6 — CoreAnimation on the GPU (research)

Choose from the G0 result:

- **EAGL path** (if GLES CA is selectable): adapt `gpu/hakios-gl` and `gpu/eagl` (S7). The `ca_hooks.c` addresses are for
  iOS 16.7; find them again in `23G90` with signature searches and never hardcode them unverified. Gate: SpringBoard draws through GLES, with the lock screen correct.
- **Metal path** (if not): Metal → Vulkan (dada `metal2vulkan`) → PanVK. This needs AIR → SPIR-V translation, a fake Metal device
  that iOS 26 accepts (IOGPU interfaces), and Vulkan features the G76 may lack. No one has published this on Mali.
  Gate: a minimal Metal app clears the screen through the G76.

## G7 — Wider use (research)

WebKit, apps, video. Only after G6 is stable.

## Risks

- Wrong power or voltage sequencing: this can hang the board. Use DTB and Samsung source values only, and add each one in its own build.
- GPU memory faults can stall the GPU until reboot (seen on the S7). Recovery is required from G2 on.
- Heat: no thermal limit exists yet (G4 is where it starts). Never run GPU tests unattended.
- Storage: G1–G4 change BOOT only. G5+ add root files with the live-root rules in `engineer.md`, section 2.

## Order relative to the rest

1. Static translation of the root (in progress, B1L-0006), plus the B1L-0005 display scaling.
2. G0, then G1–G4 (kernel only, safe to roll back).
3. G5, then G6, depending on the G0 decision.
