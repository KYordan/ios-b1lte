#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# ipsw-launchd.sh IPSW-or-URL OUT.plist -- macOS only (aea, hdiutil).
# Take /System/Library/xpc/launchd.plist unmodified from the IPSW's OS image, for
# scripts/build-userspace-bootstrap.py (B2L_LAUNCHD_PLIST) when no iOS root image exists,
# e.g. on a CI runner. Only the OS member is fetched (HTTP Range for a URL); its AEA key
# comes from Apple's public WKMS server (tools/b1l/aea-key.py).
set -euo pipefail
src=$1 out=$2
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
py="${PYTHON_COMMAND:-python3}"
work="$(mktemp -d)"; mnt="$work/mnt"; mkdir -p "$mnt"
trap 'hdiutil detach "$mnt" -force >/dev/null 2>&1 || true; rm -rf "$work"' EXIT
read -r name start size < <("$py" "$here/zip-member.py" "$src" --manifest-key OS)
echo "OS image $name: $size bytes at $start"
if [[ "$src" == http* ]]; then
  curl -fsS --retry 3 -r "$start-$((start + size - 1))" "$src" -o "$work/os.aea"
else
  "$py" - "$src" "$start" "$size" "$work/os.aea" <<'EOF'
import sys
src, start, size, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
with open(src, 'rb') as f, open(out, 'wb') as o:
    f.seek(start)
    while size:
        block = f.read(min(size, 1 << 24))
        if not block:
            sys.exit('IPSW ends inside the OS image')
        o.write(block)
        size -= len(block)
EOF
fi
[[ $(stat -f %z "$work/os.aea") -eq $size ]] || { echo "short OS image" >&2; exit 1; }
head -c 65536 "$work/os.aea" > "$work/os.head"
key="$("$py" "$here/aea-key.py" "$work/os.head")"
aea decrypt -i "$work/os.aea" -o "$work/os.dmg" -key-value "$key"
rm -f "$work/os.aea"
hdiutil attach "$work/os.dmg" -mountpoint "$mnt" -nobrowse -readonly >/dev/null
cp "$mnt/System/Library/xpc/launchd.plist" "$out"
[[ "$(head -c 8 "$out")" == bplist00 ]] || { echo "unexpected launchd.plist" >&2; exit 1; }
shasum -a 256 "$out"
