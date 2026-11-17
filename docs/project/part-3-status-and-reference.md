# Project documentation — iOS 26 port to the Galaxy S10 (beyond1lte)

## Part 3 — Build logs, current status, GPU plan and reference

---

### 11. Build logs (`docs/journal/build-b1l-XXXX.md`)

114 logs exist (0001 to 0139, with gaps). Each log contains: Date, Base, Goal, Changes, Installed (re-read BOOT hash), Result, Rollback.

**Major progress:**

| Build | What changed / result |
|-------|---------------------------|
| **0001** | First XNU boot on the S10, without iOS. 8/8 CPUs verified. Expected panic (no HFS root). |
| **0004** | `BKNoWatchdogs = YES` in SpringBoard preferences (prevents the watchdog from killing Settings). |
| **0005** | Logical screen 880×1912 @2x (440×956 points, iPhone 16 Pro Max size) instead of 960×2026. 2.6× reduction in pixel count. |
| **0014** | CPU DVFS (A55 1846, A75 2210, M4 2080 MHz) + GPU G1 probe (read-only). GPU_ID `0x72110000` (G76), 12 cores. |
| **0015** | CoreAnimation variables injected by the kernel (`CA_DISABLE_ASYNCHRONOUS`). Keyboard works (slow). |
| **0016** | GPU G2a: soft reset, L2/tiler/shaders powered on and off. |
| **0017** | Multi-plane IOSurface + first GPU job (WRITE_VALUE `0xb21cafe5` executed by the Mali). |
| **0028** | Thermal throttling T1 (9820 TMU sensors corrected). |
| **0029** | GPU G3: HakiosMali kernel service started, jobs in identity mode then through page tables. |
| **0030** | T1 fixed + G4: GPU frequency at 676 MHz via ACPM. Chip caps at 62-66 °C instead of 76 °C. |
| **0040** | Low-bank RAM treated as DRAM (fixes a Mesa alignment panic). |
| **0041/0042** | iOS rendered by the GPU (EAGL → Mesa Panfrost → Mali). Slower and more broken than the CPU. |
| **0043** | Back to CPU rendering by default (GLES as fallback, `b2l_gl=1`). Decision to move to Metal. |
| **0044** | `IOAcceleratorES` kernel service announcing a Metal plug-in. |
| **0045** | `MTLCreateSystemDefaultDevice()` returns the GPU. |
| **0047-0049** | Metal crashes fixed, test outside backboardd. |
| **0051** | 256 KB copy and fill done by the Mali, exact to the byte. |
| **0055** | Metal Scion plug-in integrated: 0 missing methods out of 129 (MTLDevice) and 414 (MTLDeviceSPI). `image_calcminmax0` executed with correct results. |
| **0058-0066** | Metal rendering bugs fixed (shader constants, viewport, coverage). QuartzCore drawing correct on the Mali (B1L-0066: 16×16 grid identical to software). |
| **0070-0073** | Metal texture on IOSurface: Mali clears to red then green, read back correctly by the CPU. `CAMetalLayer` works, `presentDrawable` status 4. QuartzCore drawing in the drawable identical to the Mac (0 of 64 lines differ). |
| **0074-0079** | Driver speed: from 8-15 ms to ~0.03-0.05 ms per draw, then 0 PAC traps per draw, 26-56 µs per draw. |
| **0080-0083** | backboardd on the Mali. Crashes fixed (targetDeviceArchitecture, inline samplers, insertelement). `air2nir` tool: 189 QuartzCore shaders verified on the Mac. |
| **0084** | backboardd draws on the Mali for the first time (a 1440×3040 image sent to the screen). |
| **0088** | **iOS interface displayed, rendered by the GPU** (home, Settings, control center, contacts, keyboard). Defects: cyan tint, Liquid Glass without blur, left edge cut off, keyboard keys without background. |
| **0090-0095** | Varyings widened to 32 bits, `demote` → `discard`, `bi_lower_swizzle` fixed. On-disk shader cache. 0 faults 0x51. Zero copy enabled (0 CPU copies). |
| **0096-0098** | No more progressive drawing. Zero copy active (IDMA, contiguous surfaces). Viewport ignored 1126 times. |
| **0099-0103** | Viewport and scissor served. 64-byte lines for SpringBoard. No more large buffers refused (page allocation). Texture views. backboardd no longer burns the CPU (LDAPR). |
| **0104-0112** | GPU buffers ≥ 1 MB allocated by page. Asynchronous waiting for command buffers. App opening crash fixed (0112). 2-7 frames/s, GPU 2-7%. Waiting bottleneck. |
| **0113-0115** | Constants copied only up to where the shader reads them (2-3.6 GB avoided / 5 s). 10-14 frames/s. **"extremely smoother"** (owner). |
| **0116-0120** | Wallpaper drawn (gradient, waves, dots) instead of the flat blue. Back to 0115 (regression from 0118 removed). |
| **0121-0125** | `kmem_data_shared` range at 2999 MB. Copy encoder (blit). Owner's photo as wallpaper. Glass still grey/black. |
| **0126-0128** | **Real Liquid Glass on the Focus button of the Control Center** (0127). **Liquid Glass rendered, photo wallpaper, "works very well"** (0128). Reflashable: `flash-saved.sh 0128`. |
| **0129-0131** | Each attachment has its own load action. Shader inventory: no `path_*`, vibrant glyphs = `fixed_frag` framebuffer fetch. Panfrost reloads the re-read targets: Wi-Fi, AirDrop, cellular, Bluetooth, AirPlay glyphs of the Control Center rendered. |
| **0132** | Images with unaligned lines copied instead of refused. Power-off menu icons repaired. No visible change for the missing glyphs. |
| **0133** | backboardd imports only 3 IOSurfaces (background, brightness, InCallService). `chronod` receives "unsupported" for 4 IOSurface selectors. No glyph or widget surface. |
| **0134** | Kernel accepts the 4 chronod calls. No visible change. 7 IOSurface imports, all large. |
| **0135** | Glyph and icon textures do reach the shaders (23 RGBA8 shapes all sampled). Only 3 imports. The `240x240 LA08` icon snapshots are imported only once, **before** the app draws them. |
| **0136** | Copy of unaligned-line IOSurface re-read before each draw (live icon snapshots). **Finding: the "stale copy" lead is not the cause — the path is never executed.** No `240x240` surface is imported. No `B2L LIVE: surface`. `MobileTimer id 16`, `MobileCal id 26/30`, `Spotlight id 14` do create `240x240 LA08` surfaces, but they are created then abandoned (never looked up, never imported). |
| **0137** | Instrumentation of IOSurface accesses (lookup, create port, metadata) with surface ID and PID. **Finding: no `240x240` surface is ever looked up; the path does not exist.** |
| **0138** | Accepts IOSurface selector 17 for `iconservicesagent`. `accepted 17`, but still no lookup on the `240x240` surfaces. Selector 17 was not the blocker. |
| **0139** (in progress) | Userspace instrumentation (`hakiosmetal_air.m`, set `b2l-metal-v88`) to trace: creation of `240x240` textures, binding to a fragment sampler, upload via `replaceRegion:`. Goal: confirm whether the Metal path does or does not consume the `240x240 LA08` icon snapshots. Result pending. |

