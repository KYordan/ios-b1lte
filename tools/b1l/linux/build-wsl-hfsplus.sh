#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# Build hfsplus.ko for the running WSL2 kernel, which ships without HFS+ (CONFIG_HFSPLUS_FS unset).
# Same source tag and config as the running kernel; vmlinux is built once for vmlinux.symvers
# (CONFIG_MODVERSIONS=y), and the module carries the running release string (the git tree would
# otherwise append "+"). About 30-50 minutes on 8 threads. Run as root.
#
#   build-wsl-hfsplus.sh [WORK]          -> WORK/bin/hfsplus.ko   (default WORK=/root/s10e)
#   insmod WORK/bin/hfsplus.ko           (lost at every WSL restart; load again)
set -euo pipefail
work=${1:-/root/s10e}
rel=$(uname -r)
[[ $rel == *-microsoft-standard-WSL2 ]] || { echo "not a WSL2 kernel: $rel" >&2; exit 1; }
tag=linux-msft-wsl-${rel%%-*}
export DEBIAN_FRONTEND=noninteractive
apt-get -o DPkg::Lock::Timeout=900 install -y -qq build-essential flex bison libssl-dev libelf-dev dwarves cpio bc >/dev/null
mkdir -p "$work/kernel" "$work/bin" && cd "$work/kernel"
[[ -d src ]] || git clone -q --depth 1 --branch "$tag" https://github.com/microsoft/WSL2-Linux-Kernel.git src
cd src
zcat /proc/config.gz > .config
scripts/config --module CONFIG_HFSPLUS_FS
make olddefconfig >/dev/null
[[ -f vmlinux.symvers ]] || make -j"$(nproc)" vmlinux > "$work/kernel/vmlinux-build.log" 2>&1
cp vmlinux.symvers Module.symvers
make -s KERNELRELEASE="$rel" modules_prepare
make -j"$(nproc)" KERNELRELEASE="$rel" M=fs/hfsplus modules
modinfo fs/hfsplus/hfsplus.ko | grep -q "vermagic: *$rel " || { echo "vermagic does not match $rel" >&2; exit 1; }
cp fs/hfsplus/hfsplus.ko "$work/bin/hfsplus.ko"
echo "built $work/bin/hfsplus.ko for $rel"
