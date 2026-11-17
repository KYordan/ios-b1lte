#!/bin/bash
# pull-delta.sh BLOCKDEV BYTES OUTPUT BASE -- copy of a phone partition into OUTPUT, reading over USB only the 4 MiB
# blocks that differ from BASE (the last pull). Read-only on the phone.
#   1. the phone hashes its partition, one sha256 per 4 MiB block, 8 workers (hash-device.sh: TWRP, model, size guards);
#   2. BASE's block hashes come from BASE.blockhashes (made once on the Mac, then saved by every pull);
#   3. OUTPUT = APFS clone of BASE (instant), then each run of changed blocks is read in one go and every block of it
#      is checked against the phone's hash; OUTPUT.blockhashes is saved for the next pull.
set -euo pipefail
dev=$1 size=$2 out=$3 base=$4
T=$(cd "$(dirname "$0")" && pwd)
adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
bs=$((4 * 1024 * 1024)); n=$((size / bs)); maxrun=64   # one read at most 256 MiB
start=$(date +%s)

echo "phone: hashing $dev ($n blocks)..."
HASH_PREFIX=1 bash "$T/hash-device.sh" "$dev" "$size" > "$out.phone.tmp"
echo "phone hashes in $(( $(date +%s) - start )) s"

if [ ! -f "$base.blockhashes" ] || [ "$(wc -l < "$base.blockhashes")" -ne "$n" ]; then
	echo "Mac: hashing $base once (saved as $base.blockhashes)..."
	python3 -I "$T/hash-image.py" "$base" | cut -d' ' -f2 > "$base.blockhashes.tmp"
	mv "$base.blockhashes.tmp" "$base.blockhashes"
fi
[ "$(wc -l < "$base.blockhashes")" -eq "$n" ]

rm -f "$out"; cp -c "$base" "$out"
# runs of changed blocks: "first count"
paste -d' ' "$out.phone.tmp" "$base.blockhashes" | awk -v m=$maxrun '
	$1 != $2 { i = NR - 1; if (c && i == s + c && c < m) c++; else { if (c) print s, c; s = i; c = 1 } }
	END { if (c) print s, c }' > "$out.runs"
changed=$(awk '{t += $2} END {print t + 0}' "$out.runs")
echo "$changed of $n blocks changed ($((changed * 4)) MiB to read)"

done_=0
while read -r s c <&3; do
	$adb exec-out "dd if=$dev bs=$bs skip=$s count=$c 2>/dev/null" </dev/null |
		dd of="$out" bs=$bs seek=$s conv=notrunc 2>/dev/null
	python3 -I "$T/hash-image.py" "$out" $(seq $s $((s + c - 1))) | while read -r i h; do
		[ "$h" = "$(sed -n "$((i + 1))p" "$out.phone.tmp")" ] || { echo "block $i: read bytes differ from the phone's hash" >&2; exit 1; }
	done
	done_=$((done_ + c)); echo "read and verified $done_/$changed blocks"
done 3< "$out.runs"

mv "$out.phone.tmp" "$out.blockhashes"; rm -f "$out.runs"
echo "PULL-OK $out in $(( $(date +%s) - start )) s"
