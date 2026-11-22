# Facts measured on the S10e (2026-11-21, Android + KernelSU root over adb)

Read-only probe; nothing was written to the phone. Compare with the S10 in [device-facts.md](device-facts.md).

## Identity

| Element | Value |
| --- | --- |
| `ro.boot.em.model` | `SM-G970F` |
| `ro.product.device` | `beyond0lte` (custom ROM "infinity", Android 17, build `CP2A.260605.016`) |
| Bootloader | `G970FXXSGHWC2` |
| Kernel | 4.14.356 FreeRunner kernel, KernelSU root |
| Recovery | not TWRP at the time of the probe `[VERIFY]` |
| Live DTB `model` | `Samsung BEYOND0LTE EUR OPEN 25 board based on EXYNOS9820` (revision **25**; S10 is 26) |
| RAM | `MemTotal` 5 543 148 kB: the **6 GB** variant, 8 cores |
| Screen | 1080 × 2280, density 480 |

`ro.boot.flash.locked=1` and `verifiedbootstate=green` are reported, yet a custom kernel runs:
the props are most likely spoofed. Read `OEM LOCK` / `KG STATE` in Download mode before flashing.

## Partitions (UFS `sda`)

| Name | Block | Bytes | S10 |
| --- | --- | --- | --- |
| boot | sda14 | 57 671 680 | same block, same size |
| recovery | sda15 | | same |
| dtb / dtbo | sda12 / sda13 | 8 MiB each | same |
| system | sda25 | 5 976 883 200 | sda25, 7 340 032 000 |
| userdata | **sda31** | 119 126 622 208 (~111 GiB, f2fs + FBE) | sda34 |
| efs / sec_efs / cpefs | sda3 / sda7 / sdd1 | | same |

The ROM mounts `system` as `logical` (retrofit dynamic partitions): overwriting sda25 removes Android.

## RAM

| Source | Ranges |
| --- | --- |
| `/proc/iomem` System RAM | `0x80000000-0xBAAFFFFF`, `0xC0000000-0xDFFFFFFF`, `0xE1900000-0xFFFFFFFF`, `0x880000000-0x97FFFFFFF` |
| DT `memory@` nodes | `80000000+3AB00000`, `C0000000+20000000`, `E1900000+1E700000`, `880000000+80000000`, `900000000+80000000` |

The low banks match the S10. The upper bank **ends at `0x980000000`** (S10: `0xA00000000`).
The S10 allowlist in `common/memory/b2l_ram_ranges.h` reaches past the end of DRAM on this phone
and must not be used as is.

Reserved memory with a `reg` (22 nodes in total, several without `reg`):

| Node | Base | Size |
| --- | --- | --- |
| sec_debug_magic | 0x80000000 | 0x1000 |
| kaslr | 0x80001000 | 0x1000 |
| el2_code | 0x87000000 | 0x200000 |
| camera_contig | 0x98200000 | 0xE00000 |
| secure_camera | 0x96000000 | 0x2200000 |
| camera | 0x99000000 | 0xE400000 |
| seclog_mem | 0xC0000000 | 0x80000 |
| modem_spare | 0xD0000000 | 0x1600000 |
| modem_if | 0xF0000000 | 0xB900000 |
| cp_ram_logging | 0xFB900000 | 0x2000000 |
| sec_rdx_bootdev | 0x880000000 | 0xAD00000 |
| el2_earlymem | **0x97E000000** | **0x2000000** (32 MiB; S10: 0x9FD800000, 40 MiB) |

## Touch

| Point | Result | Consequence |
| --- | --- | --- |
| DT node | `/hsi2c@104B0000/touchscreen@48`, `sec,sec_ts` | Same address as the S10: `B2LInput.cpp` addresses should hold |
| Chip | `get_chip_name` → **Y761** (S10: Y771), vendor SEC | Same sec_ts family `[VERIFY]` on the first boot |
| Firmware | `SE10540225` | The port driver loads no firmware |

## Display

