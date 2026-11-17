# Partition map — Galaxy S10 SM-G973F (beyond1lte)

Data measured from TWRP root (`docs/device-facts.md`).

## Partitions that can be written to (with this kit)

| Name | Block | Size | Content | Written by |
|-----|------|--------|---------|-----------|
| `boot` | `/dev/block/sda14` | 57,671,680 bytes (55 MiB) | `boot.img`: BootShim + UEFI FD + DTB + XNU trailer | `flash-images.sh` |
| `system` | `/dev/block/sda25` | 7,340,032,000 bytes | Journaled HFS+: native cache `b2l-cache.bin` + 16 MiB journal tail | `flash-images.sh` |
| `userdata` | `/dev/block/sda34` | 499,641,200,640 bytes (~465 GiB) | iOS HFS+ root (32 GiB image) | `flash-images.sh` |

## Partitions to NEVER write

`bota*`, `efs`, `sec_efs`, `cpefs`, `radio`, `keystorage`, `keydata`, `keyrefuge`, `param`, `up_param`, `uh`, `uhcfg`, `steady`, `persistent`, `misc`, `recovery`, `dtb`, `dtbo`, `vbmeta`, the GPT.

## Existing backups

`~/Developer/s10-port/device-s10/backup-20261103/` already contains: efs, sec_efs, cpefs, boot, recovery, dtb, dtbo, vbmeta, uh, uhcfg, param, up_param, keystorage, steady, persistent, misc (all SHA-256 verified).

Android SYSTEM was not backed up (owner's choice). Android userdata is gone. Returning to Android requires Odin with the stock `G973F` firmware.
