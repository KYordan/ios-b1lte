#!/bin/bash
# system-add-gl.sh VERSION_DIR -- fresh read-only pull of SYSTEM (hash-checked), add $GL_BUILD/VERSION_DIR as
# /VERSION_DIR (no HFS compression), delta-flash only the changed 4 MiB blocks. Keeps the live copy in sync.
set -euo pipefail
V=$1
A="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
L=${B2L_ROOT:-$HOME/Developer/s10-port/beyond2lte-ios}/out/live
O=${GL_BUILD:-$HOME/Developer/s10-port/build-gl}/$V
[ -d "$O" ]
[ "$($A get-state </dev/null)" = recovery ]
cd "$L"; rm -f b1l-system-live.img
( $A shell "sha256sum /dev/block/sda25" </dev/null > system-device.sha256 ) &
$A exec-out "dd if=/dev/block/sda25 bs=4194304 2>/dev/null" </dev/null > b1l-system-live.img
wait
h=$(shasum -a 256 b1l-system-live.img | cut -c1-64); d=$(cut -c1-64 system-device.sha256)
echo "pull $h device $d"; [ "$h" = "$d" ]
rm -f "b1l-system-$V.img"; cp -c b1l-system-live.img "b1l-system-$V.img"
mnt=$(hdiutil attach -plist -nobrowse -imagekey diskimage-class=CRawDiskImage "b1l-system-$V.img" </dev/null | grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1)
[ -n "$mnt" ]; mdutil -i off "$mnt" >/dev/null 2>&1 || true
mkdir -p "$mnt/$V"
for f in "$O"/*; do [ -f "$f" ] && ditto --nohfsCompression "$f" "$mnt/$V/$(basename "$f")"; done   # every plain file (B1L-0044: bundle + plist)
shasum -a 256 "$mnt/$V"/*
sync; hdiutil detach "$mnt" >/dev/null
cd "${B2L_ROOT:-$HOME/Developer/s10-port/beyond2lte-ios}"
bash ../b1l-tools/flash-delta.sh "out/live/b1l-system-$V.img" system /dev/block/sda25 7340032000 </dev/null
cd "$L"; rm -f b1l-system-live.img; cp -c "b1l-system-$V.img" b1l-system-live.img
echo SYSTEM-OK
