#!/bin/bash
# b1l-build.sh NNNN SET [--mesa] [--no-libs] [--no-kernel] -- the whole chain of one Metal build, each stage written to a status file
# that watch-build.sh shows live:
#   mesa     scripts/build-mesa.sh (only with --mesa)
#   libs     scripts/build-metal-scion.sh into build-metal/SET (skipped with --no-libs)
#   kernel   scripts/build-kernel.sh (the overlay's headers copied into xnu first)
#   package  scripts/package-boot.py -> out/b1l-metal-NNNN/boot.img
#   system   system-add-fast.sh SET (SYSTEM: only the changed 4 MiB blocks, every run read back)
#   boot     flash-boot.sh (BOOT, with its guards and read-back)
# Status: ~/Developer/s10-port/progress/NNNN.status, one line a stage: name|state|start|end|detail
# (state: pending, running, done, failed); progress/current points at the newest one.
#   example: bash b1l-build.sh 0082 b2l-metal-v38 --mesa
set -uo pipefail
N=$1; SET=$2; shift 2
MESA=0; LIBS=1; KERNEL=1
for a in "$@"; do case $a in --mesa) MESA=1 ;; --no-libs) LIBS=0 ;; --no-kernel) KERNEL=0 ;; esac; done
S=${S10_PORT:-$HOME/Developer/s10-port}
R=${B2L_ROOT:-$S/beyond2lte-ios}
P=$S/progress
mkdir -p "$P"
ST=$P/$N.status
ln -sfn "$ST" "$P/current"
stages="mesa libs kernel package system boot"
: > "$ST"
for s in $stages; do
	state=pending
	[ $s = mesa ] && [ $MESA = 0 ] && state=skipped
	[ $s = libs ] && [ $LIBS = 0 ] && state=skipped
	[ $s = kernel ] && [ $KERNEL = 0 ] && state=skipped
	echo "$s|$state|0|0|" >> "$ST"
done
echo "build|$N|$SET|$(date +%s)" > "$P/$N.meta"

set_stage() {   # name state detail
	local now=$(date +%s) tmp=$ST.tmp
	awk -F'|' -v OFS='|' -v n="$1" -v st="$2" -v now="$now" -v d="$3" '
		$1 == n { if (st == "running") $3 = now; else $4 = now; $2 = st; $5 = d } { print }' "$ST" > "$tmp" && mv "$tmp" "$ST"
}
run_stage() {   # name log command...
	local name=$1 log=$2; shift 2
	set_stage "$name" running "$log"
	if "$@" > "$log" 2>&1; then set_stage "$name" done "$log"; return 0; fi
	set_stage "$name" failed "$log"
	echo "B1L-$N: stage $name failed, see $log"
	exit 1
}

cd "$R"
if [ $MESA = 1 ]; then run_stage mesa "$R/out/build-mesa-$N.log" bash scripts/build-mesa.sh; fi
if [ $LIBS = 1 ]; then run_stage libs "$R/out/build-$N-userspace.log" env V=$SET bash scripts/build-metal-scion.sh; fi
cp xnu-overlay/bsd/kern/b2l_userspace_seed.h xnu-overlay/bsd/kern/b2l_exec_graphics_diag.h xnu/bsd/kern/
# B1L-0207: the kernel never boots earlier than its build time (B2LPlatformExpert::getGMTTimeOfDay)
echo "#define B2L_BUILD_EPOCH $(date +%s)" > xnu/iokit/Kernel/arm/b2l_build_epoch.h
touch xnu/iokit/Kernel/arm/B2LPlatformExpert.cpp
[ $KERNEL = 1 ] && run_stage kernel "$R/out/build-$N.log" env KDKROOT=/Library/Developer/KDKs/KDK_26.5_25F71.kdk B2L_DEPSROOT=$S/deps \
	BUILD_JOBS=4 BUILD_MEMORY_BYTES=12884901888 BUILD_DIR=$R/out/xnu-b2l-ref \
	bash scripts/build-kernel.sh CWARNFLAGS=-w CXXWARNFLAGS=-w
K=$R/out/xnu-b2l-ref/obj/DEVELOPMENT_ARM64_B2L/mach.development.b2l
W=$S/mu-work-b1l-touch; E=${PY_ENV:-$HOME/Developer/tools/mamba/envs/s10}/bin
if [ "$(strings -a "$K" | grep -c "$SET")" = 0 ]; then   # not grep -q: with pipefail its early exit fails the pipe
	set_stage package failed "the kernel does not name $SET"; echo "B1L-$N: the kernel does not name $SET"; exit 1
fi
run_stage package "$R/out/package-$N.log" python3 scripts/package-boot.py --mu $W \
	--firmware $W/Build/beyond1ltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd --kernel "$K" \
	--loader $W/Build/B2LXnu/RELEASE_CLANGPDB/AARCH64/XnuLoader.efi --output-dir out/b1l-metal-$N \
	--max-image-bytes 57671680 --boot-partition-bytes 57671680 --clang $E/clang --objcopy $E/llvm-objcopy
H=$(shasum -a 256 out/b1l-metal-$N/boot.img | cut -c1-64)
echo "boot image $H"
run_stage system "$S/flash-$N-system.log" env GL_BUILD=$S/build-metal bash $S/b1l-tools/system-add-fast.sh "$SET"
grep -E 'blocks changed|differing|SYSTEM-OK' "$S/flash-$N-system.log"
run_stage boot "$S/flash-$N-boot.log" bash $S/b1l-tools/flash-boot.sh "$R/out/b1l-metal-$N/boot.img" "$H"
tail -2 "$S/flash-$N-boot.log"
grep -q BOOT-OK "$S/flash-$N-boot.log" || { set_stage boot failed "no BOOT-OK"; exit 1; }
echo "B1L-$N: all stages done"
