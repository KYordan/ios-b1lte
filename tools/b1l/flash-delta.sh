#!/bin/bash
# Write only the 4 MiB blocks of IMAGE that differ from the phone partition.
# Same guards as flash-chunked.sh. Block hashes are computed on both sides; each
# run of differing blocks (max 64 blocks = 256 MiB) is pushed, hash-checked,
# written with fsync, and read back. The final pass re-hashes every block.
#   flash-delta.sh IMAGE BY-NAME EXPECTED_BLOCK EXPECTED_PARTITION_BYTES
set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/board.sh"
image=$1 name=$2 expect=$3 psize=$4
adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
# adb shell reads stdin: every call below must not consume the caller's input.
bs=$((4 * 1024 * 1024)); maxrun=64
size=$(stat -f %z "$image")
(( size % bs == 0 )) || { echo "image size not a multiple of 4 MiB" >&2; exit 1; }
n=$((size / bs))
[[ "$($adb get-state)" == recovery ]] || { echo "not in TWRP" >&2; exit 1; }
[[ "$($adb shell id -u | tr -d '\r')" == 0 ]] || { echo "no root" >&2; exit 1; }
[[ "$($adb shell getprop ro.boot.em.model | tr -d '\r')" == "$B2L_MODEL" ]] || { echo "wrong model" >&2; exit 1; }
[[ "$($adb shell readlink -f /dev/block/by-name/$name | tr -d '\r')" == "$expect" ]] || { echo "by-name mismatch" >&2; exit 1; }
[[ "$($adb shell blockdev --getsize64 $expect | tr -d '\r')" == "$psize" ]] || { echo "size mismatch" >&2; exit 1; }
(( size <= psize )) || { echo "image larger than partition" >&2; exit 1; }
[[ "$($adb shell "grep -c ${expect##*/} /proc/mounts" | tr -d '\r')" == 0 ]] || { echo "$expect is mounted" >&2; exit 1; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

hash_device() {   # one sha256 per 4 MiB block, in order; 8 workers on contiguous ranges
  local per=$(( (n + 7) / 8 ))
  $adb shell "rm -f /tmp/b1l-h.*; w=0; while [ \$w -lt 8 ]; do \
      ( i=\$((w * $per)); e=\$(( (w + 1) * $per )); [ \$e -gt $n ] && e=$n; \
        while [ \$i -lt \$e ]; do echo \"\$i \$(dd if=$expect bs=$bs skip=\$i count=1 2>/dev/null | sha256sum | cut -c1-64)\"; i=\$((i+1)); done > /tmp/b1l-h.\$w ) & \
      w=\$((w+1)); done; wait; cat /tmp/b1l-h.0 /tmp/b1l-h.1 /tmp/b1l-h.2 /tmp/b1l-h.3 /tmp/b1l-h.4 /tmp/b1l-h.5 /tmp/b1l-h.6 /tmp/b1l-h.7; rm -f /tmp/b1l-h.*" </dev/null |
    tr -d '\r' | awk 'BEGIN{p=-1} {if($1!=p+1){print "HASH ORDER ERROR" > "/dev/stderr"; exit 1} p=$1; print $2}'
}
hash_host() {
  python3 -I - "$image" "$bs" <<'EOF'
import hashlib,sys
bs=int(sys.argv[2])
with open(sys.argv[1],'rb',buffering=0) as f:
    while True:
        b=f.read(bs)
        if not b: break
        print(hashlib.sha256(b).hexdigest())
EOF
}
echo "hashing $n blocks on host and phone..."
hash_host > "$tmp/host" & hp=$!
if [[ -n "${DEV_HASHES:-}" ]]; then
  # (system-add-fast.sh: the phone's block hashes were taken a moment ago, guards included; not read twice)
  cp "$DEV_HASHES" "$tmp/dev"; echo "phone block hashes reused from $DEV_HASHES"
else
  hash_device > "$tmp/dev"
fi
wait $hp
[[ -n "${SAVE_HOST_HASHES:-}" ]] && cp "$tmp/host" "$SAVE_HOST_HASHES"
[[ $(wc -l < "$tmp/host") -eq $n && $(wc -l < "$tmp/dev") -eq $n ]] || { echo "hash list incomplete" >&2; exit 1; }
paste -d' ' "$tmp/host" "$tmp/dev" | awk '$1!=$2{print NR-1}' > "$tmp/diff"
changed=$(wc -l < "$tmp/diff" | tr -d ' ')
echo "differing blocks: $changed of $n ($((changed * 4)) MiB to write)"
(( changed )) || { echo "phone already matches image"; exit 0; }

# Group consecutive blocks into runs.
awk -v m=$maxrun 'NR==1{s=$1;p=$1;next} {if($1==p+1 && $1-s<m){p=$1}else{print s,p;s=$1;p=$1}} END{print s,p}' "$tmp/diff" > "$tmp/runs"
runs=$(wc -l < "$tmp/runs" | tr -d ' '); k=0
while read -r s e <&3; do
  k=$((k + 1)); cnt=$((e - s + 1))
  dd if="$image" of="$tmp/r" bs=$bs skip=$s count=$cnt 2>/dev/null
  want=$(shasum -a 256 "$tmp/r" | cut -d' ' -f1)
  $adb push "$tmp/r" /tmp/b1l-run >/dev/null
  [[ "$($adb shell sha256sum /tmp/b1l-run | cut -c1-64)" == "$want" ]] || { echo "run $k corrupted in transfer" >&2; exit 1; }
  $adb shell "dd if=/tmp/b1l-run of=$expect bs=$bs seek=$s conv=notrunc,fsync 2>/dev/null && rm -f /tmp/b1l-run && sync"
  have=$($adb shell "echo 3 > /proc/sys/vm/drop_caches; dd if=$expect bs=$bs skip=$s count=$cnt 2>/dev/null | sha256sum" | cut -c1-64)
  [[ "$have" == "$want" ]] || { echo "run $k readback mismatch (blocks $s-$e)" >&2; exit 1; }
  echo "run $k/$runs blocks $s-$e ($((cnt * 4)) MiB) written and verified"
done 3< "$tmp/runs"

if [[ "${FULL_CHECK:-0}" == 1 ]]; then
  echo "final check: re-hashing the phone..."
  hash_device > "$tmp/dev2"
  if cmp -s "$tmp/host" "$tmp/dev2"; then echo "all $n blocks verified for $image -> $expect"
  else echo "FINAL MISMATCH" >&2; exit 1; fi
else
  echo "all $runs runs written and read back; unchanged blocks matched before writing (FULL_CHECK=1 re-hashes everything)"
fi
