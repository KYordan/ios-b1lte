# New clean USERDATA (B1L-0250, prepared 2026-11-15)

Owner's request: rebuild a cleaner USERDATA, following ios-pipa. Ours dates from 3 November and has
accumulated settings applied after the fact, some of which failed (ExtensionKit in 0249).

## What changes in the preparation

`beyond2lte-ios/scripts/prepare-ios-root.py` now applies, **before the first boot**, settings taken from
ios-pipa. Source: github.com/nishant713/ios-pipa, `tools/pipa-root-tweaks.py`, commit `830ec1c`, MIT license
(Copyright (c) 2026 Leo Malik, dipa-studio).

| Setting | Why |
|---|---|
| ExtensionKit `enforce_xpc_cache_code_signing` disabled (`/private/var/preferences/FeatureFlags/Domain/ExtensionKit.plist`) | otherwise iOS refuses all app extensions (widgets, wallpapers); folders named after mobile, as iOS creates them |
| launchd `disabled.plist`: `com.apple.fairplayd.H2` and `com.apple.bluetoothd` disabled | they kept restarting and blocked LaunchServices app scanning on ios-pipa; **remove bluetoothd when Bluetooth returns** |
| `launchd.plist`: `_PanicOnCrash` renamed `_PanicOnCrasX` | a crashing daemon no longer panics the phone (here a panic looks like a total freeze) |
| SpringBoard: `BKNoWatchdogs`, watchdog exception for purplebuddy | a slow app is no longer killed |
| `ByHost/com.apple.migration.<UUID>.plist` (`LastSystemVersion`) | otherwise data migration can rerun at every boot; fixed UUID from `B2LPlatformExpert.cpp` (B1L-0161) |

Already present before: Setup skipped, migration (simple file), containers, logs, cryptex links.

**Deliberately left out:**
- ios-pipa's CoreAnimation settings (deep framebuffer, compression): up to the Metal session to decide;
- removing CommCenter and the MediaToolbox logger: not observed as a problem here;
- moving apps to `/Applications`: the kernel already does it before launchd (`b2l_apps_layout.h`, B1L-0024/0025).

Tested on a fake root: `--dry-run`, application, then `--verify`. **Not yet tested on a real image.**

`b1l-tools/build-ios-root.sh` no longer refuses to run when an image exists. It renames it
`ios26-userdata.hfs.img.before-<date>`, without deleting anything.

## Steps

1. **The owner runs it** (sudo, which the assistant does not have):
   ```bash
   sudo bash ~/Developer/s10-port/b1l-tools/build-ios-root.sh
   ```
   Duration: about 30 to 40 minutes. Result: `beyond2lte-ios/out/ipsw/ios26-userdata.hfs.img`, 32 GiB. There are 206 GB
   free.
2. The assistant checks the image (`--verify`, `fsck_hfs`, hash), then asks the owner's agreement to write USERDATA
   (`flash-delta.sh`). The Metal session must release the phone.
3. First boot: long (LaunchServices databases, icons). The Wi-Fi password must be typed again.

## What remains

- **Kept:** our GPU libraries (copied from SYSTEM by the kernel at boot) and the native cache (on SYSTEM).
- **Lost:** the Wi-Fi password and app data.

## PAC translation (added 2026-11-15)

Our old root had received the PAC translation after the fact, with `install-b1l-0011-root.sh` and `0012`. Without it,
every PAC instruction in the system traps, and the phone would be extremely slow. `build-ios-root.sh` now redoes it
itself (step 5b):
- **Shared cache.** `static-work/out-0250` was regenerated on 2026-11-15 from `static-work/orig`, with the same
  `nc-static` as 0012 (`apply`, then the two `nop-call` MediaServices from 0011). Result: **41,502,070 words**, like
  `apply3.txt` from 0012. `compare` against the original on the 84 files gives 0 unexpected changes, except the two NOPs
  intended in `.15` (`static-work/compare-0250.txt`).

  Warning: `static-work/out` is an older translation (4 Nov 13:39, without PACGA v2, without stub islands,
  without the NOPs). It is **not** the phone's version, contrary to what `menage-disque.md` said.
- **Writing into the image.** It is done in place: same size, inode and owner. The script first checks that
  each file in the image is indeed the original cache.
- **Programs outside the cache.** `nc-static macho-apply` translates the programs outside the cache, as in 0011, and also
  the 50 apps in `/private/var/staged_system_apps`, which the kernel then moves to `/Applications` (as in 0018).
  A final check verifies that no word left to convert remains.

Reviewed and dropped:
- `set-root-prefs.sh springboard-watchdogs` is already covered by `prepare-ios-root.py`;
- `reduce-transparency` was never applied according to the docs;
- the app copies from 0013 were cancelled by 0018, and the kernel does the rest (0024/0025).

**What comes back on its own:** the cut dictation (0164). The kernel patches it before launchd at each boot, and its guard
accepts both the original cache and the translated cache.

## To coordinate

The kernel code from 0249 (`b2l_apps_layout.h`, Metal session) rewrites `disabled.plist` once and renames the LaunchServices
databases. On a fresh root, it would replace our `disabled.plist` (one less bluetoothd): to be removed or disabled before the first boot.

**Fixed (Metal session, B1L-0250, BOOT `39be1437…`):** `b2l_ext_reset` is off by default and only comes back with the
boot-arg `b2l_ext_reset=1`. The GPU stays open to everyone (0248, set `b2l-metal-v118`). The Metal session does not want
ios-pipa's CoreAnimation settings for now. It no longer writes to USERDATA, and the phone is free, in TWRP.

## Written to the phone (2026-11-15)

- Image: `beyond2lte-ios/out/ipsw/ios26-userdata.hfs.img`, sha256 `1354515aa4495693f773986dfd90aaa86e4663f070e48a85a009a580fbeace42`.
  `fsck_hfs` OK; cache: 84 files translated; outside the cache: 0 errors, no word left to convert.
- `flash-delta.sh` to `userdata` (`/dev/block/sda34`): 6,578 blocks out of 8,192 written in 241 passes, each read back
  without error (log `out/live/flash-userdata-250.log`). Reboot to the system started afterwards.
- No backup of the old USERDATA (owner's choice). The latest copy remains
  `out/live/ios26-userdata-gpu-248.img` (14 Nov).
- The first boot has not been observed yet.
