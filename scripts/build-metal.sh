#!/bin/bash
# B1L-0044: the Metal plug-in bundle (userspace/metal/B2LMetal.m, HakiOS's, MIT) and the M1 test library for backboardd.
# Output: ~/Developer/s10-port/build-metal/$V/{B2LMetalV2, Info.plist, libb2l-metal-test.dylib}; SYSTEM gets /$V
# (b1l-tools/system-add-gl.sh $V) and the kernel publishes it (b2l_userspace_seed.h).
set -euo pipefail
V=${V:-b2l-metal-v1}
BUNDLE=${BUNDLE:-B2LMetalV2}
R=$(cd "$(dirname "$0")/.." && pwd)
O=~/Developer/s10-port/build-metal/$V
SDK=$(xcrun --sdk iphoneos --show-sdk-path)
CC="xcrun --sdk iphoneos clang -target arm64e-apple-ios26.0 -isysroot $SDK -O2 -Wall -Wno-deprecated-declarations"
rm -rf "$O"; mkdir -p "$O"
# B1L-0048: the plug-in with the GPU (HAKIOS_METAL_GPU), over the Metal core in our libOSMesa (b2l_metal_core.c)
M=${MESA_OUT:-$HOME/Developer/s10-port/build-mesa/out}
cp "$M/libOSMesa.8.dylib" "$O/libOSMesa.8.dylib"
/usr/bin/install_name_tool -id /usr/local/lib/$V/libOSMesa.8.dylib "$O/libOSMesa.8.dylib" 2>/dev/null || true
/usr/bin/codesign --force --sign - "$O/libOSMesa.8.dylib"
mkdir -p "$O.inc/hakios-metal"; cp "$R/userspace/metal/hakios_metal.h" "$O.inc/hakios-metal/"
$CC -bundle -DHAKIOS_METAL_GPU -I"$O.inc" -framework Foundation -framework Metal -lobjc "$O/libOSMesa.8.dylib" \
	-o "$O/$BUNDLE" "$R/userspace/metal/B2LMetal.m" "$R/userspace/metal/hakios_metallib.c" -I"$R/userspace/metal"
$CC -dynamiclib -install_name /usr/local/lib/$V/libb2l-metal-test.dylib -framework Foundation -framework Metal -lobjc \
	-o "$O/libb2l-metal-test.dylib" "$R/userspace/metal/b2l_metal_test.m"
/usr/bin/codesign --force --sign - --identifier dev.b2l.metal "$O/$BUNDLE"
/usr/bin/codesign --force --sign - --identifier dev.b2l.metal-test "$O/libb2l-metal-test.dylib"
rm -rf "$O/_CodeSignature"
sed "s/B2LMetalV[0-9]*/$BUNDLE/g" "$R/userspace/metal/Info.plist" > "$O/Info.plist"
rm -rf "$O.inc"
shasum -a 256 "$O"/*
