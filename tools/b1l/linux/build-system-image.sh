#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Linux/WSL version of tools/b1l/build-system-image.sh: the SYSTEM image of the B2L_BOARD phone,
# a Journaled HFS+ holding the native cache b2l-cache.bin, followed by the 16 MiB persistent log
# region, exactly the partition length. Run as root. Touches no phone.
#
#   build-system-image.sh ROOT_IMAGE [WORK]     (default WORK=/root/s10e)
#
# WIFI_DIR=dir copies fw.bin, nvram.txt, clm.blob, bt.hcd into /b2l-wifi (B1L-0149/0212).
# The result is checked with the firmware's own parser (common/cache/CacheHfs.c via
# tests/native_cache_layout_test.c).
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run as root" >&2; exit 1; }
root_image=$1; work=${2:-/root/s10e}
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
. "$repo/tools/b1l/board.sh"
out="$work/out"; mnt="$work/mnt/system"
partition_bytes=$B2L_SYSTEM_BYTES
log_bytes=$((16 * 1024 * 1024))       # B2L_SYSTEM_LOG_BYTES
hfs_bytes=$((partition_bytes - log_bytes))
cache_target=$(( ${NATIVE_CACHE_MIB:-5520} * 1024 * 1024 ))
cache_bytes=$(( 8192 + (cache_target - 8192) / 24576 * 24576 ))
image="$out/$B2L_BOARD-system.img"; hfs="$out/$B2L_BOARD-system-hfs.img"

[[ -f "$root_image" ]] || { echo "missing $root_image" >&2; exit 1; }
[[ ! -e "$image" ]] || { echo "$image exists; move it away first" >&2; exit 1; }
(( hfs_bytes % 4096 == 0 )) || { echo "HFS size not 4 KiB aligned" >&2; exit 1; }
(( cache_target + 128 * 1024 * 1024 <= hfs_bytes )) || { echo "native cache too large for $B2L_BOARD SYSTEM" >&2; exit 1; }
grep -qw hfsplus /proc/filesystems || modprobe hfsplus 2>/dev/null || insmod "${HFSPLUS_KO:?kernel has no HFS+; set HFSPLUS_KO}"
mkdir -p "$out" "$mnt"
rm -f "$hfs" "$out/root-header.bin" "$out/cache-128m.backup" "$out/cache-grow.json" "$out/log-region.bin"

echo "== 1/6 Journaled HFS+ of $hfs_bytes bytes ($B2L_BOARD)"
truncate -s "$hfs_bytes" "$hfs"
mkfs.hfsplus -J -v B2LSYSTEM "$hfs" >/dev/null
# Linux hfsplus allocates whole data clumps and keeps them; the firmware requires the allocated
# extents to equal the logical size exactly. Pick the largest cache size of the B2L form
# (8192 + n * 24576) that is also a multiple of the volume's dataClumpSize.
clump=$(python3 -c "import struct,sys; f=open(sys.argv[1],'rb'); f.seek(1024+60); print(struct.unpack('>I',f.read(4))[0])" "$hfs")
n=$(( (cache_target - 8192) / 24576 ))
for _ in $(seq 64); do (( (8192 + n * 24576) % clump == 0 )) && break; n=$((n - 1)); done
(( (8192 + n * 24576) % clump == 0 )) || { echo "no cache size aligned to clump $clump" >&2; exit 1; }
cache_bytes=$(( 8192 + n * 24576 ))
echo "data clump $clump bytes; native cache $cache_bytes bytes ($n cells)"

echo "== 2/6 root identity from the iOS root header"
head -c 4096 "$root_image" > "$out/root-header.bin"

echo "== 3/6 create and grow b2l-cache.bin to $cache_bytes bytes"
mount -t hfsplus -o loop,force,rw "$hfs" "$mnt"
trap 'mountpoint -q "$mnt" && umount "$mnt" || true' EXIT
python3 -I "$repo/scripts/init-native-cache.py" --root-header "$out/root-header.bin" --output "$mnt/b2l-cache.bin"
python3 -I "$repo/scripts/grow-native-cache.py" --cache "$mnt/b2l-cache.bin" \
  --backup "$out/cache-128m.backup" --bytes "$cache_bytes" --receipt "$out/cache-grow.json"
if [[ -n "${WIFI_DIR:-}" ]]; then
  mkdir -p "$mnt/b2l-wifi"
  for f in fw.bin nvram.txt clm.blob bt.hcd; do [[ -f "$WIFI_DIR/$f" ]] && cp "$WIFI_DIR/$f" "$mnt/b2l-wifi/$f"; done
  sha256sum "$mnt/b2l-wifi"/*
fi
sync; umount "$mnt"; trap - EXIT

echo "== 4/6 journal init (as newfs_hfs does), fsck, header and firmware-parser checks"
python3 -I "$repo/tools/b1l/linux/hfs-journal-init.py" "$hfs"
fsck.hfsplus -f -n "$hfs"
python3 -I - "$hfs" "$hfs_bytes" <<'EOF'
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
# The loader (uefi/XnuLoader/CacheEfi.c) and the kernel (bsd/dev/b2l_ufs_disk.c) map the cache through
# b2l_hfs_cache_view, which accepts up to eight inline extents: mkfs.hfsplus puts the catalog in the
# middle of the volume, so the grown cache has two. Same check as the macOS grow (records preserved,
# cross-extent writes, metadata rejected).
cc -std=c11 -O2 -D_GNU_SOURCE -I"$repo/common/cache" "$repo/tests/native_cache_layout_test.c" "$repo/common/cache/CacheHfs.c" \
  "$repo/common/cache/b2l_cache_core.c" "$repo/common/cache/CacheSha.c" -o "$work/bin/native_cache_layout_test"
"$work/bin/native_cache_layout_test" "$hfs" "$out/cache-128m.backup"

echo "== 5/6 16 MiB persistent log region"
python3 -I "$repo/scripts/b2l-log-region.py" init "$out/log-region.bin"
[[ $(stat -c %s "$out/log-region.bin") -eq $log_bytes ]] || { echo "unexpected log region length" >&2; exit 1; }

echo "== 6/6 assemble SYSTEM image"
cat "$hfs" "$out/log-region.bin" > "$image"
[[ $(stat -c %s "$image") -eq $partition_bytes ]] || { echo "SYSTEM image length mismatch" >&2; exit 1; }
rm -f "$hfs"
sha256sum "$image" | tee "$image.sha256"
echo "done: $image"
