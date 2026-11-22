#!/usr/bin/env bash
# Write a CI boot.img to the S10e's BOOT partition (sda14) from WSL, the phone in TWRP over USB.
#   wsl -e bash /mnt/c/Projects/ios-b1lte/tools/b1l/linux/flash-boot-s10e.sh [ARTIFACT_DIR]
# ARTIFACT_DIR (default: beyond0lte-boot/ in the repository) holds the extracted beyond0lte-boot-N archive.
# Checks the artifact's SHA256SUMS, links /dev/block/by-name (this TWRP has none), then tools/b1l/flash-boot.sh:
# model/block/size guards, push, dd, read-back. Only BOOT is written; the iOS data (SYSTEM, USERDATA) is untouched.
set -euo pipefail
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
dir="${1:-$repo/beyond0lte-boot}"
export B2L_BOARD=beyond0lte
export ADB="${ADB:-$repo/tools/b1l/linux/adb-wsl}"
export ADB_EXE="${ADB_EXE:-/mnt/c/platform-tools/adb.exe}"
[ -x "$ADB_EXE" ] || { echo "adb.exe not found at $ADB_EXE (set ADB_EXE)"; exit 1; }

echo "== artifact: $dir"
(cd "$dir" && sha256sum -c SHA256SUMS)
want=$(awk '$2=="boot.img"{print $1}' "$dir/SHA256SUMS")
[ -n "$want" ] || { echo "boot.img missing from SHA256SUMS"; exit 1; }

echo "== phone"
state=$("$ADB" get-state 2>/dev/null </dev/null || true)
[ "$state" = recovery ] || { echo "phone not in TWRP (adb get-state: '${state:-none}')"; exit 1; }
"$ADB" shell "[ -e /dev/block/by-name ] || ln -s /dev/block/platform/13d60000.ufs/by-name /dev/block/by-name" </dev/null

echo "== write BOOT (sda14) and read it back"
bash "$repo/tools/b1l/flash-boot.sh" "$dir/boot.img" "$want"
