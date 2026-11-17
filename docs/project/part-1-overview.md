# Project documentation — iOS 26 port to the Galaxy S10 (beyond1lte)

## Part 1 — Overview, current status and directory layout

---

### 1. What is this project?

This project runs **iOS 26** (build `23G90`, product `iPhone17,2`, iPhone 16 Pro Max) on a **Samsung Galaxy S10 (SM-G973F, codename beyond1lte, Exynos 9820 SoC)**.

It adapts the existing port `beyond2lte-ios` (made for the Galaxy S10+, SM-G975F, same Exynos 9820 SoC) to the smaller S10 model (beyond1lte). It is a **research demonstration**, not a daily phone: no calls, no 4G, no camera, no audio, no sleep. No personal data must be placed on this installation (no encryption, no SEP, no real signature).

---

### 2. Boot chain (from the power button to iOS)

```
S-Boot (Samsung) → BootShim → Mu UEFI (beyond1ltePkg) → XnuLoader (EFI) → XNU (Apple kernel) → HFS root in USERDATA → launchd → SpringBoard
```

| Link | Role |
|------|------|
| **S-Boot** | Original Samsung loader. Loads the `BOOT` partition (`sda14`). No fastboot. |
| **BootShim** | Small transition layer between S-Boot and the UEFI. |
| **Mu UEFI** | UEFI firmware based on Mu-Silicium (EDK II), adapted to the S10 by an *overlay* (`beyond1ltePkg`). Handles the display, UFS, ACPI, touch. |
| **XnuLoader** | Embedded EFI application that reads XNU from the BOOT partition, builds the `boot_args` and the Apple device tree, then hands control to XNU. |
| **XNU** | Apple's open-source kernel (version `rel/xnu-12377`), patched for the B2L board (Exynos 9820, arm64e without hardware PAC). |
| **HFS root** | iOS file system (journaled HFS+, not native APFS) installed in `USERDATA` (`sda34`). |
| **launchd / SpringBoard** | iOS userspace: starts the services and the graphical interface. |

---

### 3. Current status as of 2026-11-10 (build 0139)

From `engineer.md`, `metal-progress.md` and the logs 0136-0139:

**What works:**
- Boot verified on the S10: XNU text visible, 8/8 CPUs initialized (4×A55, 2×A75, 2×M4), timers and interrupts OK.
- Native UFS: read/write directly from XNU.
- HFS root: mounted read/write, journal replay OK.
- iOS 26 userspace: starts, SpringBoard appears, lock screen and touch work.
- Settings: open (since build 0099 on S10+, being stabilized on S10).
- Home screen, control center, contacts, keyboard display (Metal GPU rendering).
- Liquid Glass (blur, glass) rendered on the Focus button of the control center (builds 0127-0128).
- Metal GPU rendering active: backboardd draws on the Mali-G76 via the Metal bridge → libhakiosmetal → Mesa panfrost → Bifrost → Mali.
- CPU thermal throttling: the chip caps at ~62-66 °C instead of 76 °C (build 0030).
- GPU: DVFS at 676 MHz via ACPM (build 0030), first GPU job executed (build 0017), HakiosMali service started (build 0029).
- CPU frequencies read after DVFS: A55 1846, A75 2210, M4 2080 MHz (build 0015).

**What does not work yet:**
- Glyphs of the round buttons in the control center (flashlight, timer, calculator, camera, record), the Concentration moon, the volume slider glyph.
- Clock icon: hands present, dial numerals missing.
- Calendar icon: completely empty.
- Home screen widgets: white rectangles with no content.
- Dock that sometimes flickers.
- A glyph at the top left is displayed with hatching (wrong line width).
- Apps not installed (HFS/APFS block, error 166).
- No Wi-Fi, Bluetooth, audio, cellular, camera, sleep.
- The Metal path does not consume the `240x240 LA08` surfaces of the live icon snapshots (Calendar, Timer) — diagnosis in progress (builds 0136-0139).

**Build in progress:** B1L-0139, set `b2l-metal-v88` (userspace instrumentation to trace the consumption of 240×240 textures by the Metal path). Result pending.

---

### 4. Directory layout

#### Main working folder: `~/Developer/s10-port/`

