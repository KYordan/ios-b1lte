# Unusable keyboard: diagnosis (2026-11-11)

Symptom (owner): the keyboard freezes, a key sometimes takes a minute to appear, the system sometimes locks up.

## Method

- USERDATA read back read-only (`pull-delta.sh`, image `out/live/ios26-userdata-kbd-164.img`, 638 changed blocks, 628 s).
- iOS system snapshots (`CrashReporter/*.ips`, bug_type 509, taken by the FrontBoard watchdog) symbolicated against the original cache (frames `[image, offset]`: cache image `cc670b30…` → unslid VA = 0x180000000 + offset, then `dscname.py`).

## Finding

- The CPU is not saturated: over 1 100 s of operation, the kernel's `idle` threads total ~6 000 s (8 cores).
- But the **Spotlight main thread** runs non-stop: **685 s of CPU out of 1 100 s**. In 5 snapshots out of 6 (several boots), its stack is the same:

```
AFDictationConnection _tellSpeechDelegateAvailabilityChanged (block)
 → UIDictationConnection dictationConnnectionDidChangeAvailability:
 → UIDictationController postNotificationName:
 → UISystemKeyboardDockController updateDockItemsVisibility…
 → configuredGlyphWithName: → UIImage imageNamed: → _UIImageCollectImagePathsForPath (file search)
```

  Dictation availability keeps changing; at each change, the **keyboard dock** (microphone button) reloads its
  images on the main thread. Any app that shows the keyboard receives the same flow: its main thread is busy,
  the keys wait behind it → one-minute delays.
- Hints about the trigger (not proven): `-[AFDictationConnection networkAvailability:isAvailable:]` (network reachability),
  `kAFPreferencesDidChangeDarwinNotification` very frequent in os_log, crash loops of SiriAUSP / MauiAUSP /
  MacinTalkAUSP / KonaSynthesizer (speech synthesis).
- Preferences: no `Dictation Enabled` key in `com.apple.assistant.support.plist` → dictation enabled by default.

## Proposed fix

1. Test without a build: Settings → General → Keyboard → turn off **Dictation** (and Siri if possible). The setting persists.
2. If confirmed and we want to make it automatic: write `"Dictation Enabled" = false` in
   `com.apple.assistant.support.plist` (USERDATA: owner's approval required), or find the source of the changes.

## Follow-up (same day)

- The "Dictation" option does not appear in Settings → General → Keyboard: the test without a build is impossible.
- `-[UIDictationController dictationEnabled]` (UIKit, 0x1897a7e48) first calls libMobileGestalt with the key `dictation`
  and returns NO if the answer is false. Cutting the MobileGestalt capability `dictation` therefore disables dictation in
  all of UIKit, keyboard dock included. Obfuscated key in the cache: `1uZbhSbBhsNCsVSsopZ4qg`
  (base64(md5("MGCopyAnswer" + "dictation"))[:22]).
- The cache `com.apple.MobileGestalt.plist` cannot be read without root rights from the mounted image: its content is not verified.
