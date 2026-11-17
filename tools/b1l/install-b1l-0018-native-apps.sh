#!/bin/bash
# B1L-0018: give the 50 removable system apps back to iOS's own installer.
# B1L-0013 copied them into /Applications, but the shipped LaunchServices
# database (System/Library/CoreServices/com.apple.launchservices.lsd.csdb)
# describes them as staged apps that installd installs into containers, and
# lsd ignored the /Applications copies (only 4 of 50 registered).
#
# This restores the pristine first-boot layout so the shipped SystemAppMigrator
# (launched by b2l-system-app-bootstrap-v2) runs again:
#   - the 50 /Applications copies are moved (not deleted) to /private/var/b2l-parked-0018/
#   - AppPlaceholders.b2l-before-0013/* go back to AppPlaceholders/
#   - /private/var/staged_system_apps is restored from OS.dmg and translated
#   - installd state, the B2L bootstrap receipt and LaunchServices stores are renamed
#   sudo bash install-b1l-0018-native-apps.sh
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
user_home=$(eval echo "~${SUDO_USER:-$USER}")
S="$user_home/Developer/s10-port"
image="$S/beyond2lte-ios/out/live/ios26-userdata-live.img"
rollback="$S/beyond2lte-ios/out/live/ios26-userdata-live-0013.img"
osdmg="$S/ipsw-23G90/OS.dmg"
tool="$S/b1l-tools/static-translate/nc-static"
log="$S/install-b1l-0018-native-apps.log"
tag=b2l-before-0018
[[ -f "$image" && -f "$rollback" && -f "$osdmg" && -x "$tool" ]] || { echo "missing image, rollback clone, OS.dmg or nc-static" >&2; exit 1; }
exec > >(tee "$log") 2>&1
attach() { hdiutil attach -plist -nobrowse "$@" | grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1; }
os=$(attach -readonly -owners on "$osdmg"); [[ -n "$os" ]] || { echo "OS.dmg attach failed" >&2; exit 1; }
mnt=$(attach -owners on -imagekey diskimage-class=CRawDiskImage "$image"); [[ -n "$mnt" ]] || { hdiutil detach "$os"; echo "attach failed" >&2; exit 1; }
trap 'hdiutil detach "$mnt" >/dev/null 2>&1 || hdiutil detach -force "$mnt" >/dev/null 2>&1 || true; hdiutil detach "$os" >/dev/null 2>&1 || true' EXIT
mdutil -i off "$mnt" >/dev/null 2>&1 || true
[[ -x "$mnt/sbin/launchd" ]] || { echo "not the iOS root" >&2; exit 1; }
src="$os/private/var/staged_system_apps"
park="$mnt/private/var/b2l-parked-0018/Applications"
mkdir -p "$park"

echo "== 1/5 park the B1L-0013 /Applications copies"
n=0
for app in "$src"/*.app; do
  name=$(basename "$app")
  if [[ -d "$mnt/Applications/$name" && ! -e "$park/$name" ]]; then mv "$mnt/Applications/$name" "$park/$name"; n=$((n + 1)); fi
done
echo "parked=$n"

echo "== 2/5 restore placeholders"
n=0
if [[ -d "$mnt/System/Library/AppPlaceholders.b2l-before-0013" ]]; then
  for ph in "$mnt/System/Library/AppPlaceholders.b2l-before-0013"/*.app; do
    [[ -e "$ph" ]] || continue
    name=$(basename "$ph")
    [[ -e "$mnt/System/Library/AppPlaceholders/$name" ]] || { mv "$ph" "$mnt/System/Library/AppPlaceholders/$name"; n=$((n + 1)); }
  done
fi
echo "placeholders restored=$n (total now $(ls "$mnt/System/Library/AppPlaceholders" | wc -l | tr -d ' '))"

echo "== 3/5 restore /private/var/staged_system_apps from OS.dmg"
dst="$mnt/private/var/staged_system_apps"
[[ -d "$dst" ]] || { mkdir -p "$dst"; chown "$(stat -f '%u:%g' "$src")" "$dst"; chmod "$(stat -f '%Lp' "$src")" "$dst"; }
n=0
for app in "$src"/*.app; do
  name=$(basename "$app")
  [[ -e "$dst/$name" ]] && continue
  ditto "$app" "$dst/$name"; n=$((n + 1))
done
echo "staged apps restored=$n (owner as in OS.dmg: $(stat -f '%Su:%Sg' "$src/Calculator.app"))"
find "$dst" -xdev -type f -size +8k -print0 | xargs -0 "$tool" macho-apply | awk '/^macho-apply|^MACHO/' | sort | uniq -c
find "$dst" -xdev -type f -size +8k -print0 | xargs -0 "$tool" macho 2>/dev/null | grep 'MACHO TOTAL' | sort | uniq -c

echo "== 4/5 fresh installd state and B2L migration receipt (renamed)"
mi="$mnt/private/var/installd/Library/MobileInstallation"
[[ -d "$mi" ]] && mv "$mi" "$mi.$tag" && echo "MobileInstallation -> .$tag"
for f in "$mnt/private/var/db/b2l-system-app-bootstrap-v1.plist" "$mnt/private/var/log/b2l-system-app-bootstrap-v2.log"; do
  [[ -e "$f" ]] && mv "$f" "$f.$tag" && echo "renamed ${f#$mnt}"
done

echo "== 5/5 LaunchServices stores (renamed, rebuilt by lsd)"
find "$mnt/private/var" -xdev \( -name 'com.apple.LaunchServices-*.csstore' -o -name 'SystemDataOnly-com.apple.LaunchServices-*.csstore' \) -type f 2>/dev/null |
  while read -r db; do mv "$db" "$db.$tag"; echo "renamed ${db#$mnt}"; done

sync
hdiutil detach "$mnt" >/dev/null; hdiutil detach "$os" >/dev/null
trap - EXIT
dev=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$image" | awk 'NR==1{print $1}')
fsck_hfs -fn "$dev" | tail -1
hdiutil detach "$dev" >/dev/null
echo "done: B1L-0018 native app staging restored in $image (log: $log)"
