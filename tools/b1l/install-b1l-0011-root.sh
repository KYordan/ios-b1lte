#!/bin/bash
# B1L-0011 root edits on the LIVE iOS root image (run with sudo):
#   1. dyld cache: static-work/out2 = original cache translated with PACGA ABI v2,
#      plus the two verified MediaServices NOPs (About crash fix). In place,
#      same size, inode and owner.
#   2. Standalone arm64e Mach-O files (outside the dyld cache and /private/var):
#      instruction sections translated IN PLACE by nc-static macho-apply.
# Rollback image: out/live/ios26-userdata-live-0006.img (APFS clone, made before).
#   sudo bash install-b1l-0011-root.sh
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
user_home=$(eval echo "~${SUDO_USER:-$USER}")
S="$user_home/Developer/s10-port"
image="$S/beyond2lte-ios/out/live/ios26-userdata-live.img"
rollback="$S/beyond2lte-ios/out/live/ios26-userdata-live-0006.img"
src="$S/static-work/out2"
tool="$S/b1l-tools/static-translate/nc-static"
rel=private/preboot/Cryptexes/OS/System/Library/Caches/com.apple.dyld
log="$S/install-b1l-0011-root.log"
[[ -f "$image" && -f "$rollback" && -d "$src" && -x "$tool" ]] || { echo "missing image, rollback clone, out2 or nc-static" >&2; exit 1; }
exec > >(tee "$log") 2>&1

mnt=$(hdiutil attach -plist -nobrowse -owners on -imagekey diskimage-class=CRawDiskImage "$image" |
  grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1)
[[ -n "$mnt" ]] || { echo "attach failed" >&2; exit 1; }
trap 'hdiutil detach "$mnt" >/dev/null 2>&1 || hdiutil detach -force "$mnt" >/dev/null 2>&1 || true' EXIT
mdutil -i off "$mnt" >/dev/null 2>&1 || true
[[ -x "$mnt/sbin/launchd" ]] || { echo "not the iOS root" >&2; exit 1; }

echo "== 1/2 dyld shared cache (PACGA v2 + MediaServices fix)"
n=0
for f in "$src"/dyld_shared_cache_arm64e*; do
  name=$(basename "$f"); dst="$mnt/$rel/$name"
  [[ -f "$dst" ]] || { echo "missing target $name" >&2; exit 1; }
  [[ $(stat -f %z "$f") -eq $(stat -f %z "$dst") ]] || { echo "size differs: $name" >&2; exit 1; }
  cmp -s "$f" "$dst" && continue
  owner=$(stat -f '%u:%g %Lp %i' "$dst")
  dd if="$f" of="$dst" bs=4m conv=notrunc 2>/dev/null
  cmp -s "$f" "$dst" || { echo "verify failed: $name" >&2; exit 1; }
  [[ $(stat -f '%u:%g %Lp %i' "$dst") == "$owner" ]] || { echo "metadata changed: $name" >&2; exit 1; }
  n=$((n + 1))
done
echo "cache files rewritten: $n"

echo "== 2/2 standalone Mach-O files"
find "$mnt" -xdev -type f -size +8k ! -path '*/com.apple.dyld/*' ! -path "$mnt/private/var/*" -print0 |
  xargs -0 "$tool" macho-apply | awk '/^macho-apply|^MACHO|^  /' | sort | uniq -c | sort -rn | head -20
echo "check: no convertible words left"
find "$mnt" -xdev -type f -size +8k ! -path '*/com.apple.dyld/*' ! -path "$mnt/private/var/*" -print0 |
  xargs -0 "$tool" macho 2>/dev/null | grep -E 'MACHO TOTAL' | sort | uniq -c

sync
hdiutil detach "$mnt" >/dev/null
trap - EXIT
dev=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$image" | awk 'NR==1{print $1}')
fsck_hfs -fn "$dev" | tail -1
hdiutil detach "$dev" >/dev/null
echo "done: B1L-0011 root edits applied to $image (log: $log)"
