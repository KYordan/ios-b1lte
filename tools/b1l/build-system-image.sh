#!/bin/bash
# Build the SYSTEM image of the B2L_BOARD phone (board.sh; S10 by default): Journaled HFS+ holding the native cache
# b2l-cache.bin, followed by the 16 MiB persistent log region, exactly the
# partition length. Layout rules: xnu bsd/dev/b2l_ufs_aux.c, uefi CacheEfi.c.
# Local files only; touches no phone. No sudo needed.
set -euo pipefail
port="${S10_PORT:-$HOME/Developer/s10-port}"
repo="$port/beyond2lte-ios"
python="${PY_ENV:-$HOME/Developer/tools/mamba/envs/s10}/bin/python3"
root_image="$repo/out/ipsw/ios26-userdata.hfs.img"
out="$repo/out/system"
. "$(dirname "${BASH_SOURCE[0]}")/board.sh"
partition_bytes=$B2L_SYSTEM_BYTES     # SYSTEM measured on the phone (S10 7340032000, S10e 5976883200)
log_bytes=$((16 * 1024 * 1024))       # B2L_SYSTEM_LOG_BYTES
hfs_bytes=$((partition_bytes - log_bytes))
# About 5.52 GiB, as on the S10+. The S10e SYSTEM leaves ~164 MiB for HFS metadata and the
# Metal/Wi-Fi files system-add-fast.sh adds; NATIVE_CACHE_MIB shrinks the cache if they do not fit.
cache_target=$(( ${NATIVE_CACHE_MIB:-5520} * 1024 * 1024 ))
(( cache_target + 128 * 1024 * 1024 <= hfs_bytes )) || { echo "native cache too large for $B2L_BOARD SYSTEM" >&2; exit 1; }
cache_bytes=$(( 8192 + (cache_target - 8192) / 24576 * 24576 ))
image="$out/b1l-system.img"

[[ -f "$root_image" ]] || { echo "missing $root_image" >&2; exit 1; }
[[ ! -e "$image" ]] || { echo "$image exists; move it away first" >&2; exit 1; }
(( hfs_bytes % 4096 == 0 )) || { echo "HFS size not 4 KiB aligned" >&2; exit 1; }
mkdir -p "$out"
rm -f "$out/b1l-system-hfs.cdr" "$out/root-header.bin" "$out/cache-128m.backup" "$out/cache-grow.json" "$out/log-region.bin"

echo "== 1/6 Journaled HFS+ of $hfs_bytes bytes"
hdiutil create -sectors $((hfs_bytes / 512)) -layout NONE -fs "Journaled HFS+" \
  -volname B2LSYSTEM -type UDTO -o "$out/b1l-system-hfs" >/dev/null
hfs="$out/b1l-system-hfs.cdr"
[[ $(stat -f %z "$hfs") -eq $hfs_bytes ]] || { echo "unexpected HFS image length" >&2; exit 1; }

echo "== 2/6 root identity from the iOS root header"
head -c 4096 "$root_image" > "$out/root-header.bin"

echo "== 3/6 create and grow b2l-cache.bin to $cache_bytes bytes"
mnt=$(hdiutil attach -plist -nobrowse -imagekey diskimage-class=CRawDiskImage "$hfs" |
  grep -A1 '<key>mount-point</key>' | grep -o '<string>.*</string>' | sed 's/<[^>]*>//g' | head -1)
[[ -n "$mnt" ]] || { echo "attach failed" >&2; exit 1; }
trap 'hdiutil detach "$mnt" >/dev/null 2>&1 || hdiutil detach -force "$mnt" >/dev/null 2>&1 || true' EXIT
"$python" -I "$repo/scripts/init-native-cache.py" --root-header "$out/root-header.bin" --output "$mnt/b2l-cache.bin"
"$python" -I "$repo/scripts/grow-native-cache.py" --cache "$mnt/b2l-cache.bin" \
  --backup "$out/cache-128m.backup" --bytes "$cache_bytes" --receipt "$out/cache-grow.json"
sync
hdiutil detach "$mnt" >/dev/null
trap - EXIT

echo "== 4/6 fsck and header checks"
dev=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$hfs" | awk 'NR==1{print $1}')
fsck_hfs -fn "$dev"
hdiutil detach "$dev" >/dev/null
"$python" -I - "$hfs" "$hfs_bytes" <<'EOF'
import struct, sys
path, expected = sys.argv[1], int(sys.argv[2])
with open(path, 'rb') as f:
    f.seek(1024); primary = f.read(512)
    f.seek(expected - 1024); alternate = f.read(512)
for name, h in (('primary', primary), ('alternate', alternate)):
    attrs, = struct.unpack_from('>I', h, 4)
    bs, blocks, free = struct.unpack_from('>III', h, 40)
    ok = h[:4] == b'H+\0\4' and attrs & (1 << 13) and bs * blocks == expected and free <= blocks
    print(f'{name}: block={bs} blocks={blocks} free={free} journaled={bool(attrs & 1 << 13)} ok={bool(ok)}')
    if not ok: sys.exit('SYSTEM HFS header rejected by the B2L layout rules')
if primary[:4] != alternate[:4] or primary[40:48] != alternate[40:48]:
    sys.exit('primary and alternate headers disagree')
EOF

echo "== 5/6 16 MiB persistent log region"
"$python" -I "$repo/scripts/b2l-log-region.py" init "$out/log-region.bin"
[[ $(stat -f %z "$out/log-region.bin") -eq $log_bytes ]] || { echo "unexpected log region length" >&2; exit 1; }

echo "== 6/6 assemble SYSTEM image"
cat "$hfs" "$out/log-region.bin" > "$image"
[[ $(stat -f %z "$image") -eq $partition_bytes ]] || { echo "SYSTEM image length mismatch" >&2; exit 1; }
rm -f "$hfs"
shasum -a 256 "$image" | tee "$image.sha256"
echo "done: $image"
