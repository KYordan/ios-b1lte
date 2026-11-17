# Investigation: broken icons and loss of speed (2026-11-15, after B1L-0268)

Step 1 (investigation) and step 2 (plan). **Nothing was changed for this investigation.** Sources: `.before-NNNN` backups
of the driver and of Mesa (the icon series is not in git: the last commit of `beyond2lte-ios` covers 0001–0213), kernel logs
`out/logs/0242…0267-boot.txt`, documents `build-b1l-02xx.md`.

## 0. In short

1. **The GPU path itself did not slow down.** The same full-screen test (`CAMetalLayer`, Test Metal app) gives 10.8 i/s in 0242,
   10.9 in 0247 and in 0264, then 9.5 in 0267. What changed is the **amount of work** asked of the GPU and of the processor.
2. Before the icon series, the RenderBox pipelines for the icons were **refused**. Nothing was drawn, so it was fast, but
   the icons stayed empty. Since 0257, they compile, and a single pipeline takes **~10 s** in iconservicesagent: 4 creations
   in 39.6 s, one of them 18.3 s. There is **no cache of compiled shaders**, neither on disk nor in memory.
3. Since 0248, **3 to 4 more processes** use the GPU: cameracaptured, MercuryPosterExtension, SBRendererService and
   WidgetRenderer. cameracaptured alone spends 31.9 s compiling 34 pipelines. The kernel runs GPU jobs one at a time for everyone.
4. **Costly diagnostics** were left on on the hot paths:
   - images sent to the log (`B2LMOD l-img`, 14,520 lines in 0267, with 2 ms pauses under the lock), triggered
     precisely at the 256th and 4096th draws, so during regeneration;
   - a string search on every pass (`B2L PASS`).
5. **The on-disk icon cache is correct.** This is proven in 0262: 82 icons reread, Clock and Calendar included. The breakage
   comes from the **live drawing** by SpringBoard and RenderBox, that is to say from our driver or from the translator. The project has no
   "icon code" of its own: all the fixes of the series are in the Metal driver and in Mesa.
6. The driver **emulates by copies** what an Apple GPU does without copying: textures placed on a buffer or on an IOSurface.
   Outside the copy points, these copies can be stale. This is the most likely cause of the pattern "fine at rest, broken when moving, right after
   returning to the page", but it is still **to be proven** (section 4).

## 1. The "icons" series build by build

| Build | Change | Speed | Icons | Verdict |
|---|---|---|---|---|
| 0248 | GPU opened to all processes | **slower**: +4 processes on the GPU | no effect | go back to the list (boot-arg `b2l_gpu_all=0`) |
| 0251/0253 | textures on buffer; iconservicesagent crash fixed | neutral | necessary | keep |
| 0256 | Metal heap (`MTLHeap`) | neutral | necessary (otherwise RenderBox draws nothing) | keep |
| 0257 | "custom effect" pipelines accepted | **slower**: pipelines of ~10 s | the pictograms appear | keep, but with a shader cache |
| 0258 | purgeable memory reclaimed by the kernel | not measured (purge never seen triggered) | none | to watch |
| 0259 | global lock of the Gallium context | **slower**: threads are serialized | necessary (otherwise SpringBoard crashes in a loop) | keep, while reducing it (0268) |
| 0260/0262/0265/0267 | diagnostics (`B2L DRAW`, images, `B2L LOCK`, `B2L HOLD`) | **slower**: images under the lock | — | cut except the counters |
| 0261 | restart of primitive 0xFFFF | neutral | correct according to the Metal specification | keep |
| 0263 | buffers in persistent mapping | neutral | no visible effect | keep (Gallium contract) |
| 0264 | Test Metal app injected into Dictaphone by default | only when Dictaphone opens | — | cut by default |
| 0266 | fragment position +0.5 | neutral | no visible effect, but required by Metal and checked by test | keep |
| 0268 | shader translation outside the lock | to measure | — | keep if stable |

**Removing the "icon patch" as a whole would bring the speed back, but also the empty icons.** The useless and costly parts are
0248 (too many processes), the diagnostics and the injection of 0264. The deep fixes (0257, 0259, 0261, 0263, 0266) are required
by the specification or by stability.

## 2. How Metal is used here

- **Device**: a plug-in (`b2l_metal_plugin.m`) makes `MTLCreateSystemDefaultDevice()` answer with our driver
  `libhakiosmetal` (`hakiosmetal_air.m`, about 8,000 lines, based on Scion, MIT). A Mesa `rwa_device` and **a single**
  `pipe_context` per process, protected by a single lock (0259).
