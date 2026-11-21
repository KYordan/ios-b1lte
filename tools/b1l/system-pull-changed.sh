#!/bin/bash
# system-pull-changed.sh -- bring out/live/b1l-system-live.img up to what the phone's SYSTEM holds now, reading only the
# 4 MiB blocks the phone changed since the last write (as system-add-fast.sh does before a flash). Nothing is written to
# the phone. Used to read back files the phone wrote on SYSTEM (B1L-0086: /b2l-dump, the Metal driver's shader dumps).
set -euo pipefail
T=${S10_PORT:-$HOME/Developer/s10-port}/b1l-tools
A="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
L=${B2L_ROOT:-$HOME/Developer/s10-port/beyond2lte-ios}/out/live
. "$(dirname "${BASH_SOURCE[0]}")/board.sh"
DEV=$B2L_SYSTEM_DEV; BYTES=$B2L_SYSTEM_BYTES
[ "$($A get-state </dev/null)" = recovery ]
cd "$L"
[ -f b1l-system-live.img ] && [ -f b1l-system-live.blockhashes ] || { echo "no live copy to bring up to date" >&2; exit 1; }
bash "$T/hash-device.sh" $DEV $BYTES > phone.blockhashes
[ "$(wc -l < phone.blockhashes)" -eq "$(wc -l < b1l-system-live.blockhashes)" ]
paste -d' ' phone.blockhashes b1l-system-live.blockhashes | awk '$1!=$2{print NR-1, $1}' > changed.blocks
n=$(wc -l < changed.blocks | tr -d ' ')
echo "$n blocks changed on the phone"
[ "$n" -le 256 ] || { echo "too many changed blocks for a partial read" >&2; exit 1; }
while read -r b h <&3; do
	$A exec-out "dd if=$DEV bs=4194304 skip=$b count=1 2>/dev/null" </dev/null > block.tmp
	[ "$(shasum -a 256 block.tmp | cut -c1-64)" = "$h" ] || { echo "block $b: pulled bytes differ from the phone's hash" >&2; exit 1; }
	dd if=block.tmp of=b1l-system-live.img bs=4194304 seek=$b conv=notrunc 2>/dev/null
done 3< changed.blocks
rm -f block.tmp
cp phone.blockhashes b1l-system-live.blockhashes
echo "live copy brought up to the phone ($n blocks)"
