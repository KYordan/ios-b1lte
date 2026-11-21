#!/bin/bash
# system-add-fast.sh VERSION_DIR -- as system-add-gl.sh (add $GL_BUILD/VERSION_DIR to SYSTEM as /VERSION_DIR, write only
# the changed 4 MiB blocks), without reading the 7 GB of SYSTEM over USB when nothing has changed on the phone:
#   1. the phone hashes its own SYSTEM, one sha256 per 4 MiB block (hash-device.sh; only the hashes cross USB);
#   2. they are compared with the block hashes of our live copy (b1l-system-live.blockhashes, saved by the last write):
#      identical -> the live copy IS the phone's SYSTEM and is used as it is; any difference (or no saved hashes) ->
#      the full pull of system-add-gl.sh, checked against the phone's own sha256 as before;
#   3. flash-delta.sh reuses the phone's hashes from step 1 (DEV_HASHES) instead of reading the partition again, writes
#      and reads back every changed run, and saves the new image's block hashes for the next time (SAVE_HOST_HASHES).
set -euo pipefail
V=$1
T=${S10_PORT:-$HOME/Developer/s10-port}/b1l-tools
A="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
L=${B2L_ROOT:-$HOME/Developer/s10-port/beyond2lte-ios}/out/live
O=${GL_BUILD:-$HOME/Developer/s10-port/build-gl}/$V
. "$(dirname "${BASH_SOURCE[0]}")/board.sh"
DEV=$B2L_SYSTEM_DEV; BYTES=$B2L_SYSTEM_BYTES
[ -d "$O" ]
[ "$($A get-state </dev/null)" = recovery ]
cd "$L"
start=$(date +%s)
echo "phone: hashing SYSTEM in place..."
bash "$T/hash-device.sh" $DEV $BYTES > phone.blockhashes
echo "phone hashes in $(( $(date +%s) - start )) s"
changed=-1
if [ -f b1l-system-live.img ] && [ -f b1l-system-live.blockhashes ] && \
   [ "$(wc -l < phone.blockhashes)" -eq "$(wc -l < b1l-system-live.blockhashes)" ]; then
	paste -d' ' phone.blockhashes b1l-system-live.blockhashes | awk '$1!=$2{print NR-1, $1}' > changed.blocks
	changed=$(wc -l < changed.blocks | tr -d ' ')
fi
if [ "$changed" -eq 0 ]; then
	echo "live copy matches the phone block for block: no pull"
elif [ "$changed" -gt 0 ] && [ "$changed" -le 256 ]; then
	# The phone changes SYSTEM by itself at every boot (the persistent log at its end, the native cache's pages):
	# only those blocks are read back, each checked against the hash the phone gave for it, then put in the live copy.
	echo "$changed blocks changed on the phone since the last write: pulling only those"
	while read -r b h <&3; do
		$A exec-out "dd if=$DEV bs=4194304 skip=$b count=1 2>/dev/null" </dev/null > block.tmp
		[ "$(shasum -a 256 block.tmp | cut -c1-64)" = "$h" ] || { echo "block $b: pulled bytes differ from the phone's hash" >&2; exit 1; }
		dd if=block.tmp of=b1l-system-live.img bs=4194304 seek=$b conv=notrunc 2>/dev/null
	done 3< changed.blocks
	rm -f block.tmp
	cp phone.blockhashes b1l-system-live.blockhashes
	echo "live copy brought up to the phone ($changed blocks)"
else
	echo "live copy unknown or different: full pull"
	rm -f b1l-system-live.img
	( $A shell "sha256sum $DEV" </dev/null > system-device.sha256 ) &
	$A exec-out "dd if=$DEV bs=4194304 2>/dev/null" </dev/null > b1l-system-live.img
	wait
	h=$(shasum -a 256 b1l-system-live.img | cut -c1-64); d=$(cut -c1-64 system-device.sha256)
	echo "pull $h device $d"; [ "$h" = "$d" ]
fi
rm -f "b1l-system-$V.img"; cp -c b1l-system-live.img "b1l-system-$V.img"
mnt=$(hdiutil attach -plist -nobrowse -imagekey diskimage-class=CRawDiskImage "b1l-system-$V.img" </dev/null | grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1)
[ -n "$mnt" ]; mdutil -i off "$mnt" >/dev/null 2>&1 || true
mkdir -p "$mnt/$V"
for f in "$O"/*; do [ -f "$f" ] && ditto --nohfsCompression "$f" "$mnt/$V/$(basename "$f")"; done
shasum -a 256 "$mnt/$V"/*
# B1L-0149: Broadcom WiFi firmware + NVRAM for B2LWifi (read by the kernel at seed time), kept out of git
WF=${S10_PORT:-$HOME/Developer/s10-port}/device-s10/wifi-bt-fw/system
if [ -d "$WF" ]; then
	mkdir -p "$mnt/b2l-wifi"
	for f in "$WF"/*; do [ -f "$f" ] && ditto --nohfsCompression "$f" "$mnt/b2l-wifi/$(basename "$f")"; done
	shasum -a 256 "$mnt/b2l-wifi"/*
fi
sync; hdiutil detach "$mnt" >/dev/null
cd "${B2L_ROOT:-$HOME/Developer/s10-port/beyond2lte-ios}"
DEV_HASHES="$L/phone.blockhashes" SAVE_HOST_HASHES="$L/b1l-system-$V.blockhashes" \
	bash "$T/flash-delta.sh" "out/live/b1l-system-$V.img" system $DEV $BYTES </dev/null
cd "$L"; rm -f b1l-system-live.img; cp -c "b1l-system-$V.img" b1l-system-live.img
mv "b1l-system-$V.blockhashes" b1l-system-live.blockhashes
echo "SYSTEM-OK in $(( $(date +%s) - start )) s"