- **Queues and command buffers**: `HakiosMetalCommandQueue` / `HakiosMetalCommandBuffer`. The encoders **record** the work
  (`HakiosMetalWork`). `-commit` replays it into Gallium under the lock, then empties the context. The kernel (`B2LMaliService`) runs
  the job chains **one at a time** for the whole system. The end is awaited by a barrier (fence). Then come the
  write-backs, the events and the end handlers.
- **Buffers**: panfrost objects in persistent mapping (0263). "No copy" (`NoCopy`) means, here, one copy on input and one on output.
- **Textures**:
  - all in **Shared** mode, except MSAA;
  - a heap (`MTLHeap`) is only a counter: each resource is a separate allocation, and `makeAliasable` does nothing;
  - textures placed on a buffer, or on an IOSurface whose rows are not a multiple of 64 bytes: **private copy**
    refreshed at every `setFragmentTexture` (`hakiosRefreshLiveCopy`, copy by the processor under the lock), and
    **written back into the buffer** after the GPU (`hakiosWriteBack`, read by the GPU then copy).
- **Shaders**: Apple's `.metallib` files (AIR, LLVM bitcode) are translated into NIR (`air_to_nir`), then compiled by the Bifrost
  compiler of panfrost, **at every pipeline creation and in every process**. The linked functions (`custom_fn`) are reread
  every time.
- **Cache**: no pipeline cache. On iPhone, the compiled binaries are cached by the system. Here, every startup
  and every process retranslates and recompiles.
- **`MTLFence`**: `newFence` returns nil. This is not a problem as long as everything runs in submission order.

## 3. Where the speed went (measured)

| Cost | Measurement | Build |
|---|---|---|
| Pipeline creation, iconservicesagent | 4 creations, 39.6 s, **~9.9 s each** (one of 18.3 s) | 0267 |
| Pipeline creation, cameracaptured | 34 creations, 31.9 s (~0.94 s each) | 0267 |
| Pipeline creation, backboardd / SpringBoard | 120 in 8.1 s / 91 in 4.6 s | 0267 |
| `-commit` under the lock, backboardd | 10.3 s cumulative, 2.0 s in a single hold | 0267 |
| Live copies / write-backs | iconservicesagent 559 ms; SpringBoard 2.1 s | 0267 |
| Processes on the GPU | 5 in 0247 → 8 in 0267 | logs |
| Diagnostic images in the log | 14,520 lines (base64, up to 4 MB per process, 2 ms pauses, under the lock) | 0267 |
| GPU memory of backboardd | 711 MB reserved in 750 objects, of which 343 MB marked purgeable | 0267 |

The "big one-off slowdowns at the moment the icons regenerate" are explained by three measurements:
- the creation of an icon pipeline takes ~10 s;
- up to 0268, the whole process stayed blocked during that time;
- the diagnostic images, triggered during the same phase, add to it.

## 4. Why the icons are redrawn when they already exist correctly

- **It is not the icon cache.** The file `com.apple.IconsCache` is correct (0262).
- **It is iOS 26 that recomposes the icons live.** SpringBoard places the pictogram and the glass effect with RenderBox
  (pipelines `custom_effect` / `uber`), and redraws the same sizes hundreds of times: the dumps at the 256th and 4096th draws
  exist because these counters are reached. This behavior comes from iOS, not from a cache bug on our side. It cannot be
  avoided without touching SpringBoard; we can only make it correct and fast.
- **What, on our side, can make this redraw wrong at times.** Ranked from most likely to least likely:
  1. **Stale live copies.** On an Apple GPU, a texture placed on a buffer *is* the buffer. Here, it is a copy updated at
     only two moments:
     - when `setFragmentTexture` is encoded, while the drawing runs later, at `-commit`;
     - after the end of the GPU.
     A render target placed on a buffer is **filled only once, at its creation** (shortcut from 0235, noted in the code). If
     RenderBox reuses this buffer or texture for another icon, or reloads its content (`loadAction` Load), the old image
     stays. A rebuilt view creates new textures, so new copies: this is consistent with "I leave, I come back, it's correct".
  2. **Reading between processes before the end of the write.** It is not verified that backboardd waits for the end of SpringBoard's
     drawing into an IOSurface before reading it. ios-pipa had to add per-surface barriers in the kernel.
  3. **RenderBox shaders still wrong in a case not covered by the tests**, for example a glass gradient or a distance mask
     at a specific size or format. The unit tests pass (43 OK), but they do not replay the real SpringBoard pipelines.
