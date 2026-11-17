#!/bin/bash
# Read the first BYTES of a phone partition to a local image in verified 512 MiB chunks.
# Read-only on the phone. Resumable: chunks already present with the right hash are kept.
#
#   pull-chunked.sh BLOCK BYTES OUTPUT [BASE_IMAGE]
# With BASE_IMAGE (what was last written), unchanged chunks are copied locally
# from it instead of being transferred; only changed chunks cross USB.
set -euo pipefail
block=$1 size=$2 out=$3 base=${4:-}
adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
chunk=$((512 * 1024 * 1024)); bs=$((4 * 1024 * 1024)); per=$((chunk / bs))
(( size % chunk == 0 )) || { echo "size must be a multiple of 512 MiB" >&2; exit 1; }
count=$((size / chunk))
touch "$out"
for ((i = 0; i < count; i++)); do
  want=$($adb shell "dd if=$block bs=$bs skip=$((i * per)) count=$per 2>/dev/null | sha256sum" | cut -d' ' -f1)
  have=$(dd if="$out" bs=$bs skip=$((i * per)) count=$per 2>/dev/null | shasum -a 256 | cut -d' ' -f1)
  if [[ "$have" == "$want" ]]; then echo "chunk $((i+1))/$count already local"; continue; fi
  if [[ -n "$base" ]]; then
    dd if="$base" of="$out" bs=$bs skip=$((i * per)) seek=$((i * per)) count=$per conv=notrunc 2>/dev/null
    have=$(dd if="$out" bs=$bs skip=$((i * per)) count=$per 2>/dev/null | shasum -a 256 | cut -d' ' -f1)
    if [[ "$have" == "$want" ]]; then echo "chunk $((i+1))/$count unchanged (copied from base)"; continue; fi
  fi
  $adb exec-out "dd if=$block bs=$bs skip=$((i * per)) count=$per 2>/dev/null" |
    dd of="$out" bs=$bs seek=$((i * per)) conv=notrunc 2>/dev/null
  have=$(dd if="$out" bs=$bs skip=$((i * per)) count=$per 2>/dev/null | shasum -a 256 | cut -d' ' -f1)
  [[ "$have" == "$want" ]] || { echo "chunk $i mismatch after read" >&2; exit 1; }
  echo "chunk $((i+1))/$count read and verified"
done
echo "all $count chunks verified: $out"
