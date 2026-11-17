#!/bin/bash
# B1L-0084: air2nir for THIS Mac -- Scion's AIR front end from the command line (src/compiler/air/air2nir.c), built
# natively from the same Mesa tree scripts/build-mesa.sh patches for the phone (so with every air-b2l change), to check
# shaders without a boot:
#   air2nir --report FILE.metallib      one line per function: translated, or the reason it is refused
#   air2nir FILE.metallib FUNCTION      the NIR of one function
# Run scripts/build-mesa.sh first (it lays out and patches src/compiler/air). Output: $B/build-host/src/compiler/air/air2nir
set -euo pipefail
B=${MESA_BUILD:-$HOME/Developer/s10-port/build-mesa}
export PATH=$B/venv/bin:$HOME/Developer/tools/mamba/envs/s10/bin:$PATH
cd "$B"
# the Mac's own compilers: the mamba environment on PATH has a clang whose headers (endian.h) the SDK lacks
export CC=$(xcrun --sdk macosx -f clang) CXX=$(xcrun --sdk macosx -f clang++) SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
[ -d mesa-25.0.7/src/compiler/air ] || { echo "run scripts/build-mesa.sh first" >&2; exit 1; }
if [ ! -f build-host/build.ninja ]; then
	meson setup build-host mesa-25.0.7 --buildtype=debugoptimized -Db_ndebug=false \
		-Dplatforms= -Dgallium-drivers= -Dvulkan-drivers= -Dglx=disabled -Degl=disabled -Dgbm=disabled \
		-Dopengl=false -Dgles1=disabled -Dgles2=disabled -Dosmesa=false -Dllvm=disabled -Dshared-glapi=disabled \
		-Dzstd=disabled -Dxmlconfig=disabled -Dexpat=disabled -Dshader-cache=disabled -Dvalgrind=disabled \
		-Dlibunwind=disabled -Dglvnd=disabled -Dvideo-codecs= -Dtools= -Dgallium-vdpau=disabled \
		-Dgallium-va=disabled -Dgallium-xa=disabled -Dlmsensors=disabled -Dbuild-tests=false > meson-host.log
fi
ninja -C build-host src/compiler/air/air2nir > ninja-host.log
echo "build-air-host.sh: $B/build-host/src/compiler/air/air2nir"
