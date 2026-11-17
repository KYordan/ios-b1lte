#!/bin/bash
# flash-saved.sh NNNN [--save SET] -- reflash a saved build (SYSTEM libs + BOOT), then boot iOS.
#   save once:  bash flash-saved.sh 0128 --save b2l-metal-v77   (copies boot.img and the set into saved-builds/0128)
#   flash:      bash flash-saved.sh 0128                         (phone in TWRP; same guards as every build)
set -euo pipefail
N=${1:?usage: flash-saved.sh NNNN [--save SET]}
S=${S10_PORT:-$HOME/Developer/s10-port}; D=$S/saved-builds/$N; T=$S/b1l-tools
adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
if [ "${2:-}" = --save ]; then
	SET=${3:?--save needs the set name, e.g. b2l-metal-v77}
	mkdir -p "$D"
	cp "$S/beyond2lte-ios/out/b1l-metal-$N/boot.img" "$D/boot.img"
	rm -rf "$D/$SET"; cp -R "$S/build-metal/$SET" "$D/$SET"
	echo "$SET" > "$D/set"; shasum -a 256 "$D/boot.img" | cut -c1-64 > "$D/boot.sha256"
	echo "saved B1L-$N ($SET) in $D"; exit 0
fi
[ -f "$D/boot.img" ] || { echo "no saved build $N in $D"; exit 1; }
[ "$($adb get-state 2>/dev/null </dev/null)" = recovery ] || { echo "put the phone in TWRP first"; exit 1; }
SET=$(cat "$D/set"); H=$(cat "$D/boot.sha256")
echo "B1L-$N: SYSTEM ($SET)..."; GL_BUILD=$D bash "$T/system-add-fast.sh" "$SET" | grep -E 'differing|SYSTEM-OK'
echo "B1L-$N: BOOT..."; bash "$T/flash-boot.sh" "$D/boot.img" "$H" | tail -2 | tee /dev/stderr | grep -q BOOT-OK
$adb reboot </dev/null; echo "B1L-$N flashed, rebooting to iOS"
