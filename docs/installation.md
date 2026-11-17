# Installation

> [!WARNING]
> Writing the wrong partition can brick the phone or erase its EFS (IMEI, calibration). Make the backup below first and keep it off the build machine.

## Requirements

- Galaxy S10 **SM-G973F** (`beyond1lte`), bootloader unlocked, TWRP with root ADB.
- `adb` (set `ADB` if it is not at the default path) and, if several devices are connected, `B2L_SERIAL`.
- The three images: `boot.img`, `system.img`, `userdata.img` (see [build.md](build.md)).

## Steps

1. **Check the phone**: `tools/flash-kit/check-device.sh` verifies TWRP, root, `ro.boot.em.model = SM-G973F` and lists the target partitions.
2. **Back up**: `tools/flash-kit/backup-partitions.sh` saves EFS, boot, recovery, DTB and the other critical partitions, with SHA-256 hashes.
3. **Flash**: put the images in `tools/flash-kit/images/` and run `tools/flash-kit/flash-images.sh`. It writes only BOOT (`sda14`), SYSTEM (`sda25`) and USERDATA (`sda34`), checks sizes and hashes, and reads every write back.
4. **Reboot** to the system. The `XnuLoader` boot menu appears first ([boot-menu.md](boot-menu.md)).

Partitions that must never be written are listed in [tools/flash-kit/docs/partition-map.md](../tools/flash-kit/docs/partition-map.md). The kit never repartitions the phone.

## Going back to Android

TODO: document the tested restore path (TWRP restore of the backed-up partitions, or Odin with the stock firmware). Restoring through Odin has not been tested yet.