**Stable reference build for rollback:** 0128 (set `b2l-metal-v77`), reflashable with `flash-saved.sh 0128`.

---

### 12. GPU plan (Mali-G76 MP12) — `docs/gpu/plan.md`

Owner decision: **stop investing in CPU rendering for Liquid Glass and effects; everything goes through the GPU.**

Target chain:
```
CoreAnimation (backboardd, CA::OGL::GLESContext)
  └─ EAGL bridge in backboardd (libb2l-gl.dylib)
       └─ libOSMesa + Mesa 25.0.7 Panfrost, Bifrost compiler
            └─ replacement libdrm → IOKit
                 └─ B2LMali kernel service: panfrost 1.2 DRM interface
                      └─ Mali-G76: MMU AS0, job slots 0/1, IRQ or polling
```

| Step | Content | Status |
|-------|---------|--------|
| G0 | Choice of path (GLES present in iOS 26) | Done (`docs/gpu/g0.md`) |
| G1 | GPU ID read | Done (B1L-0014/0015) |
| G2a | Reset + core power-up | Done (B1L-0016) |
| G2b | First GPU job | Done (B1L-0017) |
| G3 | B2LMali kernel service | Done (B1L-0029) |
| G3b | Interrupts (GIC instead of polling) | To do |
| G4 | GPU frequency (DVFS via ACPM) | Done (B1L-0030, 676 MHz) |
| G5 | Mesa for iOS | To do |
| G6 | CoreAnimation on the GPU (EAGL bridge) | To do |
| G7 | Display without copy (zero copy) | Partly done (B1L-0096 to 0098) |
| G8 | Liquid Glass | In progress (rendered on Focus button, 0127-0128) |

