#!/bin/bash
# Overwrite, in place, the dyld shared cache files of the LIVE iOS root image with
# the statically translated copies (same size, same inode, owner and hard links).
# Run with sudo:   sudo bash install-translated-cache.sh
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
user_home=$(eval echo "~${SUDO_USER:-$USER}")
image="$user_home/Developer/s10-port/beyond2lte-ios/out/live/ios26-userdata-live.img"
src="$user_home/Developer/s10-port/static-work/out"
rel=private/preboot/Cryptexes/OS/System/Library/Caches/com.apple.dyld
[[ -f "$image" && -d "$src" ]] || { echo "missing image or translated cache" >&2; exit 1; }

mnt=$(hdiutil attach -plist -nobrowse -owners on -imagekey diskimage-class=CRawDiskImage "$image" |
  grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1)
[[ -n "$mnt" ]] || { echo "attach failed" >&2; exit 1; }
trap 'hdiutil detach "$mnt" >/dev/null 2>&1 || hdiutil detach -force "$mnt" >/dev/null 2>&1 || true' EXIT
mdutil -i off "$mnt" >/dev/null 2>&1 || true
[[ -x "$mnt/sbin/launchd" ]] || { echo "not the iOS root" >&2; exit 1; }

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
sync
hdiutil detach "$mnt" >/dev/null
trap - EXIT
dev=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$image" | awk 'NR==1{print $1}')
fsck_hfs -fn "$dev" | tail -1
hdiutil detach "$dev" >/dev/null
echo "done: $n translated cache files installed into $image"