| Folder | Role |
|---------|------|
| `beyond2lte-ios/` | Core of the project: XNU patches, EFI loader, build/packaging scripts, iOS tooling. Local branch `b1l` (S10 adaptation). |
| `beyond2lte-uefi/` | UEFI overlay for the S10+ (reference). Contains `beyond2ltePkg`, `yap.sh`. |
| `uefi-touch-42412d0/` | Another copy of the UEFI overlay (touch branch, commit 42412d0), touch protocols and diagnostics expected by the loader. |
| `Mu-Silicium/` | EDK II / Mu firmware base (commit `a8ca6f0d`, Mu Basecore `762a65d`). |
| `mu-work-b1l-touch/` | Disposable Mu working tree to build the S10 firmware (beyond1lte). |
| `mu-work-b1l/` | Previous Mu working tree. |
| `device-s10/` | Real phone data: DTB (`base.dtb`, `merged.dtb`, `rev26.dtbo`), TWRP dumps, partition backups (efs, boot, vbmeta, etc.). |
| `b1l-tools/` | Install scripts, flashing, iOS root construction, native cache, arm64e translation. |
| `build-gl/` | OpenGL ES / OSMesa bridge (S7 reference, several versions). |
| `build-mesa/` | Mesa builds (Panfrost for Mali). |
| `build-metal/` | Metal → Vulkan builds (dada / scion). |
| `deps/` / `deps-src/` | Build dependencies (KDK, SDK, libraries). |
| `progress/` | Build status files (one `.status` per build NN, a `current` pointing to the latest). |
| `saved-builds/` | Kept builds (e.g. `0128/`). |
| `static-work/` | Static arm64e translation work (`orig/`, `out/` files, comparisons). |
| `tools-src/` | Additional tools (macOS UnifiedLogs). |
| `*.log` / `run-*.sh` / `flash-*.log` | Logs of past builds and flashes, historical run scripts. |

#### Documentation and planning folder: `~/Port iOS 26 to S10/`

| File / folder | Role |
|-------------------|------|
| `PLAN.md` | Full port plan: strategy, phases 0-7, recovery of iOS 26, hardware, risks, verification. |
| `engineer.md` | Engineer brief: target, current status, storage policy, build environment, workflow, priorities, diagnostics. |
| `gpu.md` | Mali-G76 GPU acceleration plan (steps G0-G7). |
| `docs/` | Detailed build logs (`build-b1l-XXXX.md`), screenshots, GPU plans, phase 4 plan, measured S10 facts (`docs/device-facts.md`), S7 GPU reference (`docs/gpu/herolte-reference.md`). |
| `DOCUMENTATION_PROJET_PART{1,2,3}.md` | This document (overview, build, security, logs, GPU, reference). |

---

### 5. Role of the various scripts

#### In `beyond2lte-ios/scripts/` (build and tooling)

| Script | Role |
|--------|------|
| `build-kernel.sh` | Compiles the XNU kernel (target `DEVELOPMENT ARM64 B2L`, SDK iPhoneOS 26.5). Checks the XNU revision (`ac9718fb…`), integrates libpthread/BearSSL/HFS, runs `make`. Requires `KDKROOT` and `B2L_DEPSROOT`. |
| `build-boot-firmware.sh` | Builds the UEFI firmware (Mu-Silicium) for a given board (`B2L_BOARD`). |
| `build-efi.sh` | Builds the standalone EFI applications. |
| `build-mesa.sh` | Cross-compiles Mesa (Panfrost) for iOS arm64. |
| `build-metal.sh` / `build-metal-scion.sh` | Builds the Metal libraries and the Air bridge (scion). |
| `build-userspace-bootstrap.py` | Generates the userspace payload from the iOS root. |
| `prepare-xnu.sh` | Prepares the XNU sources (applies the B2L overlay). |
| `fetch-xnu.sh` | Fetches the pinned XNU sources. |
| `package-boot.py` | Packages the Android v1 `boot.img` (UEFI firmware + XNU kernel + loader + trailer). Checks the header, the `B2LXNU02` trailer, the 256-byte seed. |
| `make-board-platform.py` | Derives the `beyond1ltePkg` platform package from the S10+ overlay. |
| `prepare-boot-firmware.py` | Prepares the boot firmware. |
| `b2l_board.py` | Board table, identity by `ro.boot.em.model`. |
| `test-loader.sh` | Tests the EFI loader on the host. |
| `test-macho.sh` | Tests Mach-O files. |
| `test-arm64e.sh` | Tests arm64e emulation. |
| `test-native-cache.sh` | Tests the native code cache. |
| `test-ufs-transport.sh` | Tests UFS transport. |
| `pull-b2l-log.py` | Retrieves the kernel log from TWRP. |
| `pull-b2l-unified-logs.py` | Retrieves iOS unified logs (tracev3) and `.ips` crash reports. |
| `inspect-kernel.sh` | Inspects an XNU kernel (symbols, sections). |
| `apply-board-patches.py` | Applies board-specific patches. |
| `decode-b2l-diag.py` | Decodes B2L diagnostics. |
| `stage-libpthread.py` / `stage-bearssl.py` / `stage-hfs.py` | Integrates libpthread, BearSSL and the HFS driver into XNU. |
| `extract-b2lmod.py` | Extracts B2L modules. |
| `grow-native-cache.py` / `init-native-cache.py` | Manages the native code cache. |
| `check-native-cache-codegen.py` | Checks native cache code generation. |
| `audit-kernel-rcpc.py` | Audits the kernel. |
| `build-air-host.sh` | Builds the Air host. |
| `build-firehose.sh` | Builds the Firehose protocol (not used here). |
| `build-springboard-dylib.py` | Builds a SpringBoard dylib. |
| `stage-hfs.py` | Stages the HFS driver. |
| `unsign-branches.py` | Removes signatures from branches. |