Panel ID `SDC_910115` (`91 01 15`); the S10 s6e3ha9 reports DDI `0x37 0x4d`. The XNU framebuffer
comes from the UEFI GOP, so the kernel is unaffected, but everything sized for 1440 × 3040
(boot menu font cells, tests, wallpaper crop in `build-springboard-dylib.py`) needs 1080 × 2280.

## Port changes (branch `beyond0lte`)

| Area | Change |
| --- | --- |
| Board table | `scripts/b2l_board.py` and `tools/b1l/board.sh` describe `beyond0lte` (model, partitions, RAM end, display); `tests/test_board_profiles.py` keeps them in step with each other and with the DT evidence |
| DT evidence | `common/memory/beyond0lte-6g-dt.json` from the live DTB (`tools/b1l/fdt2json.py`); only `camera`, `el2_earlymem` and the missing `memory@980000000` differ from the S10 |
| UEFI memory map | `prepare-boot-firmware.py` takes the board's DT evidence and reserves `0x980000000+0x80000000` as absent DDR on top of the two low-bank gaps |
| Loader RAM | `B2L_RAM_ALLOW_END` (`b2l_ram_ranges.h`, `Arena.h`) = `0x97E000000` for the S10e, passed by `build-efi.sh` from `B2L_BOARD`; the kernel keeps the S10 superset, the loader decides |
| UEFI platform | `make-board-platform.py --board beyond0lte` builds `beyond0ltePkg` and retargets the frame-buffer PCDs to 1080 × 2280 (stops if it cannot find them) |
| Display | Loader accepted GOPs ≥ 1184 px only: now ≥ 768 px with a narrow progress panel. Boot menu has a compact layout below 2600 px. iOS canvas 880 × 1912 @2x (440 × 956 pt, the iPhone17,2 layout, as on the S10; ~3 % wider than the panel's aspect). The first boot used 750 × 1584 and SpringBoard left the lower third unrendered |
| Flash tools | Every guard (model, block, size) reads `board.sh`. `tools/flash-kit` also fixed: `$adb` was never set, `UID` is read-only in bash, block paths were compared without `/dev/block/`, vbmeta was sda11 |
| SYSTEM image | `build-system-image.sh` sizes from the board; ~164 MiB stay free next to the 5.52 GiB native cache (`NATIVE_CACHE_MIB` to shrink it) |

Since then the S10e has been built on GitHub Actions and booted many times; results are in
[First boot](#first-boot-2026-11-21) and [Boots since](#boots-since-the-first-one).

## Building for the S10e

### Without a Mac: GitHub Actions

`.github/workflows/build-boot.yml` builds XNU, the `beyond0ltePkg` firmware with XnuLoader and
`boot.img` on a GitHub macOS runner (free for this public repository). It is adapted from
zekasio/beyond2lte-ios and pinned to the revisions of the S10 build: Mu-Silicium `a8ca6f0d`,
beyond2lte-uefi `42412d0` (`codex/touch-from-8549bfa`). The KDK comes from dortania's mirror.

1. Repository *Settings → Secrets and variables → Actions*: add `B2L_ARTIFACT_PASSWORD` (12+ characters).
   The kernel embeds a launch cache derived from the 23G90 `launchd.plist`, so the artifact is a 7z
   archive encrypted with it (artifacts of a public repository are readable by any GitHub user).
2. *Actions → Build boot.img (XNU + UEFI) → Run workflow* (board `beyond0lte`), or push to `beyond0lte`.
3. Download the artifact and open it with 7-Zip and the password: `boot.img`, `manifest.json`,
   `SHA256SUMS`, the kernel, the FD and the loader.

The first run also takes `launchd.plist` from the IPSW's OS image (`tools/b1l/ipsw-launchd.sh`: only
that 7.75 GiB member is fetched with HTTP Range, the AEA key from Apple's public WKMS server) and
caches it for later runs. The live DTB is published only after `tools/b1l/sanitize-dtb.py` zeroed
`/chosen` (serial number, `ap_serial`, `em.did`, initrd range).

### iOS images on Linux / WSL2 (no Mac)

`tools/b1l/linux/` makes the USERDATA root and the SYSTEM image without `hdiutil`. Checked on
WSL2 (Ubuntu 24.04, 2026-11-21) with the 23G90 IPSW:

1. Decrypt the IPSW images: `tools/b1l/zip-member.py IPSW --manifest-key OS` (and `Cryptex1,SystemOS`),
   `tools/b1l/aea-key.py` for the key (Apple's public WKMS), `ipsw fw aea FILE --key-val KEY`
   (github.com/blacktop/ipsw, release checksum verified); AppOS is a plain DMG member.
2. HFS+ for the WSL kernel: `tools/b1l/linux/build-wsl-hfsplus.sh`, then `insmod hfsplus.ko`.
   APFS read: apfs-fuse (github.com/sgan81/apfs-fuse).
3. `HFSPLUS_KO=... tools/b1l/linux/build-ios-root.sh` — copy, `prepare-ios-root.py`, PAC translation.
   Result on 23G90: 41,502,070 cache words (the B1L-0250 figure), the two MediaServices NOPs as the
   only other changes, ~1,100 standalone Mach-O files translated, 0 convertible words left.
4. `B2L_BOARD=beyond0lte WIFI_DIR=... tools/b1l/linux/build-system-image.sh ROOT_IMAGE`.

Differences from the macOS images, all checked with the firmware's own parser (`common/cache/CacheHfs.c`):
- `mkfs.hfsplus -J` leaves the journal uninitialized (JIB `NeedInit`); `hfs-journal-init.py` writes the
  empty journal header as `newfs_hfs`/XNU `journal_create` do, so `b2l_hfs_journal_empty` accepts both images.
- The catalog sits mid-volume, so the grown cache has two inline extents; the loader and kernel use
  `b2l_hfs_cache_view` (up to eight). Linux allocates whole 64 KiB data clumps, so the cache size is
  rounded down to a clump multiple (5,788,073,984 bytes on the S10e) to keep allocation equal to size.
- `grow-native-cache.py` preallocates only on macOS (`F_PREALLOCATE`); `copy-tree.py` skips the symlink
  mode, which Linux cannot set.

Wi-Fi files used for the S10e SYSTEM: `bcmdhd_sta.bin_b1`, `bcmdhd_clm.blob`, `nvram.txt_CS01_semco_b1`,
`bcm4375B1_semco.hcd` (the S10's Semco choice; the S10e module is `[VERIFY]` from the B2L WIFI OTP log).

### Flashing from WSL

- TWRP: official `twrp-3.7.0_9-2-beyond0lte.img` (sha256 `0488c876…`), written to `recovery` (`sda15`)
  from rooted Android with `dd` and read back; the 27 small partitions were backed up first
  (`device-s10e/backup-20261121`, outside git: `efs` holds the IMEI).
- This TWRP has no `/dev/block/by-name`, only `/dev/block/platform/13d60000.ufs/by-name`. After each TWRP
  boot: `adb shell ln -s /dev/block/platform/13d60000.ufs/by-name /dev/block/by-name` (tmpfs, gone at reboot).
- `export ADB=tools/b1l/linux/adb-wsl ADB_EXE=/mnt/c/platform-tools/adb.exe B2L_BOARD=beyond0lte`: the
  wrapper strips the `\r` of Windows adb.exe (guards compare `get-state` and `getprop` output) and passes
  Windows paths for push/pull; `exec-out` stays binary.
- `tools/flash-kit/check-device.sh` passes on the S10e in TWRP (boot sda14, system sda25, userdata sda31).

### With a Mac

```bash
export B2L_BOARD=beyond0lte
# UEFI platform from the S10+ overlay, with the phone's live DTB (device-s10e/dump/live.dtb)
python3 scripts/make-board-platform.py --board beyond0lte --mu "$MU_WORK" --dtb device-s10e/dump/live.dtb
B2L_ENABLE_TOUCH=1 WORKSPACE="$MU_WORK" scripts/build-boot-firmware.sh   # RAM end and display come from the board
tools/b1l/build-system-image.sh
MU_WORK="$MU_WORK" bash tools/b1l/b1l-build.sh 0001 b2l-metal-v1
```

Before the first write: TWRP for `beyond0lte`, `tools/flash-kit/backup-partitions.sh`, and `OEM LOCK`/`KG STATE` checked in Download mode.

## First boot (2026-11-21)

Images: CI run 44894733432 `boot.img` (sha256 `a7a22ac0…`), Linux-built USERDATA root (`f0b05ed8…`) and
SYSTEM (`96c89bb4…`), flashed from WSL in TWRP with `flash-chunked.sh` / `flash-boot.sh`, every chunk read back.

- S-Boot accepts the BOOT image; `beyond0ltePkg` UEFI starts; the boot menu draws on the 1080 × 2280 panel.
- XNU starts with the 6 GiB RAM profile, mounts the Linux-made HFSX root, launchd runs.
- Apple logo, then the iOS 26 lock screen (wallpaper, clock from the RTC, default Cupertino time zone).
- Touch works: sec_ts **Y761** behaves like the S10's Y771 (open question 3 closed).
- The `b2l_prof` sampler (built again by 0109) prints `B2L PROF WAIT` stacks of backboardd on the
  console every 10 s, over the bottom of the UI. A red SpringBoard status bar reads `rdar:45025538`
  (not seen in the photos of builds 7 and 8).

## Boots since the first one

Build numbers are GitHub Actions runs of `build-boot.yml`; only `boot.img` (`sda14`) changed, written and read back from TWRP.

| Build | Change | Result on the phone |
| --- | --- | --- |
| 7 | iOS canvas 880 × 1912 @2x (the iPhone17,2 layout) instead of 750 × 1584 | iOS laid out completely, but only the top 1912 of 2280 panel rows were drawn: the old console stayed below |
| 8 | The scaled copy maps the whole panel (it used `fSurface`'s mapping, sized for the logical rows, and wrote ~1.6 MB a frame past it); `haltRestart()` through the PMU; boot menu with *Power off*, 10 s countdown, S10e layout | **Full screen**, dock visible. Power off no longer panics ("Halt/Restart Timed Out") but restarts |
| 11 | ACPM stop before `PS_HOLD` (Samsung `exynos_acpm_reboot`); battery driver | Power off still restarts; log: battery bus "has no timings" (left unconfigured by the bootloader) |
| 12 | WTSR/SMPL disarmed through ACPM before power off (Samsung `s2m_rtc_shutdown`); battery bus set up (USI_CMGP03 I2C mode, USI reset released, fast-mode timings) | **Power off from iOS switches the phone off.** The menu's *Power off* still restarts (no on-screen note: the APM SRAM is not mapped in UEFI, so both ACPM steps were skipped). Battery: USI mode was `0x0` and is now I2C, but the first byte stays in the TX FIFO (`trans=0xa0006 fifo=0x1000001`): the pins were never muxed |
| 13 | UEFI maps the APM SRAM and mailbox (GCD MMIO, uncached) and shows both ACPM results for 3 s before `PS_HOLD`; battery pins `gpm12-0`/`gpm13-0` set to function 2, no pull (DT `hsi2c45-bus`) | **Battery works: 56 % in Settings and the status bar.** Menu power off: `wtsr -14, acpm -14`, then a restart: the GCD describes the APM ranges only in part, so nothing was sent |
| 14 | `Mapped()` walks the range descriptor by descriptor and adds only the absent pieces | superseded by 16 |
| 15 | Charger state decoded as Samsung's `max77705_charger.c` (`CHGIN_DTLS` too; `CHG_DTLS` 3-4 full); battery log only on changes, 10 s poll | superseded by 16 |
| 16 | Display sleep: IOMobileFramebuffer selector 12 (`kern_RequestPowerChange`, one scalar, 0 = off) implemented: panel black while off, swaps not shown until on. Selector 79 is `IOMobileFramebufferGetBlock` (block 0x40, 44 bytes), a read only | **Charging shown on USB** (about 4 W, no fast-charge negotiation). Display off panicked: "Unaligned kernel data abort" in `bzero` (esr 0x96000061, x2 = 4320 × 2280): `DC ZVA` is not allowed on the write-combined panel mapping |
| 17 | Panel cleared with plain 64-bit stores | **Display sleep works** (power button and auto-lock). **Menu power off works** (`wtsr 0, acpm 0`, the phone switches off) |
| 18 | Storage size: libMobileGestalt (23G90) answers `TotalDiskCapacity` from `IOServiceGetMatchingService(IOBSDNameMatching("disk0"))` and its SInt64 `Size`. `b2l_ufs_disk_init` now publishes `B2LDisk` (`BSD Name` = `disk0`, `Size` = READ CAPACITY of the whole LU, read-only, not an IOMedia). Log line `B2L UFS: disk0 registry entry published`; boot-arg `b2l_disk0=0` turns it off. GPU: new workflow `build-graphics.yml` builds the Metal/GL sets for SYSTEM (`GRAPHICS_DIR=` in `build-system-image.sh`); SYSTEM must be rebuilt and rewritten once for it. CPU clocks: a load limit in the thermal thread (every 100 ms, busiest CPU of each cluster from `processor_info`; aims at 80 % busy, back to the boot target at 90 %; bottoms 400/500/500 MHz; boot-arg `b2l_governor=0` turns it off), log `B2L DVFS LOAD:` every 10 s. SpringBoard dylib: the publication check accepts the native cache's translated code words, as for the bootstrap executable (`B2L SB: dylib publication error=5` in build 17). Charger: CHG_DETAILS_02 to CHG_CNFG_12 logged read only at start and on plug/unplug | **Capacity shows 128 GB** ("117.19 GB of 128 GB used": used = disk0 minus the free space of the 32 GiB HFS root). **SpringBoard dylib `publication error=0`.** Load limit works: idle 442/507/520 MHz, 46 °C instead of ~65 °C, 419 requests, none failed. Charger regs the same on battery and on USB: CNFG_02 0x52, CNFG_09 0x23 (about 0.9 A charge and input limits by Samsung's register layout; charging 225 mA into the battery with the screen on). **Panic** after a few minutes: "Interrupts held disabled timed out: 523 us >= 461 us" on CPU0 in `waitq_wakeup64_thread` (`workq_kill_old_threads_call`): the iPhoneOS limit is 12000 ticks, 461 us at 26 MHz, too tight at 0.44 GHz |
| 21 | Loader: `interrupt-masked-debug-mode=1` (trace instead of panic). First boot with the new SYSTEM: the GPU set (`b2l-graphics-5`) | No panic. **Metal on the Mali:** all 12 files published, `MetalPluginName B2LMetalv95`, 7 GPU clients, 4956 job chains, 0 failed; fragment jobs 0.4–1.3 ms. **The screen flickered black every other frame:** backboardd alternated surface 3 (3584-byte rows, imported) and the framebuffer's client surface 2 (the panel's 4320-byte rows, not a multiple of 64): `B2L IMPORT: surface 2 … 4320 bytes a row -> refused`, drawn into a private texture, never written back, shown black |
| 22 | Scaled: the client surface's rows are its own width rounded up to 64 bytes (3584) | **No more flicker:** `B2L IMPORT: surface 2 880x1912 … -> ok`, 0 refused. **Opening Safari kills the phone**, as in build 21 (and B1L-0218..0223 on the S10): the log stops about 1 s after `com.apple.WebKit.WebContent`, `.Networking` and `.GPU` start, no panic text. The S10 traced it to the first run of WebKit's JIT code. Also seen: `vnode_setsize for swap files failed: 1` (the swap file's first attempt fails on `PROTECTION_CLASS_C`, ENOTSUP on HFS, and later attempts find it already marked swap); left alone |
| 23 | Patch 0110: `mmap(MAP_JIT)` answers EPERM to `com.apple.WebKit*` (what a process without the JIT entitlement gets), so JavaScriptCore runs on its interpreter; other MAP_JIT users are allowed and logged once (`B2L JIT:`); boot-arg `b2l_webkit_jit=1` gives WebKit its JIT back | **Safari no longer kills the phone**: 20 `MAP_JIT refused` (WebContent), pages load (1.3 MB received), no other MAP_JIT user. Still slow: clusters 85–100 % busy under use; at 66–68 °C the 65/60/72 thresholds held M4 at 936 MHz (one touch of 72 sends every cluster to its floor, a level returns only at <= 60). Even idle one M4 stays 40–90 % busy: backboardd's `coreanimation.display` thread runs 40–90 % in the kernel, i.e. the scaled CPU copy (880x1912 -> 1080x2280, no zero-copy when scaled) reading write-combined (uncached) memory a pixel at a time, plus ~1.6 MB a frame of panel read-back for the repeated rows. RAM: iOS owns 4909 MiB of the 6 GiB (the rest is firmware carve-outs), no jetsam kills. backboardd "crash" = SIGTERM at shutdown |
| 24 | Scaled copy (`B2LBlit.h b2l_blit_scaled_scratch`): each needed source row read once with memcpy into cached scratch, scaled through a column table, written with memcpy; repeated rows never read back from the panel (boot-arg `b2l_fb_fastscale=0`: old path). Thermal defaults 75/70/85 C (were 65/60/72; boot-args `b2l_thr_hi/lo/crit`). Touch: SET_TOUCHFUNCTION 0x69 = default + GLOVE, Samsung's "Touch sensitivity" for screen protectors (boot-arg `b2l_touch_glove=0`: 0x61). Loader adds `b2l_perf=1` (B2L PERF lines: blit ms per 10 s) | **Owner: "the phone flies", touch much better and smoother.** Scaled copy ~9 ms a frame (1480 ms for 161, ~1 GB/s into the write-combined panel). M4 holds 2.08 GHz to ~75 C, steps down only at 77–79 C under sustained use; idle 442/507/520 MHz at 47–50 C. backboardd's display thread 1 % when idle. Touch 0 transfer failures. No panic; the `B2L CRASH` lines are SIGKILL/SIGTERM at shutdown |
| 25 | Brightness (`B2LBacklight.h`, started by `B2LPlatformExpert`, S10e panel only): `AppleARMBacklight` with `backlight-control` = true, `IODisplayParameters`/`brightness` 0..65536, `BrightnessMilliNits`, what 23G90's CoreBrightness looks for (shared-cache strings); every `setProperties` logged (`B2L BL: set …`). Panel S6E3FA7 (`s6e3fa7_beyond0_a3_s0`) through DSIM0 0x19080000 (Samsung cal_9820 regs: PAYLOAD 0x5c, PKTHDR 0x58, FIFOCTRL 0x68): F0 5A 5A, B1 AOR, F7 03, F0 A5 A5; AOR only (vtotal 2304, 0x0014 = 420 nits .. 0x08D4), gamma left as the bootloader set it. 6 s into the boot a visible test: 40 nits for 1.5 s, then 300. Boot-args `b2l_backlight=0`, `b2l_bl_test=0` | **Rebooted to recovery.** The panel path works (`40 nits … AOR 0x0827: ok`, `300 nits … AOR 0x02a1: ok`, 12 packets, 0 failed). But backboardd crashed three times at the same pc, CoreBrightness+0x169924 (slide from `mach_msg2_trap`, `dsc_sym.py`): a NULL brightness-curve table. `-getBacklightCalibration:` (`[[self reader] copyProperty:@"backlight-calibration-parameters"]`) failed, and the fallback converts a brightness before the curve (`copyMarketingTable`) is built. launchd: "rebooting due to critical process crashes: backboardd". Owner flashed build 24 back |
| 26 | `AppleARMBacklight` also publishes `backlight-calibration-parameters` (dictionary, IOFixed: current-for-max/mid-backlight, hardware-max-current-limit, lMax/Mid/MinProduct, lMax/Mid/MinPanel) and `backlight-marketing-table` (74 IOFixed nits, Samsung's s6e3fa7 beyond0 lum_tbl), keys read from CoreBrightness's code. Patch 0111: a backboardd crash (exception) removes the service, so the next backboardd starts without it instead of a reboot loop. Boot-arg `b2l_bl_ios=0`: panel path only, no service | boots, no backboardd crash, the slider drives the panel (iOS writes `brightness-nits` as 16.16, plus DisplayBrightness/CompensationTweak dictionaries). Bug: values under ~15 nits (< 1 000 000 raw) were read as millinits → full brightness at the bottom of the slider; top request seen 396.9 nits, cap `brightness-nits-cap` 420 |
| 27 | `brightness-nits` always 16.16, kept as 16.16 down to the AOR (finer steps at the low end); a step that maps to the AOR already sent is not sent again; the 6 s dim test is off by default (`b2l_bl_test=1` brings it back) | not tested yet |
| 28 | Audio and Bluetooth through iOS, ported from malthiscode/ios-b1lte 38230ad (S10 B1L-0274..0282): ABOX path kept open with the iOS volume, `/dev/hakios-audio`, HakiOS's AudioServerPlugIn **built in CI and embedded in the kernel** (scripts/build-audio-plugin.py, no SYSTEM rebuild), root model D94AP + `/product/audio` acoustic-id 8017 + `/product` product-id shown to audiomxd only (IOUserClient `b2l_hide_product_id`, from their later commit), real-time floor 40 µs (`sched_rt.c`), bluetoothd enabled with the BlueTool stand-in (rebuilt in CI from userspace/b2l-bluetool.c), `com.b2l.bluetooth` control, Skywalk user pipe for A2DP. Build 26 already showed on the S10e: both CS35L40 answer, the boot beep plays (heard by the user), BCM4375 takes bt.hcd ("Beyond E43"). Kill switches: `b2l_audio_ios=0`, `b2l_hw_model=0`, `b2l_product_id=0`, `b2l_bt_ios=0`. userspace/audio-plugin/tests/test4.cpp is stale (ring doubled in 38230ad) | not tested yet |

Build 17 log (pulled from TWRP on 22 Nov): no panic; six display off/on cycles; battery 51 → 48 % at
0.7–1.0 A; 55–62 °C; clean shutdown (`syncing disks`, `/` unmounted). Still `B2L GL/METAL: publication error=2`, and
backboardd's selector 79 (block 0x40) answered *unsupported* after each display on, without visible effect.

Status after build 17: power off from iOS and the menu, battery level and charging, display sleep and full-screen
iOS all work. The phone draws about 1 A with the screen on (CPU composition, no CPU idle states): expect 2.5–3 h from
a full battery until Metal takes the drawing off the CPU.

Seen on boot 8–11, still open: ~~the power button dims and freezes the picture~~ (fixed in build 17); *Settings → General → About*
shows *Capacity: Zero KB* (no IOMedia for the UFS disk) with the right *Available*; widgets stay at "Waiting…";
app-switcher snapshots are black; the GL/Metal userland is not on the S10e SYSTEM image (`B2L GL: publication error=2`).

## Open questions

1. Does the external Mu `MemoryMapLib` describe DDR up to `0xA00000000`, or does RamManagerDxe read the real size? The absent-DDR reservation covers either case `[VERIFY]` on the first UEFI boot.
2. ~~The GOP stride on the S10e~~: 1080 × 2280, stride 4320 (boot 7 log: `AppleCLCD 1080x2280 stride 4320`).
3. ~~sec_ts Y761 behaves like the S10's Y771~~: yes, touch works (first boot).
4. Wi-Fi module (Semco vs Murata NVRAM) `[VERIFY]` from the `B2L WIFI` OTP log.
5. ~~Does `PS_HOLD` low switch the S10e off once WTSR/SMPL are disarmed?~~ Yes, from iOS (build 12).
6. ~~Is the CMGP USI clock running when the bootloader leaves the battery bus unconfigured?~~ Yes: the controller takes mode, timings and data (build 12); the pins were the missing part.
