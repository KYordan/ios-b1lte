# Facts measured on the S10 (2026-11-03, TWRP root over adb)

Raw data, outside the repository: `~/Developer/s10-port/device-s10/dump/` (live DTB, iomem, partitions)
and backups `~/Developer/s10-port/device-s10/backup-20261103/` (these contain the IMEI: copy them off the Mac, never into git).

## Identity

| Element | Value |
| --- | --- |
| `ro.boot.em.model` | `SM-G973F` |
| `ro.product.device` (TWRP) | `beyond1lte` |
| Bootloader | `G973FXXUHHXJ1` |
| Recovery | TWRP 3.7.1_12 Artisan v2, ExtremeKernel 4.14.356 kernel |
| Live DTB `model` | `Samsung BEYOND1LTE EUR OPEN 26 board based on EXYNOS9820` (same revision 26 as the S10+ reference) |
| RAM | ~7.1 GB visible (`MemTotal` 7 421 288 kB), 8 cores |

`OEM LOCK`, `KG STATE`, `RMM STATE`: not readable from TWRP. To be read in Download mode (phase 0).

## Partitions (UFS `sda`)

| Name | Block | Bytes |
| --- | --- | --- |
| boot | sda14 | 57 671 680 (55 MiB, identical to the S10+) |
| recovery | sda15 | 67 633 152 |
| dtb / dtbo | sda12 / sda13 | 8 MiB each |
| system | sda25 | 7 340 032 000 |
| userdata | sda34 | 499 641 200 640 (~465 GiB) |
| efs / sec_efs / cpefs | sda3 / sda7 / sdd1 | 20 / 20 / 6 MiB |

## Backups (read-only, SHA-256 identical phone ↔ Mac)

efs, sec_efs, cpefs, boot, recovery, dtb, dtbo, vbmeta, uh, uhcfg, param, up_param,
keystorage, steady, persistent, misc. Hash list: `SHA256SUMS.txt` in the backup folder.
Restoration via Odin: not yet tested (the phase 0 criterion is still open).

## Comparison with the S10+ reference (`beyond2lte-ios`)

| Point | Result | Consequence |
| --- | --- | --- |
| Reserved memory | 30 properties compared, 0 differences with `beyond2lte-8g-dt.json` | None |
| RAM banks | `0x80000000-0xBAB00000`, `0xC0000000-0xE0000000`, `0xE1900000-0x100000000`, `0x880000000-0xA00000000` | The S10+ allowlist (7 044 MiB) fits entirely inside: `b2l_ram_ranges.h` is valid |
| Touch | `/hsi2c@104B0000/touchscreen@48`, sec_ts Y771 | Addresses in `B2LInput.cpp` are correct |
| Keys | GPIOs identical to the S10+ (only the pinctrl phandles change) | None |
| Screen | s6e3ha9, DDI `0x37 0x4d` (S10+: `0x38 0x4e`) | The framebuffer comes from the UEFI GOP: nothing in the kernel |
| Touch firmware | `y771_beyond1_phole.bin` (S10+: `beyond2`) | The driver does not load any firmware |

## What really has to change for `beyond1lte`

1. **UEFI**: start from the `beyond2ltePkg` overlay (corrected memory map, UFS, touch DXE), renamed
   `beyond1ltePkg`, with the S10's DTB. The original `beyond1ltePkg` has the DXE Heap on the screen area.
2. **DTB of boot.img**: the equivalent of `Resources/DTBs/beyond2lte.dtb` (base + rev 26 overlay + `model`).
   The live DTB (`dump/live.dtb`) is already merged by the bootloader `[VERIFY]`: check that S-Boot accepts it.
3. **Identity and guards**:
   - `package-boot.py`: `--board beyond2lte`, `model SM-G975F`, DTB path.
   - `pull-b2l-log.py`: requires `ro.product.device == beyond2lte`, to be replaced by `ro.boot.em.model == SM-G973F`.
   - `DeviceTree.c`: `compatible`, `board-config` and `samsung,beyond2lte-display` (cosmetic on the iOS side `[VERIFY]`).
4. **XNU kernel**: no hardware address to change according to these measurements.
