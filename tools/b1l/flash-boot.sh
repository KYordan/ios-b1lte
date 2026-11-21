#!/bin/bash
# flash-boot.sh IMAGE SHA256 -- guarded BOOT write + readback (all adb calls with </dev/null)
# The phone (model, BOOT block and size) comes from board.sh: B2L_BOARD=beyond0lte for the S10e.
set -e; . "$(dirname "${BASH_SOURCE[0]}")/board.sh"
adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"; img=$1; want=$2; size=$(b2l_size "$img")
dev=$B2L_BOOT_DEV
[ "$size" -le "$B2L_BOOT_BYTES" ] || { echo image-too-large; exit 1; }
[ "$($adb get-state 2>/dev/null </dev/null)" = recovery ] || { echo not-in-twrp; exit 1; }
[ "$(shasum -a 256 $img | cut -d' ' -f1)" = $want ] || { echo host-hash-bad; exit 1; }
$adb shell "[ \"\$(id -u)\" = 0 ] && [ \"\$(getprop ro.boot.em.model)\" = $B2L_MODEL ] && [ \"\$(readlink -f /dev/block/by-name/boot)\" = $dev ] && [ \"\$(blockdev --getsize64 $dev)\" = $B2L_BOOT_BYTES ] && ! grep -q '^$dev ' /proc/mounts && echo GUARD_OK" </dev/null | grep -q GUARD_OK || { echo guard-failed; exit 1; }
echo "guards ok ($B2L_BOARD $B2L_MODEL)"; $adb push $img /tmp/b1l-boot.img </dev/null
[ "$($adb shell sha256sum /tmp/b1l-boot.img </dev/null | cut -c1-64)" = $want ] || { echo push-bad; exit 1; }
$adb shell "dd if=/tmp/b1l-boot.img of=$dev bs=4096 conv=fsync && sync && rm -f /tmp/b1l-boot.img" </dev/null
got=$($adb shell "echo 3 > /proc/sys/vm/drop_caches; dd if=$dev bs=$size count=1 2>/dev/null | sha256sum" </dev/null | cut -c1-64)
echo "boot readback $got"; [ "$got" = $want ] && echo BOOT-OK