**Step T1 (priority):** CPU thermal throttling. Done (B1L-0030): chip caps at 62-66 °C instead of 76 °C.

---

### 13. Metal plan — `docs/metal-plan.md`

Decided after B1L-0042 to leave GLES for Metal, because iOS 26's display server and Liquid Glass are written for Metal.

| Step | Content | Status |
|-------|---------|--------|
| M1 | Kernel publishes `MetalPluginName=B2LMetal` / `MetalPluginClassName=B2LMetalDevice` | Done (B1L-0044) |
| M2 | Memory and copies done by the GPU | Done (B1L-0051) |
| M3 | Run Apple's shaders (compute) | Done (B1L-0055) |
| M4 | Draw with Apple's shaders | Done (B1L-0066, correct QuartzCore drawing) |
| M5 | Draw into screen surfaces (IOSurface) | Done (B1L-0070 to 0073) |
| M6 | iOS interface rendered by Metal | In progress (B1L-0088 to 0139) |

**M6 success criteria:** 40 frames/s when scrolling the home screen, correct Liquid Glass, faster than CPU rendering.

---

### 14. GPU reference from the S7 port — `docs/gpu/herolte-reference.md`

The Galaxy S7 port (`wardevsa/ios-a5xelte`, iOS 16.7.16, Mali-T880 Midgard) serves as a reference:
- Chain: CoreAnimation (GLES) → EAGL bridge → OSMesa + Mesa Panfrost → replacement libdrm (`herodrm.c`) → HeroGPU kernel service.
- ~35-38 fps at 650 MHz.
- Parts transferable to the S10: kernel job service (Job Manager), Mesa Panfrost, `herodrm.c`.
- Blocking point: the EAGL bridge + `ca_hooks.c` depends on CoreAnimation's GLES renderer in iOS 16; to be checked in iOS 26.

---

### 15. Measured facts on the S10 — `docs/device-facts.md`

| Item | Value |
|---------|--------|
| `ro.boot.em.model` | `SM-G973F` |
| `ro.product.device` (TWRP) | `beyond1lte` |
| Bootloader | `G973FXXUHHXJ1` |
| Recovery | TWRP 3.7.1_12 Artisan v2, kernel ExtremeKernel 4.14.356 |
| Live DTB `model` | `Samsung BEYOND1LTE EUR OPEN 26 board based on EXYNOS9820` |
| RAM | ~7.1 GB visible (MemTotal 7 421 288 kB), 8 cores |
| Boot partition | `sda14`, 57,671,680 bytes |
| System partition | `sda25`, 7,340,032,000 bytes |
| Userdata partition | `sda34`, 499,641,200,640 bytes (~465 GiB) |

**What must change for beyond1lte compared to beyond2lte:**
1. UEFI: start from the `beyond2ltePkg` overlay, renamed `beyond1ltePkg`, with the S10 DTB.
2. boot.img DTB: equivalent of `Resources/DTBs/beyond2lte.dtb` (base + rev 26 overlay + `model`).
3. Identity and guards: `package-boot.py` (board beyond1lte, model SM-G973F), `pull-b2l-log.py` (require `ro.boot.em.model == SM-G973F`), `DeviceTree.c`.
4. XNU kernel: no hardware addresses to change according to the measurements.

---

### 16. Summary of overall status (2026-11-10)

The project has moved past the proof-of-concept phase:
- **Full boot**: S-Boot → BootShim → UEFI → XnuLoader → XNU → HFS root → launchd → SpringBoard, verified on the S10.
- **Metal GPU rendering active**: the iOS interface is rendered by the Mali-G76, with Liquid Glass on the Focus button of the control center.
- **Performance**: 10-14 frames/s, "extremely smoother" than CPU rendering (owner, B1L-0115).
- **Still to do:** missing glyphs (round buttons, Concentration, volume), Clock/Calendar icons, white widgets, the `240x240` icon snapshot path (diagnosis in progress, builds 0136-0139), Wi-Fi, Bluetooth, audio, cellular, camera, sleep.

The previous documentation (Parts 1 and 2) covers the directory layout, the scripts, the build environment, the build process and the security policy.
