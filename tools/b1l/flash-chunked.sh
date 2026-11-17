#!/bin/bash
# Write a large image to one phone partition from TWRP in verified 512 MiB chunks.
# Resumable: a chunk whose on-device bytes already match is skipped.
#
#   flash-chunked.sh IMAGE BY-NAME EXPECTED_BLOCK EXPECTED_PARTITION_BYTES
#   e.g. flash-chunked.sh ios26-userdata.hfs.img userdata /dev/block/sda34 499641200640
set -euo pipefail
image=$1 name=$2 block=$3 partition_bytes=$4
adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
chunk=$((512 * 1024 * 1024)); bs=$((4 * 1024 * 1024)); per=$((chunk / bs))
size=$(stat -f %z "$image")
(( size % bs == 0 )) || { echo "image is not 4 MiB aligned" >&2; exit 1; }
(( size <= partition_bytes )) || { echo "image larger than partition" >&2; exit 1; }
tmp=$(mktemp -d "${TMPDIR:-/tmp}/b1l-flash.XXXXXX"); trap 'rm -rf "$tmp"' EXIT

guard() {
  $adb shell "[ \"\$(id -u)\" = 0 ] && [ \"\$(getprop ro.boot.em.model)\" = SM-G973F ] &&
    [ \"\$(readlink -f /dev/block/by-name/$name)\" = $block ] &&
    [ \"\$(blockdev --getsize64 $block)\" = $partition_bytes ] &&
    ! grep -q '^$block ' /proc/mounts && echo GUARD_OK" | grep -q GUARD_OK ||
    { echo "guard failed: identity, partition or mount state changed" >&2; exit 1; }
}

guard
count=$(( (size + chunk - 1) / chunk ))
for ((i = 0; i < count; i++)); do
  blocks=$per
  (( (i + 1) * chunk > size )) && blocks=$(( (size - i * chunk) / bs ))
  dd if="$image" of="$tmp/c" bs=$bs skip=$((i * per)) count=$blocks 2>/dev/null
  want=$(shasum -a 256 "$tmp/c" | cut -d' ' -f1)
  have=$($adb shell "dd if=$block bs=$bs skip=$((i * per)) count=$blocks 2>/dev/null | sha256sum" | cut -d' ' -f1)
  if [[ "$have" == "$want" ]]; then echo "chunk $((i+1))/$count already written"; continue; fi
  guard
  $adb push "$tmp/c" /tmp/b1l-chunk >/dev/null
  pushed=$($adb shell "sha256sum /tmp/b1l-chunk" | cut -d' ' -f1)
  [[ "$pushed" == "$want" ]] || { echo "chunk $i corrupted in transfer" >&2; exit 1; }
  $adb shell "dd if=/tmp/b1l-chunk of=$block bs=$bs seek=$((i * per)) conv=notrunc,fsync 2>/dev/null && rm -f /tmp/b1l-chunk && sync"
  have=$($adb shell "echo 3 > /proc/sys/vm/drop_caches; dd if=$block bs=$bs skip=$((i * per)) count=$blocks 2>/dev/null | sha256sum" | cut -d' ' -f1)
  [[ "$have" == "$want" ]] || { echo "chunk $i readback mismatch" >&2; exit 1; }
  echo "chunk $((i+1))/$count written and verified"
done
echo "all $count chunks verified for $image -> $block"
