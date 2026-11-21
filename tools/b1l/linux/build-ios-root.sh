#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Linux/WSL version of tools/b1l/build-ios-root.sh: the 32 GiB case-sensitive Journaled
# HFS+ iOS 26 root for USERDATA, with the PAC translation of build B1L-0250.
# Run as root. Touches no phone.
#
#   build-ios-root.sh [WORK]       (default WORK=/root/s10e)
#
# Needs: WORK/ipsw-23G90/{OS,SystemOS,AppOS}.dmg decrypted (tools/b1l/zip-member.py,
# tools/b1l/aea-key.py, `ipsw fw aea --key-val`), apfs-fuse, hfsprogs (mkfs.hfsplus,
# fsck.hfsplus), a kernel with HFS+ (WSL: HFSPLUS_KO=/path/hfsplus.ko), cc, python3.
# Same steps as the macOS script: hdiutil -> truncate/mkfs.hfsplus/loop mount,
# APFS attach -> apfs-fuse; copy-tree.py, prepare-ios-root.py and nc-static unchanged.
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo "run as root" >&2; exit 1; }
work=${1:-/root/s10e}
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ipsw="$work/ipsw-23G90"
out="$work/out"
image="$out/ios26-userdata.hfs.img"
log="$out/build-ios-root.log"
nc="$work/bin/nc-static"
mnt="$work/mnt"
rel=private/preboot/Cryptexes/OS/System/Library/Caches/com.apple.dyld

for t in apfs-fuse mkfs.hfsplus fsck.hfsplus cc python3; do
  command -v "$t" >/dev/null || { echo "missing $t" >&2; exit 1; }
done
for d in OS SystemOS AppOS; do [[ -f "$ipsw/$d.dmg" ]] || { echo "missing $ipsw/$d.dmg" >&2; exit 1; }; done
if ! grep -qw hfsplus /proc/filesystems; then
  modprobe hfsplus 2>/dev/null || insmod "${HFSPLUS_KO:?kernel has no HFS+; set HFSPLUS_KO to a matching hfsplus.ko}"
fi
mkdir -p "$out" "$work/bin" "$mnt"/{root,os,systemos,appos}
if [[ -e "$image" ]]; then
  old="$image.before-$(date -r "$image" +%Y%m%d-%H%M)"
  [[ ! -e "$old" ]] || { echo "$old already exists; move it away first" >&2; exit 1; }
  mv "$image" "$old"; [[ ! -e "$image.sha256" ]] || mv "$image.sha256" "$old.sha256"
  echo "earlier image kept as $old"
fi
exec > >(tee "$log") 2>&1

cleanup() {
  for m in appos systemos os; do mountpoint -q "$mnt/$m" && fusermount3 -u "$mnt/$m" || true; done
  if mountpoint -q "$mnt/root"; then sync; umount "$mnt/root" || umount -l "$mnt/root" || true; fi
}
trap cleanup EXIT

echo "== 0/7 nc-static"
cc -std=c11 -O2 -D_GNU_SOURCE -I"$repo/common/cache" -I"$repo/xnu-overlay/bsd/kern" \
  "$repo/tools/b1l/static-translate/nc-static.c" "$repo/common/cache/b2l_cache_core.c" \
  "$repo/common/cache/CacheSha.c" -o "$nc"

echo "== 1/7 raw 32 GiB case-sensitive Journaled HFS+"
truncate -s 32G "$image"
mkfs.hfsplus -s -J -v B2LIOSROOT "$image" >/dev/null

echo "== 2/7 mount"
mount -t hfsplus -o loop,force,rw "$image" "$mnt/root"
grep -E " $mnt/root hfsplus .*\brw\b" /proc/mounts >/dev/null || { echo "HFS+ root is not mounted read-write" >&2; exit 1; }
apfs-fuse "$ipsw/OS.dmg" "$mnt/os"
apfs-fuse "$ipsw/SystemOS.dmg" "$mnt/systemos"
apfs-fuse "$ipsw/AppOS.dmg" "$mnt/appos"
src_os="$mnt/os/root" src_sys="$mnt/systemos/root" src_app="$mnt/appos/root"
[[ -x "$src_os/sbin/launchd" && -d "$src_sys/System/Library/Caches/com.apple.dyld" ]] || {
  echo "unexpected source layout" >&2; exit 1; }

