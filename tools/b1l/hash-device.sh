#!/bin/bash
# hash-device.sh BLOCKDEV BYTES: one sha256 per 4 MiB block of the partition, in order, computed ON THE PHONE (8 workers);
# nothing crosses USB but the hashes. Read-only. Same guards for the device as flash-delta.sh.
set -euo pipefail
expect=$1 psize=$2
adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
bs=$((4 * 1024 * 1024)); n=$((psize / bs)); per=$(( (n + 7) / 8 ))
[[ "$($adb get-state </dev/null)" == recovery ]] || { echo "not in TWRP" >&2; exit 1; }
[[ "$($adb shell getprop ro.boot.em.model </dev/null | tr -d '\r')" == SM-G973F ]] || { echo "wrong model" >&2; exit 1; }
dsize=$($adb shell blockdev --getsize64 $expect </dev/null | tr -d '\r')
# HASH_PREFIX=1 (pull-delta.sh, read-only): hash only the first BYTES of a larger partition
[[ "$dsize" == "$psize" ]] || { [[ "${HASH_PREFIX:-0}" == 1 && "$dsize" -gt "$psize" ]]; } || { echo "size mismatch" >&2; exit 1; }
$adb shell "rm -f /tmp/b1l-h.*; w=0; while [ \$w -lt 8 ]; do \
    ( i=\$((w * $per)); e=\$(( (w + 1) * $per )); [ \$e -gt $n ] && e=$n; \
      while [ \$i -lt \$e ]; do echo \"\$i \$(dd if=$expect bs=$bs skip=\$i count=1 2>/dev/null | sha256sum | cut -c1-64)\"; i=\$((i+1)); done > /tmp/b1l-h.\$w ) & \
    w=\$((w+1)); done; wait; cat /tmp/b1l-h.0 /tmp/b1l-h.1 /tmp/b1l-h.2 /tmp/b1l-h.3 /tmp/b1l-h.4 /tmp/b1l-h.5 /tmp/b1l-h.6 /tmp/b1l-h.7; rm -f /tmp/b1l-h.*" </dev/null |
  tr -d '\r' | awk -v n=$n 'BEGIN{p=-1} {if($1!=p+1){print "HASH ORDER ERROR" > "/dev/stderr"; exit 1} p=$1; print $2} END{if(p!=n-1){print "HASH COUNT ERROR" > "/dev/stderr"; exit 1}}'
