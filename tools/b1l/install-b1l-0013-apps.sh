#!/bin/bash
# B1L-0013: restore the 50 user-facing system apps that the first-boot
# SystemAppMigrator consumed without installing (nativeResult=0, 0 installed,
# /private/var/staged_system_apps left empty).
#
# Approach: install them as built-in system apps in /Applications (like
# Preferences.app and MobilePhone.app), copied from the pristine OS.dmg
# /private/var/staged_system_apps. Their AppPlaceholders and the installd /
# LaunchServices state are only RENAMED, so iOS re-registers everything fresh
# on next boot. Nothing is deleted. New arm64e binaries are statically
# translated with nc-static macho-apply.
#   sudo bash install-b1l-0013-apps.sh
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
user_home=$(eval echo "~${SUDO_USER:-$USER}")
S="$user_home/Developer/s10-port"
image="$S/beyond2lte-ios/out/live/ios26-userdata-live.img"
rollback="$S/beyond2lte-ios/out/live/ios26-userdata-live-0012.img"
osdmg="$S/ipsw-23G90/OS.dmg"
tool="$S/b1l-tools/static-translate/nc-static"
log="$S/install-b1l-0013-apps.log"
tag=b2l-before-0013
[[ -f "$image" && -f "$rollback" && -f "$osdmg" && -x "$tool" ]] || { echo "missing image, rollback clone, OS.dmg or nc-static" >&2; exit 1; }
exec > >(tee "$log") 2>&1

attach() { hdiutil attach -plist -nobrowse "$@" | grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1; }
os=$(attach -readonly -owners on "$osdmg"); [[ -n "$os" ]] || { echo "OS.dmg attach failed" >&2; exit 1; }
mnt=$(attach -owners on -imagekey diskimage-class=CRawDiskImage "$image"); [[ -n "$mnt" ]] || { hdiutil detach "$os"; echo "attach failed" >&2; exit 1; }
trap 'hdiutil detach "$mnt" >/dev/null 2>&1 || hdiutil detach -force "$mnt" >/dev/null 2>&1 || true; hdiutil detach "$os" >/dev/null 2>&1 || true' EXIT
mdutil -i off "$mnt" >/dev/null 2>&1 || true
[[ -x "$mnt/sbin/launchd" && -d "$mnt/Applications/Preferences.app" ]] || { echo "not the iOS root" >&2; exit 1; }
src="$os/private/var/staged_system_apps"
[[ -d "$src" ]] || { echo "no staged_system_apps in OS.dmg" >&2; exit 1; }
owner=$(stat -f '%u:%g' "$mnt/Applications/Preferences.app")
echo "== /Applications owner taken from Preferences.app: $owner"

echo "== 1/4 copy staged apps into /Applications"
mkdir -p "$mnt/System/Library/AppPlaceholders.$tag"
copied=0 skipped=0 parked=0
for app in "$src"/*.app; do
  name=$(basename "$app")
  if [[ -e "$mnt/Applications/$name" ]]; then skipped=$((skipped + 1)); continue; fi
  ditto "$app" "$mnt/Applications/$name"
  chown -R "$owner" "$mnt/Applications/$name"
  copied=$((copied + 1))
  ph="$mnt/System/Library/AppPlaceholders/$name"
  if [[ -e "$ph" ]]; then mv "$ph" "$mnt/System/Library/AppPlaceholders.$tag/$name"; parked=$((parked + 1)); fi
done
echo "copied=$copied skipped(existing)=$skipped placeholders parked=$parked"

echo "== 2/4 fresh installd state (renamed, not deleted)"
mi="$mnt/private/var/installd/Library/MobileInstallation"
[[ -d "$mi" ]] && mv "$mi" "$mi.$tag" && echo "MobileInstallation -> MobileInstallation.$tag"

echo "== 3/4 LaunchServices databases (renamed, rebuilt by lsd at boot)"
find "$mnt/private/var" -xdev -name 'com.apple.LaunchServices*.csstore' -type f 2>/dev/null | while read -r db; do
  mv "$db" "$db.$tag"; echo "renamed ${db#$mnt}"
done

echo "== 4/4 static translation of the new apps"
find "$mnt/Applications" -xdev -type f -size +8k -print0 |   # idempotent: translated words are not convertible again
  xargs -0 "$tool" macho-apply | awk '/^macho-apply|^MACHO/' | sort | uniq -c
find "$mnt/Applications" -xdev -type f -size +8k -print0 | xargs -0 "$tool" macho 2>/dev/null | grep 'MACHO TOTAL' | sort | uniq -c

sync
hdiutil detach "$mnt" >/dev/null; hdiutil detach "$os" >/dev/null
trap - EXIT
dev=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$image" | awk 'NR==1{print $1}')
fsck_hfs -fn "$dev" | tail -1
hdiutil detach "$dev" >/dev/null
echo "done: B1L-0013 apps installed into $image (log: $log)"
