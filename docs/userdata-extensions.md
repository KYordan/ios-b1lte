# Unregistered app extensions (2026-11-14, USERDATA re-read after 0248)

Read only: `out/live/ios26-userdata-gpu-248.img` (pull-delta, 974 blocks, 970 s).

## Finding

- `/Applications/*.app` contains 296 `.appex` (e.g. `MobileTimer.app/PlugIns/WorldClockWidget.appex`,
  `MobileCal.app/PlugIns/CalendarWidgetExtension.appex`).
- The LaunchServices database (`…/InternalDaemon/*/Library/Caches/com.apple.LaunchServices-20971545-v2.csstore`, 14 Nov 22:52)
  names **none** of them: WorldClockWidget 0, CalendarWidgetExtension 0, WeatherWidget 0. The apps themselves are there
  (com.apple.mobiletimer 15, com.apple.mobilecal 12). The extensions from `/System/Library/ExtensionKit` are there.
- `/private/var/preferences/FeatureFlags/Domain/` does not exist.

## Reference: ios-pipa (github.com/nishant713/ios-pipa, MIT license)

- Same symptom ("LaunchServices registers apps but no app extensions"), fixed at their boot 50 (NOTES.md):
  `tools/pipa-root-tweaks.py` `extensions()` writes `/private/var/preferences/FeatureFlags/Domain/ExtensionKit.plist`
  = `{enforce_xpc_cache_code_signing: {Enabled: false}}`; plus the LaunchServices database erased once
  (`tools/root-files/ls-rebuild.txt`) so that lsd re-registers everything; and fairplayd.H2 / bluetoothd disabled
  (a daemon that crashes in a loop blocked re-registration, boots 47/48).
- The string `enforce_xpc_cache_code_signing` exists in our iOS 26 cache (`dyld_shared_cache_arm64e.01`).
- Not verified: that this setting is enough on iOS 26, and that it is the cause of the Clock/Calendar/Control Center icons.

## Proposal (writes USERDATA: owner's agreement needed)

The kernel, once only before launchd (like `b2l_apps_layout.h`): write ExtensionKit.plist, rename the two
LaunchServices databases to `*.b2l-before-0249` (nothing is erased), boot-arg switch.