#### In `b1l-tools/` (install, flash, iOS root)

| Script | Role |
|--------|------|
| `b1l-build.sh NNNN SET [--mesa] [--no-libs] [--no-kernel]` | Full chain for a Metal build: mesa → libs → kernel → package → system → boot. Writes a status file (`progress/NNNN.status`) readable by `watch-build.sh`. |
| `flash-boot.sh IMAGE SHA256` | Guarded write of the BOOT partition (`sda14`) with readback and SHA-256 verification. Checks TWRP, root, SM-G973F model, block device and non-mounted state. |
| `flash-chunked.sh IMAGE NAME BLOCK BYTES` | Writes a large image (USERDATA, SYSTEM) in verified 512 MiB chunks, resumable (skips what is already written). |
| `flash-delta.sh` / `flash-saved.sh` | Delta flash / saved-image flash variants. |
| `hash-device.sh` | Computes/verifies the hashes of the phone's partitions. |
| `build-ios-root.sh` | Builds the iOS root image (HFS+) from the IPSW. Requires sudo. |
| `build-system-image.sh` | Builds the SYSTEM image (contains the native cache + journal). |
| `set-root-prefs.sh <profile>` | Sets root preferences (e.g. `BKNoWatchdogs`). |
| `install-translated-cache.sh` | Installs the translated code cache. |
| `set-launchd-env.sh` | Configures the launchd environment. |
| `system-add-fast.sh` / `system-add-gl.sh` | Adds files to the SYSTEM image (modified chunks only). |
| `pull-chunked.sh` | Retrieves an image from the phone in chunks (for backup / rollback). |
| `system-pull-changed.sh` | Retrieves only the modified chunks. |
| `copy-tree.py` / `dt-table.py` / `dts-nodes.py` / `fdt2json.py` | Tools for handling file trees and DTBs. |
| `aea-key.py` | AEA key handling (encrypted iOS images). |
| `install-availability.sh` | Installs the availability frameworks. |
| `install-b1l-0011-root.sh` … `install-b1l-0018-native-apps.sh` | Historical install scripts, one per build. |
| `watch-build.sh` / `watch-flash.sh` / `watch-progress.sh` / `watch-system-flash.sh` | Live display of build/flash progress. |

#### Historical run scripts (`run-*.sh` at the root of `s10-port/`)

These are past run scripts (builds 0069 to 0081 and beyond), each running a sequence: kernel build → package → SYSTEM flash → BOOT flash. Example `run-0081.sh`:
1. Copies the graphics headers into XNU
2. Builds the kernel with `build-kernel.sh`
3. Packages `boot.img` with `package-boot.py`
4. Adds the system with `system-add-fast.sh`
5. Flashes BOOT with `flash-boot.sh`

---

### 6. The project's golden rule

> **One single change per test build. Each build has its log `docs/journal/build-b1l-XXXX.md`. "Verified" means observed on the S10.** Never claim a result that has not been observed on the device.

Part 2 covers the build environment, the step-by-step build process, and the security/storage policy.
