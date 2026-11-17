#!/bin/bash
# flash-boot.sh IMAGE SHA256 -- guarded BOOT write + readback (all adb calls with </dev/null)
set -e; adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"; img=$1; want=$2; size=$(stat -f %z $img)
[ "$($adb get-state 2>/dev/null </dev/null)" = recovery ] || { echo not-in-twrp; exit 1; }
[ "$(shasum -a 256 $img | cut -d' ' -f1)" = $want ] || { echo host-hash-bad; exit 1; }
$adb shell "[ \"\$(id -u)\" = 0 ] && [ \"\$(getprop ro.boot.em.model)\" = SM-G973F ] && [ \"\$(readlink -f /dev/block/by-name/boot)\" = /dev/block/sda14 ] && [ \"\$(blockdev --getsize64 /dev/block/sda14)\" = 57671680 ] && ! grep -q '^/dev/block/sda14 ' /proc/mounts && echo GUARD_OK" </dev/null | grep -q GUARD_OK || { echo guard-failed; exit 1; }
echo guards ok; $adb push $img /tmp/b1l-boot.img </dev/null
[ "$($adb shell sha256sum /tmp/b1l-boot.img </dev/null | cut -c1-64)" = $want ] || { echo push-bad; exit 1; }
$adb shell "dd if=/tmp/b1l-boot.img of=/dev/block/sda14 bs=4096 conv=fsync && sync && rm -f /tmp/b1l-boot.img" </dev/null
got=$($adb shell "echo 3 > /proc/sys/vm/drop_caches; dd if=/dev/block/sda14 bs=$size count=1 2>/dev/null | sha256sum" </dev/null | cut -c1-64)
echo "boot readback $got"; [ "$got" = $want ] && echo BOOT-OK