- Leads from the requested list, with their status:

| Lead | Status |
|---|---|
| Cache absent or invalidated | on-disk icon cache correct; **pipeline cache absent** (cause of slowness, not of breakage) |
| Textures recreated at every frame | not measured; live copies are refreshed at **every** binding, even without change |
| BGRA/RGBA, premultiplied alpha, sRGB | no clue: 8 formats and the R8/RG16F/BGRA8 pass pass the tests |
| Mipmaps | textures on buffer refused beyond one level (Metal rule), no refusal seen |
| No row / alignment | Mali's 64-byte rule: it is what creates the live copies (rows of 480 or 384 bytes) |
| CPU/GPU synchronization | **main lead**: copies taken at encoding, not at execution; barriers between processes not verified |
| Purged textures | 343 MB marked purgeable in backboardd; the kernel reclaim (0258) has never been seen triggered |
| Storage mode | everything in Shared, consistent with the copies; no sign of a bug |

## 5. Control Center

From your feedback of 0262 and 0263, its icons are **wrong too**. They are drawn in SpringBoard (and chronod for the
WidgetKit controls) by the same RenderBox and CoreAnimation paths, with no project code between the two. This points to
**the driver and the translator**, not to a project icon code (there is none). A photo of the Control Center on the current
build would confirm it.

## 6. ANGLE, iOS Pipa, MoltenVK

- **ios-pipa** (iOS port by Leo Malik, MIT license, local clone from 12 November) **does not use ANGLE**. It runs on iOS 16, where
  CoreAnimation can still draw in OpenGL ES through `EAGLServer`; it plugs this path into the Adreno GPU (eagl.c, taken from
  HackiOS). Its icons are drawn by the processor. **iOS 26 no longer has EAGLServer** (B1L-0031): CoreAnimation, RenderBox and
  the glass only speak Metal now.
- **ANGLE** translates OpenGL ES to Metal or Vulkan. This is the reverse of our need: iOS 26 *emits* Metal, and the Mali *understands*
  GLES or Vulkan. A "GL → Metal" bridge would bring nothing, and a "GL like ios-pipa" bridge no longer exists in iOS 26.
  **Not recommended.**
- **MoltenVK** translates Vulkan to Metal: same direction problem. The useful variant would be **Metal → Vulkan on panvk**, Mesa's
  Vulkan driver, present for Bifrost v6/v7 in our Mesa 25.0.7 (`src/panfrost/vulkan`, archs 6 and 7). Its maturity level
  on the G76 is not verified.
  - Expected gains: command recording per thread without a global lock, explicit barriers, serializable pipeline cache.
  - Cost: rewriting the `rwa_*` layer (air_metal.c, ~7,000 lines) and a good part of the driver, so several weeks.
  - **Not now.** To be reassessed if steps P1 to P4 are not enough.

## 7. Prioritized plan

One build per step, measurements before and after: `B2L HOLD`/`B2L LOCK`, `CAMetalLayer` i/s, pipeline creation times and memory
(`mali mem`), plus your feedback on the screen.

| # | Step | Why (demonstrated cause) | Expected gain | Cost |
|---|---|---|---|---|
| P1 | Cut the diagnostics on the hot paths (images `B2LMOD`, `B2L PASS`, `DRAW`, `TEX`, `LIVE`), keep the counters | 14,520 lines and pauses under the lock | end of the jolts linked to the log | 1 build |
| P2 | Go back to 4 GPU processes (`b2l_gpu_all=0`) and cut the Test Metal injection by default | +4 processes since 0248, 32 s of compilation for cameracaptured | GPU and cores freed for SpringBoard and backboardd | 1 build (boot-arg) |
| P3 | Read the result of 0268 (translation or compilation) then **cache of compiled shaders**: memory per process, then shared disk (key = hash of the bitcode, the linked functions, the constants, the target format and the driver version) | pipelines of ~10 s, recompiled at every startup and in every process | creation in milliseconds after the first startup | 2 to 3 builds |
| P4 | Live copies: copy only if the source has changed (`IOSurfaceGetSeed` for surfaces), copy **at execution time** (at `-commit`), and refresh the loaded render targets (`loadAction` Load) | copies at every binding; shortcut from 0235 | fewer copies; fixes lead no. 1 if it is the right one | 2 builds |
| P5 | Proof of the breakage: a targeted diagnostic that compares, during a movement, the buffer content and the copy content at draw time, and counts the loaded targets without refresh | no breakage hypothesis is proven yet | tells which of the three leads is the right one | 1 build |
| P6 | Per-surface write barriers in the kernel (ios-pipa lesson), if P5 shows reads before the end | lead no. 2 | fewer half-drawn images | 2 builds |
| P7 | Optimizations afterwards: grouping of draws into passes, suballocation per command buffer (ios-pipa: from 200 µs to 13 µs per draw), texture and buffer reserve, triple buffering of the `CAMetalLayer`, DVFS | measurements from 0264/0267 | toward 40 i/s | several builds |

