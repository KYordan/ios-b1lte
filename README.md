# ios-s10e

*(formerly `ios-b1lte`; the S10e port lives on the `beyond0lte` branch)*

**iOS 26 on the Samsung Galaxy S10e (`beyond0lte`, SM-G970F, Exynos 9820)**

Apple's open-source XNU kernel running on a Galaxy S10e and booting the **unmodified iOS 26 userland** (launchd, system daemons, backboardd, SpringBoard) from the phone's own storage.

This branch is the S10e port. It shares the kernel, drivers and loader with the Galaxy S10 port on [`main`](https://github.com/KYordan/ios-s10e/tree/main) (`beyond1lte`, SM-G973F) and adds the S10e board profile, its 6 GB memory map, the 1080 × 2280 display, a Linux/WSL image pipeline and a GitHub Actions build, so **no Mac is needed**.

**Today (23 November 2026, build 33):** the S10e boots to the iOS 26 lock screen and home screen, full screen and smooth, drawn by the **Mali GPU through Metal**, with touch (screen-protector mode), Wi-Fi, **sound** through the speaker, wired and **Bluetooth headphones**, **display brightness** from the iOS slider, Safari (JavaScript without JIT), battery level and temperatures, power off and display sleep. In progress: charging under iOS (builds 30–33) and Settings panes that hang (Date & Time waits on `locationd`). See [What works](#status-on-the-s10e) and [Working on now](#working-on-now).

> [!WARNING]
> Experimental research project, not a product. Apple's security policies (AMFI, code signing, Secure Enclave) are relaxed for hardware bring-up. Installing iOS **erases Android** (`system` and `userdata` are overwritten), and a wrong partition write can brick the phone. Back up first (see [Installing](#installing)).

---

## Hardware

| Component | Galaxy S10e hardware | Port driver & subsystem |
| --- | --- | --- |
| **SoC** | Samsung Exynos 9820 (8 nm) | `B2LPlatformExpert`; Apple device tree built by `XnuLoader` |
| **CPU** | 8 cores, 3 clusters: 2× Mongoose M4, 2× Cortex-A75, 4× Cortex-A55 | PSCI SMP, GIC, cluster-aware placement; arm64e instructions emulated in the kernel |
| **RAM** | 6 GB (DRAM ends at `0x980000000`) | S10e RAM profile (`B2L_RAM_ALLOW_END = 0x97E000000`) |
| **Display** | 5.8" AMOLED, 1080 × 2280 (Samsung S6E3FA7, DSIM0) | `B2LFramebuffer` on the UEFI GOP; iOS draws an 880 × 1912 @2x canvas scaled to the panel; `AppleARMBacklight` (`B2LBacklight.h`) sets the brightness through the panel's AOR over DSI |
| **Touchscreen** | Samsung `sec_ts` Y761 (`hsi2c@104B0000`) | `B2LInput` HID digitizer, glove mode for screen protectors |
| **Audio** | ABOX + 2× Cirrus CS35L40 amplifiers | `B2LAudio.h` + `/dev/hakios-audio`; HakiOS's AudioServerPlugIn built in CI and carried by the kernel |
| **Bluetooth** | Broadcom BCM4375 (UART) | `B2LBluetooth` + `com.b2l.bluetooth` control for `bluetoothd` |
| **GPU** | ARM Mali-G76 MP12 | `B2LMaliService` (from HakiOS) + Mesa + Metal driver (`B2LMetalv95`), drawing the interface since build 21 |
| **Wi-Fi** | Broadcom BCM4375 (PCIe) | `B2LWifi` + `apple80211` bridge for `wifid` |
| **Battery / charger** | MAX77705 fuel gauge and charger (`hsi2c@15D60000`); NTC thermistors on the Exynos ADC (`0x15C40000`) | `B2LBattery` (`IOPMPowerSource`), `B2LBatteryTemp.h` (temperatures, charge limits) |
| **PMIC / power** | S2MPS19 via the APM firmware (ACPM) | RTC clock, WTSR/SMPL, ACPM stop, PMU `PS_HOLD` / `SWRESET` |
| **Storage** | UFS | Native UFS transport; HFS+ root on `userdata` (`sda31`); native dyld cache on `system` (`sda25`) |
| **Boot chain** | Samsung S-Boot, bootloader unlocked | `beyond0ltePkg` UEFI (Project Mu) → `XnuLoader` boot menu → XNU |

---

## Status on the S10e

Tested on the phone unless marked otherwise. Build numbers are GitHub Actions runs of [build-boot.yml](.github/workflows/build-boot.yml).

| Subsystem | Status | Notes |
| --- | --- | --- |
| **Boot chain** | ✅ Works | S-Boot → UEFI → boot menu → XNU → Apple logo → lock screen |
| **Boot menu** | ✅ Works | S10e layout; 10 s countdown; *Reboot* and *Reboot to recovery* (TWRP) confirmed. *Power off*: see Power below |
| **CPU / SMP** | ✅ Works | All 8 CPUs start. Load-based clocks (`b2l_perf`), thermal limit 75/70/85 °C on the M4 cores (build 24) |
| **RAM** | ✅ Works | 6 GB profile, no access past the end of DRAM. iOS runs from UFS, not from RAM |
| **Display** | ✅ Works | Full screen; CPU composition. Build 24's row-buffer copy to the panel takes ~9 ms a frame instead of reading back write-combined memory: "the phone flies" |
| **Display brightness** | ✅ Works | Build 27: the Control Center / Settings slider dims the panel smoothly (`AppleARMBacklight` with Samsung's calibration and 74-step curve; S6E3FA7 AOR over DSIM0, about 8 to 420 nits). Only the AOR is changed, the gamma is the bootloader's; no auto-brightness (no light sensor yet) |
| **Touch** | ✅ Works | sec_ts Y761; glove mode (build 24) for screen protectors |
| **Volume buttons** | 🚧 Partial | Volume up/down reach iOS (HID 0xE9/0xEA); with audio (build 28) the volume change itself is not confirmed yet |
| **Audio** | ✅ Works | Build 28/29: speaker and headphones heard. Port of malthiscode/ios-b1lte's S10 audio (ABOX path kept open, `/dev/hakios-audio`, D94AP model and `/product` acoustic-id for audiomxd, 40 µs real-time floor); the HakiOS plug-in is built in CI and published by the kernel, no SYSTEM rebuild. No microphone yet |
| **Bluetooth** | ✅ Works | Build 28/29: Bluetooth headphones pair and play sound (A2DP). The BCM4375 runs its firmware and `bluetoothd` reaches it (BlueTool stand-in, vendor HCI replies, Skywalk pipe for A2DP). Other device types not tested |
| **Wi-Fi** | ✅ Works | Joins a WPA2 network. Build 31: firmware roaming off (a roam needed a new handshake and the router dropped us ~13 s after joining, so Safari loaded nothing) |
| **Safari / WebKit** | 🚧 Partial | Build 23: no more freeze (JavaScriptCore without JIT, patch 0110). Pages to be tested again with build 31's Wi-Fi fix |
| **Clock** | ✅ Works | Time from the PMIC RTC |
| **Root filesystem** | ✅ Works | HFS+ root on UFS `userdata`, made on Linux. No swap file (HFS lacks protection class C) |
| **SpringBoard & apps** | 🚧 Partial | Lock screen, home screen, dock, Control Center, Settings, Photos, App Library, Safari; widgets stay at "Waiting…", app-switcher snapshots are black; SpringBoard restarts now and then ("lockViewController/homeViewController is nil!") |
| **Settings** | 🚧 Partial | Most panes open. **Date & Time and some others hang** and iOS kills Settings (0x8badf00d): they wait on `locationd`, which waits in turn on another service (being traced, builds 29–33) |
| **Power off / restart** | ✅ Works | Restart, power off from iOS and from the boot menu. Sequence as Samsung's kernel: PMIC WTSR/SMPL disarmed, APM stopped, then `PS_HOLD` low |
| **Battery level** | ✅ Works | Level, voltage and current in Settings and the status bar (`B2LBattery`, MAX77705 fuel gauge) |
| **Battery temperature** | ✅ Works | Build 30: battery, charger and USB-port thermistors on the Exynos ADC with the S10e DTB's tables; the battery's goes to iOS |
| **Charging under iOS** | 🚧 In progress | The charger's watchdog ran out a minute after boot, so the phone never charged (builds ≤ 29). Build 30 kicks it (as Samsung's driver) within temperature limits; build 32 raises S-Boot's 400/450 mA to 1500 mA in / 1000 mA charge on a wall charger; build 33 lowers the input to 1000 mA when the charger is hot (≥ 46 °C) instead of stopping. Build 33 not tested yet |
| **Display sleep** | ✅ Works | Power button and wake: backboardd's `RequestPowerChange` turns the panel black and holds swaps. The panel itself stays powered |
| **Storage size** | ✅ Works | Capacity 128 GB (`B2LDisk`, `disk0` with the real UFS size). "Used" counts everything outside the 32 GiB HFS root as used |
| **GPU (Metal)** | 🚧 Partial | Since build 21 (SYSTEM with the `b2l-graphics` set built on [GitHub Actions](.github/workflows/build-graphics.yml)): the Mali-G76 draws the interface through Metal (`MetalPluginName B2LMetalv95`); backboardd, SpringBoard and iconservicesagent are GPU clients, thousands of job chains, none failed. The finished frame is still copied and scaled to the panel by the CPU (~9 ms, because iOS draws an 880 × 1912 canvas); apps' own Metal/OpenGL ES not tested. Plan: [docs/plans/s10e-metal.md](docs/plans/s10e-metal.md) |
| **Location, sensors, haptics, flashlight** | ❌ Not started | References: [Jesvi-iOS](https://github.com/jesvi-eulah/Jesvi-iOS) (S20, Exynos 990) |
| **Camera, cellular, fingerprint, microphone** | ❌ Not started | |

### Working on now

1. **Charging under iOS** (build 33): positive current on a wall charger, the input limit following the charger's temperature.
2. **Settings panes that hang** (Date & Time…): the kernel's wait profiler follows `locationd` and names the process each blocked thread sent to (`B2L PROF DEST`), to find the service it waits on.
3. **Safari** with the Wi-Fi fix of build 31.
4. Next: the microphone; the panel's native resolution so the GPU's frame needs no CPU copy; then, after Jesvi-iOS's notes, haptics, sensors and auto-brightness.

Measured device facts, partition map and the build/boot history: [docs/device-facts-beyond0lte.md](docs/device-facts-beyond0lte.md).

---

## How it boots

The kernel, the `beyond0ltePkg` UEFI firmware and its embedded `XnuLoader` are packed into an Android boot image that S-Boot loads from the `boot` partition (`sda14`). `XnuLoader` builds the Apple device tree, shows the [boot menu](docs/boot-menu.md) and jumps into XNU. iOS comes from two partitions written once: the HFS+ root on `userdata` and the native dyld cache plus the persistent kernel log on `system`.

Boot menu keys: **Volume +/−** move, **Power** select, **Bixby** stop the countdown. Holding **Volume +** while it starts preselects *Reboot to recovery*; **Volume −** opens it without a countdown.

---

## Building

### `boot.img` on GitHub Actions (no Mac)

1. Fork the repository, then in *Settings → Secrets and variables → Actions* add `B2L_ARTIFACT_PASSWORD` (12+ characters). The kernel embeds data derived from the IPSW, so the artifact is a 7z archive encrypted with it.
2. Push to `beyond0lte` (changes under `xnu-overlay/`, `uefi/`, `patches/`, `scripts/`, … start a build), or run *Actions → Build boot.img (XNU + UEFI)* by hand.
3. After about 10 minutes download `beyond0lte-boot-N` and open it with 7-Zip and the password: `boot.img`, `SHA256SUMS`, `manifest.json`, the kernel, the firmware and the loader.

### iOS images on Linux / WSL2

`tools/b1l/linux/` builds the USERDATA root and the SYSTEM image from the iOS 26.6.2 IPSW (**23G90**, iPhone17,2) without `hdiutil`. The steps (IPSW decryption, HFS+ module for WSL, PAC translation of the dyld cache) are in [docs/device-facts-beyond0lte.md](docs/device-facts-beyond0lte.md#ios-images-on-linux--wsl2). Apple's binaries are never committed or redistributed.

### With a Mac

The S10 flow on `main` works with `B2L_BOARD=beyond0lte`; see [docs/build.md](docs/build.md) and the [Mac section](docs/device-facts-beyond0lte.md#with-a-mac).

---

## Installing

Requirements: an S10e **SM-G970F** with an unlocked bootloader, the official TWRP for `beyond0lte` and `adb`.

1. In TWRP, back up every small partition (`tools/flash-kit/backup-partitions.sh`); `efs` holds the IMEI. Keep the backup off the repository.
2. The S10e TWRP has no `/dev/block/by-name`; link it after each TWRP boot, and point the tools at the board (WSL with Windows `adb.exe` shown):
   ```bash
   export ADB=tools/b1l/linux/adb-wsl ADB_EXE=/mnt/c/platform-tools/adb.exe B2L_BOARD=beyond0lte
   $ADB shell ln -s /dev/block/platform/13d60000.ufs/by-name /dev/block/by-name
   ```
3. Write USERDATA (`sda31`) and SYSTEM (`sda25`) once with `tools/b1l/flash-chunked.sh` (verified 512 MiB chunks, resumable), then `boot.img` to `boot` (`sda14`). Every write is read back:
   ```bash
   bash tools/b1l/flash-boot.sh beyond0lte-boot/boot.img "$(awk '$2=="boot.img"{print $1}' beyond0lte-boot/SHA256SUMS)"
   ```
4. A new kernel only needs the `boot.img` write; the iOS data stays.
5. After a boot, read the kernel log from TWRP (read-only):
   ```bash
   B2L_BOARD=beyond0lte python3 scripts/pull-b2l-log.py --serial <SERIAL> --output out/logs/boot.txt
   ```

Back to Android: restore `boot`, `system` and `userdata` from your backup or flash the stock firmware with Odin.

---

## Boot arguments

Set by the loader (`XnuLoader`) or useful when experimenting:

| Argument | Effect | Default |
| --- | --- | --- |
| `b2l_fb_lw`, `b2l_fb_lh` | iOS canvas size, scaled to the panel | `880` × `1912` on the S10e |
| `b2l_battery=0` | Do not start the battery driver | started |
| `b2l_disk0=0` | Do not publish the `disk0` registry entry (storage size in Settings) | published |
| `b2l_governor=0` | CPU clusters at their boot/thermal level, no load limit (before build 18) | load limit on |
| `b2l_synth_mac=0` | No synthetic MAC addresses in `/chosen` | on |
| `b2l_gl=1` | OpenGL ES instead of Metal | off |
| `b2l_backlight=0`, `b2l_bl_ios=0` | No brightness control / panel path only, no iOS service | on |
| `b2l_touch_glove=0` | Touch without glove mode | on |
| `b2l_audio_ios=0` | No `/dev/hakios-audio` and no audio plug-in | on |
| `b2l_hw_model=0`, `b2l_product_id=0` | Keep the Samsung root model / no D94 product-id for audiomxd | on |
| `b2l_bt_ios=0` | No BlueTool stand-in for bluetoothd | on |
| `b2l_webkit_jit=1` | Let WebKit map JIT memory (froze the phone in build 22) | off |
| `b2l_wifi_roam=1` | Let the Wi-Fi firmware roam | off |
| `b2l_chg_wdt=0`, `b2l_chg_ilim=0` | Never kick the charger watchdog / keep S-Boot's charge currents | on |
| `b2l_hang_bt=0` | No backtrace of apps killed for hanging | on |
| `keepsyms=1` | Kernel symbols in panic logs | recommended during bring-up |

---

## Documentation

| Topic | Where |
| --- | --- |
| S10e facts, building without a Mac, boot history | [docs/device-facts-beyond0lte.md](docs/device-facts-beyond0lte.md) |
| Boot menu | [docs/boot-menu.md](docs/boot-menu.md) |
| How the port works | [docs/architecture.md](docs/architecture.md) |
| Troubleshooting, kernel log | [docs/troubleshooting.md](docs/troubleshooting.md), [docs/persistent-console.md](docs/persistent-console.md) |
| GPU and Metal | [docs/gpu/](docs/gpu); S10e plan: [docs/plans/s10e-metal.md](docs/plans/s10e-metal.md) |
| Galaxy S10 facts | [docs/device-facts.md](docs/device-facts.md) |
| Build-by-build history (French, S10) | [docs/journal/](docs/journal) |

---

## Repository structure

```
├── .github/workflows/  # build-boot.yml: XNU + UEFI + boot.img on a macOS runner
├── boards/             # Sanitized board DTBs (beyond0lte)
├── common/             # Code shared by the loader and the kernel (SMP, memory, cache)
├── docs/               # Documentation (see above)
├── patches/            # Patch series applied on Apple open-source XNU
├── scripts/            # Kernel/EFI builds, board profiles, packaging, log readers
├── tests/              # Host tests (C and Python)
├── tools/
│   ├── b1l/            # Build/flash loop; linux/ has the WSL image pipeline and adb wrapper
│   └── flash-kit/      # Device checks, partition backup, guarded flashing
├── uefi/               # XnuLoader (boot menu, device tree) and BootSurvey
├── userspace/          # iOS-side libraries: Metal driver, Mesa glue, audio plug-in
└── xnu-overlay/        # Board drivers and files copied over the XNU tree
```

---

## Third-party projects & libraries

| Project | Link | Role in this port | License |
| --- | --- | --- | --- |
| Apple XNU (xnu-12377) | https://github.com/apple-oss-distributions/xnu | Kernel; `patches/` and Apple files in `xnu-overlay/` derive from it | APSL 2.0 |
| Apple hfs (715.120.4) | https://github.com/apple-oss-distributions/hfs | HFS+ root volume | APSL 2.0 |
| Apple libpthread (539.100.4) | https://github.com/apple-oss-distributions/libpthread | Staged by `scripts/stage-libpthread.py` | APSL 2.0 |
| Apple dyld | https://github.com/apple-oss-distributions/dyld | Reference for dyld cache fix-ups | APSL 2.0 |
| HakiOS | https://github.com/oosexgang/hakios-xnu-kernel | Mali kernel driver, Metal/EAGL bridges, Wi-Fi network layer, audio plug-in | MIT (kernel parts APSL 2.0) |
| Scion (HakiOS Metal driver) | TODO: link (commit `a6f4bc38`) | Base of `userspace/metal/scion` | MIT |
| fiio-jm21-xnu | https://github.com/soumarcelino/fiio-jm21-xnu | AudioServerPlugIn in `userspace/audio-plugin` | MIT |
| ios-pipa | https://github.com/nishant713/ios-pipa | Root-volume settings, WPA code in `b2l_wpa.c` | MIT |
| ios-a5xelte | https://github.com/wardevsa/ios-a5xelte | SpringBoard injection idea | TODO |
| malthiscode/ios-b1lte | https://github.com/malthiscode/ios-b1lte | Galaxy S10 fork: iOS audio and Bluetooth (commit 38230ad), product-id for audiomxd, ported in build 28 | BSD-2-Clause |
| Jesvi-iOS | https://github.com/jesvi-eulah/Jesvi-iOS | iOS 16.5 on the Galaxy S20 (Exynos 990): reference for the MAX77705 charger watchdog, thermistors on the Exynos ADC and charge limits (build 30–33) | MIT (kernel files APSL 2.0) |
| beyond2lte-ios | https://github.com/orhunnx/beyond2lte-ios | Galaxy S10+ port this repository derives from | TODO (files carry BSD-2-Clause) |
| zekasio/beyond2lte-ios | https://github.com/zekasio/beyond2lte-ios | GitHub Actions build this workflow is adapted from | TODO |
| beyond2lte-uefi | https://github.com/orhunnx/beyond2lte-uefi | UEFI firmware hosting `XnuLoader` | TODO |
| Mu-Silicium (Project Mu) | https://github.com/Project-Silicium/Mu-Silicium | UEFI build base | BSD-2-Clause-Patent |
| Mesa 3D (25.0.7) | https://mesa3d.org | panfrost / Bifrost shader compiler | MIT |
| libdrm (`xf86drm.h` subset) | https://gitlab.freedesktop.org/mesa/drm | Interface rewritten in `userspace/mesa/hakios-drm` | MIT |
| BearSSL | https://www.bearssl.org | Crypto staged by `scripts/stage-bearssl.py` | MIT |
| Spleen font | https://github.com/fcambus/spleen | Boot menu font | BSD-2-Clause |
| LineageOS Exynos 9820 kernel | https://github.com/LineageOS/android_kernel_samsung_exynos9820 | Hardware reference only (registers, PSCI, RTC, ACPM, PMIC, reboot) | GPL-2.0 |
| KdkSupportPkg (dortania) | https://github.com/dortania/KdkSupportPkg | Mirror of the Kernel Debug Kit used by CI | Apple proprietary (not redistributed here) |
| ipsw (blacktop) | https://github.com/blacktop/ipsw | IPSW image decryption on Linux | MIT |
| apfs-fuse | https://github.com/sgan81/apfs-fuse | Reading the IPSW's APFS images on Linux | GPL-2.0 |
| Android platform-tools (adb) | https://developer.android.com/tools/releases/platform-tools | Device access and flashing | Apache-2.0 |
| TWRP | https://twrp.me | Root recovery used for flashing | GPL-3.0 |
| LLVM (`llvm-mc`) | https://llvm.org | Disassembly in the dyld cache tools | Apache-2.0 WITH LLVM-exception |

Full notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

---

## Credits

- **Apple Inc.:** open-source releases of XNU, hfs, libpthread and dyld.
- **[beyond2lte-ios](https://github.com/orhunnx/beyond2lte-ios):** the Galaxy S10+ port the S10 and S10e ports come from; **[zekasio](https://github.com/zekasio/beyond2lte-ios)** for the CI build.
- **[HakiOS](https://github.com/oosexgang/hakios-xnu-kernel):** XNU on non-Apple ARM64 hardware, the Mali driver and the Metal architecture.
- **[ios-pipa](https://github.com/nishant713/ios-pipa)** and **[ios-a5xelte](https://github.com/wardevsa/ios-a5xelte):** Wi-Fi, root-volume and SpringBoard ideas.
- **[malthiscode](https://github.com/malthiscode/ios-b1lte):** iOS audio and Bluetooth on the Galaxy S10, ported to the S10e.
- **[Jesvi Jonathan / Jesvi-iOS](https://github.com/jesvi-eulah/Jesvi-iOS):** charging, thermistor and charge-limit work on the Galaxy S20's MAX77705.
- **[Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) / Project Mu:** the UEFI base.
- **LineageOS and Samsung kernel developers:** the Exynos 9820 hardware reference.

---

## License

- Code written for this project: [BSD 2-Clause License](LICENSE).
- Kernel patches and Apple files in `xnu-overlay/` keep Apple's notices and are distributed under the [Apple Public Source License 2.0](https://opensource.apple.com/apsl/).
- Third-party code keeps its own license; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- iOS, SpringBoard, firmware and all Apple or Samsung binaries belong to their owners and are **not** distributed here.
