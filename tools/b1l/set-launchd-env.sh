#!/bin/bash
# Add environment variables to one launchd job of the LIVE iOS root image, in both
# the job cache (/System/Library/xpc/launchd.plist) and its LaunchDaemons plist.
# Files are rewritten in place (same inode, owner and mode); originals are kept
# next to them as *.b2l-before-env once.
#   sudo bash set-launchd-env.sh com.apple.backboardd CA_DISABLE_ASYNCHRONOUS=1 CA_DISABLE_ASYNC=1
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run with sudo" >&2; exit 1; }
label=${1:?label}; shift; (( $# )) || { echo "no KEY=VALUE given" >&2; exit 1; }
user_home=$(eval echo "~${SUDO_USER:-$USER}")
image="$user_home/Developer/s10-port/beyond2lte-ios/out/live/ios26-userdata-live.img"
mnt=$(hdiutil attach -plist -nobrowse -owners on -imagekey diskimage-class=CRawDiskImage "$image" |
  grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1)
[[ -n "$mnt" ]] || { echo "attach failed" >&2; exit 1; }
trap 'hdiutil detach "$mnt" >/dev/null 2>&1 || hdiutil detach -force "$mnt" >/dev/null 2>&1 || true' EXIT
mdutil -i off "$mnt" >/dev/null 2>&1 || true
[[ -x "$mnt/sbin/launchd" ]] || { echo "not the iOS root" >&2; exit 1; }
/usr/bin/python3 -I - "$mnt" "$label" "$@" <<'EOF'
import os, plistlib, shutil, sys
root, label, pairs = sys.argv[1], sys.argv[2], sys.argv[3:]
env = dict(p.split('=', 1) for p in pairs)
def rewrite(path, data, fmt):
    st = os.stat(path)
    backup = path + '.b2l-before-env'
    if not os.path.exists(backup):
        shutil.copy2(path, backup); os.chown(backup, st.st_uid, st.st_gid)
    with open(path, 'r+b') as f:          # same inode
        f.seek(0); f.write(plistlib.dumps(data, fmt=fmt, sort_keys=False)); f.truncate()
    after = os.stat(path)
    assert (after.st_ino, after.st_uid, after.st_gid, after.st_mode) == (st.st_ino, st.st_uid, st.st_gid, st.st_mode)
cache = os.path.join(root, 'System/Library/xpc/launchd.plist')
d = plistlib.load(open(cache, 'rb'))
hits = [(k, j) for k, j in d['LaunchDaemons'].items() if isinstance(j, dict) and j.get('Label') == label]
assert len(hits) == 1, f'{label}: {len(hits)} cache entries'
key, job = hits[0]
job.setdefault('EnvironmentVariables', {}).update(env)
rewrite(cache, d, plistlib.FMT_BINARY)
print('cache', key, job['EnvironmentVariables'])
own = os.path.join(root, key.lstrip('/'))
if os.path.exists(own):
    raw = open(own, 'rb').read()
    fmt = plistlib.FMT_BINARY if raw.startswith(b'bplist') else plistlib.FMT_XML
    j = plistlib.loads(raw)
    j.setdefault('EnvironmentVariables', {}).update(env)
    rewrite(own, j, fmt)
    print('plist', key, j['EnvironmentVariables'])
EOF
sync
hdiutil detach "$mnt" >/dev/null
trap - EXIT
dev=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$image" | awk 'NR==1{print $1}')
fsck_hfs -fn "$dev" | tail -1
hdiutil detach "$dev" >/dev/null
echo "done: $label environment updated in $image"