Recommended order: **P1 and P2 first** (quick return to the former speed, without losing the icons), then P5 to prove the
cause of the breakage **before** fixing it (P4 or P6), and P3 in parallel for the one-off slowdowns. The processes removed in P2
will be reopened one by one, once P3 is in place.

## 8. What I cannot verify without the device

- **Metal System Trace, GPU Frame Capture and signposts are not usable**: Instruments and Xcode cannot connect to
  this port (no Apple GPU, no developer link). The driver counters (`B2L HOLD`, `B2L LOCK`) and the kernel job
  times replace them.
- What the screen shows: only your photos can say it.
- To provide at each test:
  1. the build in TWRP after the test (I read the kernel log myself);
  2. a photo of the home screen **at rest** and one **during a scroll**;
  3. a photo of the **Control Center**;
  4. the approximate time of the moment the icons break;
  5. your agreement to read USERDATA read-only if the iOS logs are useful (read-only, nothing written).

## 9. How hakios-arm64e-dada does it (review of 2026-11-15)

Repository `superboyyy/hakios-arm64e-dada` (iOS 26 arm64e on Xiaomi 15, Snapdragon 8 Elite, Adreno 830), read on GitHub without
downloading anything else except two text patches (scratchpad folder). Licenses:
- kernel and userland: APSL 2.0 (HakiOS);
- tools: MIT;
- GPU patches: license of each project, namely metal2vulkan under **GPL-2.0-or-later OR LGPL-3.0-or-later**, NativeMetal (UniXNU)
  under MIT and Mesa under MIT.

- **Their GPU path**: Metal → **metal2vulkan** (steelbrain, in Rust: translates AIR into SPIR-V and provides the Metal API over Vulkan)
  → **Turnip** (Mesa's Vulkan for Adreno) → their kernel driver. The Metal plug-in comes from UniXNU NativeMetal. They first had
  the same path as us (HakiOS: air_metal.c + Gallium freedreno, `userland/tools/mesa/air`), then moved to Vulkan (`gpu2/`).
- **Why we cannot take it as is**: metal2vulkan requires a **Vulkan 1.2** device. In our Mesa 25.0.7, panvk
  (Mesa's Vulkan for the Mali) advertises **Vulkan 1.0** on Bifrost v7 (`get_vk_version`), without `descriptorIndexing` nor
  `shaderFloat16`, with 4 descriptor sets at most. It even refuses to open on v6/v7 without
  `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1` ("not well-tested on v7"). The Adreno 830 has a full Vulkan; the Mali-G76 does not.
  A more recent Mesa has not been verified: it would have to be downloaded.
- **What we can take** (ideas only, without copying code):
  1. **Texture placed directly on the buffer** (their `PaddedTargetsPolicy::Alias`), instead of a private copy ("shadow").
     Here, the 13 buffer textures of the 0267 log all have a row stride and an offset that are multiples of 64 bytes
     (256, 384, 512, 832), so compatible with the Mali. Yet `newTextureWithDescriptor:offset:bytesPerRow:` always copies them.
     Placing them on the buffer would remove the live copies, the write-backs and the stale copies (lead no. 1 of the breakage).
     Their precaution: check that the tiled render writes do not overflow after the last row or column
     (16×4 texels on Turnip; 16×16 tiles on the Mali, to be tested).
  2. Their cited performance fixes (UniXNU 0003, 0005, 0009–0015): pipeline cache warm-up, fewer allocations
     per draw, unchanged bindings skipped. These are our steps P3 and P7.
  3. **Pixel-by-pixel validation against a Mac's GPU** (same AIR shader, two GPUs).
