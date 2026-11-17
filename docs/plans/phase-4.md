# Plan of fixes after B1L-0003 (phase 4)

State as of 2026-11-04, observed by the owner on the S10:

| Element | State |
| --- | --- |
| Lock screen, touch, keyboard | **Work** |
| Settings | **Crashes** on opening |
| Control Center | **Does not appear** |
| Wallpaper | Black |
| Clock | Red |
| Smoothness | Low: CPU rendering at 1440×3040, @3x |

Project rule: **a single change per build**, with a log `docs/journal/build-b1l-XXXX.md`.
Fixes on the **kernel or loader** side go through BOOT (~26 MB, a few seconds).
Fixes on the **iOS** side touch the root (USERDATA). `[HYPOTHESIS]` = to be proven by the logs.

## Findings of step 0 (2026-11-04, logs `out/logs/0003.logarchive`)

- **Settings has no bug: it is killed by the watchdog.** The 3 `Preferences-*.ips` reports give
  `0x8BADF00D`, "scene-create watchdog transgression … exhausted real (wall clock) time allowance of 20.00 seconds".
  At the time of the kill: 167 s of CPU, of which 113 s in system time, which matches the arm64e emulation of code not yet translated.
  The main thread is in SwiftUI (`SidebarRow_iOS`, `ScaledMetric`).
  Fix: `BKNoWatchdogs = YES` in `com.apple.springboard`, as on the S7 (`sweet-root-tweaks.py`, `herolte-tune-ios-root.sh`),
  then the native cache learns the Settings code over successive boots.
- Other crashes: voice services (KonaSynthesizer, SiriAUSP, MacinTalkAUSP, MauiAUSP), MomentsUIService and BlastDoor.
  They have no visible effect, but are candidates for daemon cleanup (performance).
- **Non-standard screen size**: 1440×3040 at @3x gives 480×1013.33 points. The system wallpapers only cover
  393×852 … **440×956** (D94) and "any". The S7 had this problem (WallpaperKit filters by `logicalScreenClass`).

## Revised order

1. **B1L-0004 (root)**: `BKNoWatchdogs`. A single preferences file, prepared on the up-to-date copy of USERDATA.
2. **B1L-0005 (BOOT)**: logical screen **880×1912 at @2x = 440×956 points**, the real size of the iPhone 16 Pro Max,
   instead of 960×2026. We get 2.6× fewer pixels than today, the D94 screen class, and a 3% stretch toward the panel.
   The MobileGestalt cache regeneration must also be forced: the single backup of `b2l_profile_cache_refresh`
   already exists, so a new generation is needed.
3. **B1L-0006 (root)**: Reduce transparency (control center, clock).
4. Wallpaper: to be re-evaluated after B1L-0005 (correct screen class).

## Step 0: logs and working base (no write to the phone)

Without this data, steps 3 and 4 would be mere guesses.

1. From TWRP, export the iOS unified logs (tracev3) and the `.ips` crash reports:
   `B2L_BOARD=beyond1lte B2L_SERIAL=<SERIAL> scripts/pull-b2l-unified-logs.py --adb <adb> --include-crash-reports --output out/logs/0003.logarchive`.
   We will look for: the Preferences crash (exception, backtrace), CoreAnimation, CoreImage and CoreGraphics errors
   (`invalid image colorspace: NULL`, `invalid context`), and PosterBoard.
2. Read USERDATA (32 GiB) back to the Mac, to obtain `ios26-userdata-live.img`.
   **Essential before any change to the root**: the native cache has rewritten translated pages
   into the iOS files on the phone. Rewriting the original image would erase these translations
   and bring back a cold boot of 40 minutes or more.
   Root changes are then made on this copy, and `flash-chunked.sh` only rewrites the modified chunks.

## Step 1: B1L-0004, 2/3 resolution at @2x (BOOT only)

The biggest smoothness gain, and independent of the other problems.

- Today: framebuffer of 1440×3040 and `display-scale = 3`, i.e. 480×1013 points (`uefi/XnuLoader/DeviceTree.c`).
- Target: **960×2026 pixels at @2x**. We keep **the same 480×1013 points**, so the same layouts,
  with **2.25× fewer pixels** to draw for the CPU.
- The kernel enlarges the image ×1.5 to the screen's 1440×3040. This is the S7 method (`sweetfb_scale`,
  "logical = 2/3 panel, x1.5", patches 0222 and 0236 of `ios-herolte`).
- To modify:
  - `B2LFramebuffer.cpp`: logical size announced to iOS, plus a scaled copy (CPU, then GPU at step 6.3);
  - `DeviceTree.c`: `display-scale = 2` and `artwork-scale-factor = 2`;
  - regenerate the MobileGestalt cache, via the refresh already present (`B2L PROFILE: MobileGestalt refresh`).
    The S7 had to erase this cache once. `[VERIFY]`
- Risks: missing @2x resources for the D94 profile (fallback to @3x scaled), and a CPU cost for the ×1.5 copy.
- Criterion: lock screen and keyboard sharp, with no touch offset. Measure the time to the lock screen
  and the idle CPU usage, before and after.
- Rollback: BOOT 0003.

## Step 2: B1L-0005, reduce transparency (root, very small change)

`[HYPOTHESIS]`: the control center and the red clock come from iOS 26 materials (Liquid Glass, vibrancy:
blur, color matrix, background layers) that CPU rendering cannot apply. This is the same symptom as the
"red veil" of vibrancy on the S7.

- Cheapest test: enable *Reduce Transparency* in the root's accessibility preferences.
  Surfaces become opaque and the clock should turn plain white.
- If the control center appears: the cause is confirmed, and the real fix (filters, then GPU) moves to phase 6.
- If nothing changes: the cause is elsewhere, and the step 0 logs will settle it.

## Step 3: B1L-0006, Settings crash

Unknown cause: it will be the `.ips` report from step 0.
- On the S10+, Settings has opened since 0099. We have the same kernel patches (state 0100), so the crash may come from
  a difference of the S10, or from a hardware-related panel (battery, screen, Wi-Fi).
- Leads to check, in order: exception in Preferences.app or one of its bundles, missing service (XPC),
  or inconsistent MobileGestalt property.

## Step 4: wallpaper

`[HYPOTHESIS]`: PosterBoard has no poster provider. These are extensions installed like apps,
and app installation is blocked on HFS (error 166, "50 candidates, 0 installed" on the S10+).
- Short term: static image drawn from SpringBoard, as on the S7 (`gpu/eagl/src/herosb.c`).
- Long term: staging compatibility on HFS (the author's `docs/hfs-apfs-installation.md`), which also unblocks the apps.

## Then (general plan)

- Performance: move `BTAudioHALPlugin` (on the S7, idle CPU drops from >300% to <25%), daemons that loop,
  static arm64e translation of the root (see the discussion of 2026-11-04).
- Phase 5: Wi-Fi (Broadcom PCIe). Phase 6: Mali-G76 GPU (`docs/gpu/herolte-reference.md`).
- Sleep and thermal limit: still absent, do not leave the device on unattended.
