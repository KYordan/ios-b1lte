#!/bin/bash
# Merge preference keys into the LIVE iOS root image (read back from the phone),
# keeping each file's owner and mode. Run with sudo:
#
#   sudo bash set-root-prefs.sh springboard-watchdogs
#
# Profiles (one change per build, see docs/plan-phase4.md):
#   springboard-watchdogs   com.apple.springboard: BKNoWatchdogs = YES (as on the S7)
#   reduce-transparency     com.apple.Accessibility: EnhancedBackgroundContrastEnabled = YES
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
user_home=$(eval echo "~${SUDO_USER:-$USER}")
image="$user_home/Developer/s10-port/beyond2lte-ios/out/live/ios26-userdata-live.img"
python="$user_home/Developer/tools/mamba/envs/s10/bin/python3"
profile=${1:?profile}
prefs=private/var/mobile/Library/Preferences
case "$profile" in
  springboard-watchdogs) file=$prefs/com.apple.springboard.plist; keys='{"BKNoWatchdogs": true}' ;;
  reduce-transparency)   file=$prefs/com.apple.Accessibility.plist; keys='{"EnhancedBackgroundContrastEnabled": true}' ;;
  *) echo "unknown profile $profile" >&2; exit 1 ;;
esac
[[ -f "$image" ]] || { echo "missing $image (read USERDATA back first)" >&2; exit 1; }

mnt=$(hdiutil attach -plist -nobrowse -owners on -imagekey diskimage-class=CRawDiskImage "$image" |
  grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1)
[[ -n "$mnt" ]] || { echo "attach failed" >&2; exit 1; }
trap 'hdiutil detach "$mnt" >/dev/null 2>&1 || hdiutil detach -force "$mnt" >/dev/null 2>&1 || true' EXIT
mdutil -i off "$mnt" >/dev/null 2>&1 || true
[[ -x "$mnt/sbin/launchd" ]] || { echo "not the iOS root" >&2; exit 1; }

"$python" -I - "$mnt/$file" "$keys" <<'EOF'
import json, os, plistlib, sys
path, keys = sys.argv[1], json.loads(sys.argv[2])
if os.path.exists(path):
    st = os.lstat(path); data = plistlib.load(open(path, 'rb')); fmt = plistlib.FMT_BINARY
    uid, gid, mode = st.st_uid, st.st_gid, st.st_mode & 0o7777
else:
    data, fmt, uid, gid, mode = {}, plistlib.FMT_BINARY, 501, 501, 0o600
before = {k: data.get(k) for k in keys}
data.update(keys)
blob = plistlib.dumps(data, fmt=fmt)
with open(path, 'r+b' if os.path.exists(path) else 'wb') as f:   # same inode when it exists
    f.write(blob); f.truncate(); f.flush(); os.fsync(f.fileno())
os.chown(path, uid, gid); os.chmod(path, mode)
print(f'{path}: {before} -> {keys}; owner {uid}:{gid} mode {oct(mode)}')
EOF
sync
hdiutil detach "$mnt" >/dev/null
trap - EXIT
dev=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$image" | awk 'NR==1{print $1}')
fsck_hfs -fn "$dev" | tail -1
hdiutil detach "$dev" >/dev/null
echo "done: $profile applied to $image"
