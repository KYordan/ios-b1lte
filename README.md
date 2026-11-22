# ios-s10e

*(formerly `ios-b1lte`; the S10e port lives on the `beyond0lte` branch)*

**iOS 26 on the Samsung Galaxy S10e (`beyond0lte`, SM-G970F, Exynos 9820)**

Apple's open-source XNU kernel running on a Galaxy S10e and booting the **unmodified iOS 26 userland** (launchd, system daemons, backboardd, SpringBoard) from the phone's own storage.

This branch is the S10e port. It shares the kernel, drivers and loader with the Galaxy S10 port on [`main`](https://github.com/KYordan/ios-s10e/tree/main) (`beyond1lte`, SM-G973F) and adds the S10e board profile, its 6 GB memory map, the 1080 × 2280 display, a Linux/WSL image pipeline and a GitHub Actions build, so **no Mac is needed**.

**Today (November 2026):** the S10e boots to the iOS 26 lock screen and home screen, full screen, with touch and Wi-Fi. Settings, Control Center, Photos and the App Library open. Power off (iOS and boot menu), battery level and charging, and display sleep work. Next: storage size, then [Metal on the GPU](docs/plans/s10e-metal.md).

> [!WARNING]
> Experimental research project, not a product. Apple's security policies (AMFI, code signing, Secure Enclave) are relaxed for hardware bring-up. Installing iOS **erases Android** (`system` and `userdata` are overwritten), and a wrong partition write can brick the phone. Back up first (see [Installing](#installing)).

---

## Hardware

| Component | Galaxy S10e hardware | Port driver & subsystem |
| --- | --- | --- |
| **SoC** | Samsung Exynos 9820 (8 nm) | `B2LPlatformExpert`; Apple device tree built by `XnuLoader` |
| **CPU** | 8 cores, 3 clusters: 2× Mongoose M4, 2× Cortex-A75, 4× Cortex-A55 | PSCI SMP, GIC, cluster-aware placement; arm64e instructions emulated in the kernel |
| **RAM** | 6 GB (DRAM ends at `0x980000000`) | S10e RAM profile (`B2L_RAM_ALLOW_END = 0x97E000000`) |
| **Display** | 5.8" AMOLED, 1080 × 2280 (panel `SDC_910115`) | `B2LFramebuffer` on the UEFI GOP; iOS draws an 880 × 1912 @2x canvas scaled to the panel |
| **Touchscreen** | Samsung `sec_ts` Y761 (`hsi2c@104B0000`) | `B2LInput` HID digitizer |
| **GPU** | ARM Mali-G76 MP12 | `B2LMaliService` (from HakiOS) + Mesa + Metal driver; not active on the S10e yet |
| **Wi-Fi** | Broadcom BCM4375 (PCIe) | `B2LWifi` + `apple80211` bridge for `wifid` |
| **Battery / charger** | MAX77705 fuel gauge and charger (`hsi2c@15D60000`) | `B2LBattery` (`IOPMPowerSource`) |
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
| **CPU / SMP** | ✅ Works | All 8 CPUs start. Clocks: boot target with a thermal cap; build 18 adds a load limit (lower clocks when idle, for battery life), not tested yet |
| **RAM** | ✅ Works | 6 GB profile, no access past the end of DRAM |
| **Display** | ✅ Works | Full screen since build 8 (the scaled copy used to cover only 1912 of the 2280 rows and wrote past its mapping). CPU composition, about 7–20 frames/s |
| **Touch** | ✅ Works | sec_ts Y761 behaves like the S10's Y771 |
| **Wi-Fi** | ✅ Works | Joins a WPA2 network; Control Center and Settings show it |
| **Clock** | ✅ Works | Time from the PMIC RTC |
| **Root filesystem** | ✅ Works | HFS+ root on UFS `userdata`, made on Linux |
| **SpringBoard & apps** | 🚧 Partial | Lock screen, home screen, dock, Control Center, Settings, Photos, App Library; widgets stay at "Waiting…", app-switcher snapshots are black |
| **Power off / restart** | ✅ Works | Restart, power off from iOS (build 12) and from the boot menu (build 17: `wtsr 0, acpm 0`). Sequence as Samsung's kernel: PMIC WTSR/SMPL disarmed, APM stopped, then `PS_HOLD` low |
| **Battery** | ✅ Works | Level shown in Settings and the status bar since build 13 (`B2LBattery`: MAX77705 fuel gauge; the driver sets up the bus the bootloader leaves unconfigured: USI mode, pins, timings). Charging shown on USB (build 16); about 4 W, no fast-charge negotiation yet |
| **Display sleep** | ✅ Works | Power button and wake since build 17: backboardd's `RequestPowerChange` (IOMobileFramebuffer selector 12, found in the 23G90 dyld cache) turns the panel black and holds swaps. The panel itself stays powered (no DSI control yet) |
| **Storage size** | ✅ Works | Capacity 128 GB since build 18 (`B2LDisk`, `disk0` with the real UFS size, which MobileGestalt reads). "Used" counts everything outside the 32 GiB HFS root as used |
| **GPU (Metal)** | ❌ Not on the S10e yet | The Mali-G76 comes up and runs test jobs; the S10e SYSTEM image does not carry the Metal userland yet. Since build 18 it is built on GitHub Actions ([build-graphics.yml](.github/workflows/build-graphics.yml)) and `build-system-image.sh` takes it with `GRAPHICS_DIR=`. Plan: [docs/plans/s10e-metal.md](docs/plans/s10e-metal.md) (the S10 already draws its interface on the GPU: [metal-progress.md](docs/gpu/metal-progress.md)) |
| **Audio, Bluetooth** | ⏸️ Parked | As on the S10 |
| **Camera, cellular, fingerprint** | ❌ Not started | |

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
- **[Mu-Silicium](https://github.com/Project-Silicium/Mu-Silicium) / Project Mu:** the UEFI base.
- **LineageOS and Samsung kernel developers:** the Exynos 9820 hardware reference.

---

## License

- Code written for this project: [BSD 2-Clause License](LICENSE).
- Kernel patches and Apple files in `xnu-overlay/` keep Apple's notices and are distributed under the [Apple Public Source License 2.0](https://opensource.apple.com/apsl/).
- Third-party code keeps its own license; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- iOS, SpringBoard, firmware and all Apple or Samsung binaries belong to their owners and are **not** distributed here.
