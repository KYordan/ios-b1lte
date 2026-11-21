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
| Display | Loader accepted GOPs ≥ 1184 px only: now ≥ 768 px with a narrow progress panel. Boot menu has a compact layout below 2600 px. Default iOS canvas 750 × 1584 @2x (375 × 792 pt) |
| Flash tools | Every guard (model, block, size) reads `board.sh`. `tools/flash-kit` also fixed: `$adb` was never set, `UID` is read-only in bash, block paths were compared without `/dev/block/`, vbmeta was sda11 |
| SYSTEM image | `build-system-image.sh` sizes from the board; ~164 MiB stay free next to the 5.52 GiB native cache (`NATIVE_CACHE_MIB` to shrink it) |

Not verified: no build ran (needs the Mac toolchain), no flash, no boot. The C host tests changed here
(`progress_test.c`, `boot_menu_draw_test.c`) were not compiled on the Windows host.

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

## Open questions

1. Does the external Mu `MemoryMapLib` describe DDR up to `0xA00000000`, or does RamManagerDxe read the real size? The absent-DDR reservation covers either case `[VERIFY]` on the first UEFI boot.
2. The GOP stride on the S10e (1080 or padded) `[VERIFY]` from the loader log.
3. sec_ts Y761 behaves like the S10's Y771 `[VERIFY]` on the first boot with touch.