echo "== 3/7 copy OS"
python3 -I "$repo/tools/b1l/copy-tree.py" "$src_os" "$mnt/root" --report "$out/copy-os.json"
echo "== 4/7 copy cryptexes"
mkdir -p "$mnt/root/private/preboot/Cryptexes"
python3 -I "$repo/tools/b1l/copy-tree.py" "$src_sys" "$mnt/root/private/preboot/Cryptexes/OS" --report "$out/copy-systemos.json"
python3 -I "$repo/tools/b1l/copy-tree.py" "$src_app" "$mnt/root/private/preboot/Cryptexes/App" --report "$out/copy-appos.json"

echo "== 5/7 prepare-ios-root.py (dry run, apply, verify)"
python3 -I "$repo/scripts/prepare-ios-root.py" "$mnt/root" --dry-run
python3 -I "$repo/scripts/prepare-ios-root.py" "$mnt/root" --report "$out/prepare-report.json"
python3 -I "$repo/scripts/prepare-ios-root.py" "$mnt/root" --verify

echo "== 5b/7 PAC translation (B1L-0011/0012/0250)"
orig="$work/static-work/orig"; tr="$work/static-work/out"
rm -rf "$tr"; mkdir -p "$orig" "$tr"
mapfile -t cache < <(ls "$src_sys/System/Library/Caches/com.apple.dyld/"dyld_shared_cache_arm64e* | grep -vE '\.(atlas|symbols)$' | sort -V)
[[ ${#cache[@]} -eq 84 ]] || { echo "expected 84 cache files, found ${#cache[@]}" >&2; exit 1; }
for f in "${cache[@]}"; do cmp -s "$f" "$orig/$(basename "$f")" 2>/dev/null || cp "$f" "$orig/"; done
names=(); for f in "${cache[@]}"; do names+=("$(basename "$f")"); done
main_first=("$orig/dyld_shared_cache_arm64e"); for n in "${names[@]}"; do [[ $n == dyld_shared_cache_arm64e ]] || main_first+=("$orig/$n"); done
"$nc" apply "$tr" "${main_first[@]}"
tr_files=(); for f in "${main_first[@]}"; do tr_files+=("$tr/$(basename "$f")"); done
# MediaServices objc_release on an error path (B1L-0011): exact targets checked by nop-call.
"$nc" nop-call "${tr_files[@]}" -- 0x1ae8593f8 0x1b2571fe0
"$nc" nop-call "${tr_files[@]}" -- 0x1ae85d7f0 0x1b2571ff0
for n in "${names[@]}"; do "$nc" compare "$orig/$n" "$tr/$n" > "$out/compare-$n.txt" 2>&1 || {
  echo "compare reported differences for $n (see $out/compare-$n.txt)"; }; done
n=0
for name in "${names[@]}"; do
  dst="$mnt/root/$rel/$name"
  [[ -f "$dst" ]] || { echo "missing target $name" >&2; exit 1; }
  cmp -s "$orig/$name" "$dst" || { echo "$name in the image is not the original cache" >&2; exit 1; }
  meta=$(stat -c '%u:%g %a %i' "$dst")
  dd if="$tr/$name" of="$dst" bs=4M conv=notrunc status=none
  cmp -s "$tr/$name" "$dst" || { echo "verify failed: $name" >&2; exit 1; }
  [[ $(stat -c '%u:%g %a %i' "$dst") == "$meta" ]] || { echo "metadata changed: $name" >&2; exit 1; }
  n=$((n + 1))
done
echo "cache files translated: $n"
echo "standalone arm64e Mach-O files (outside the cache and /private/var, plus the staged apps), in place"
macho_files() {
  find "$mnt/root" -xdev -type f -size +8k ! -path '*/com.apple.dyld/*' ! -path "$mnt/root/private/var/*" -print0
  [[ ! -d "$mnt/root/private/var/staged_system_apps" ]] || find "$mnt/root/private/var/staged_system_apps" -xdev -type f -size +8k -print0
}
macho_files | xargs -0 "$nc" macho-apply | awk '/^macho-apply|^MACHO|^  /' | sort | uniq -c | sort -rn | head -20
echo "check: no convertible words left"
macho_files | xargs -0 "$nc" macho 2>/dev/null | grep -E 'MACHO TOTAL' | sort | uniq -c

echo "== 6/7 unmount and fsck"
cleanup; trap - EXIT
fsck.hfsplus -f -n "$image"

echo "== 7/7 hash"
sha256sum "$image" | tee "$image.sha256"
echo "done: $image"
