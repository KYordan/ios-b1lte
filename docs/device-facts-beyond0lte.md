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

## What has to change for `beyond0lte`

1. `scripts/b2l_board.py`: add `beyond0lte` → `SM-G970F`, userdata `/dev/block/sda31`.
2. Model and size guards in `tools/b1l/*.sh` and `tools/flash-kit/*.sh` (`SM-G973F`, sda34, system size).
3. `common/memory/b2l_ram_ranges.h` and the loader RAM profile: upper bank ends at `0x980000000`,
   `el2_earlymem` at `0x97E000000`.
4. UEFI: a `beyond0ltePkg` built like `scripts/make-board-platform.py`, with this phone's DTB and a 1080 × 2280 GOP.
5. Boot menu and graphics tests for 1080 × 2280.
6. Install TWRP for `beyond0lte` and take the full partition backup before any write.
