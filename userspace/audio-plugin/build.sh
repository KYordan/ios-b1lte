#!/bin/bash
# B2L audio, lot 4: HakiOS's AudioServerPlugIn (MIT, ios-a5xelte gpu/hakios-audio) for iOS 26 on the S10.
# Output: ~/Developer/s10-port/build-audio/$V/{HakiosAudio, Info.plist}. SYSTEM gets /$V
# (GL_BUILD=~/Developer/s10-port/build-audio b1l-tools/system-add-fast.sh $V); the kernel publishes it
# to /System/Library/Audio/Plug-Ins/HAL/HakiosAudio.driver/ (xnu-overlay bsd/kern/b2l_audio_seed.h).
set -euo pipefail
V=${V:-b2l-audio-v1}
D=$(cd "$(dirname "$0")" && pwd)
O=${S10_PORT:-$HOME/Developer/s10-port}/build-audio/$V
SDK=$(xcrun --sdk iphoneos --show-sdk-path)
rm -rf "$O"; mkdir -p "$O"
xcrun --sdk iphoneos clang -target arm64e-apple-ios26.0 -isysroot "$SDK" -O1 -Wall -bundle -framework CoreFoundation \
	-I"$D" -o "$O/HakiosAudio" "$D/audio_plugin.c"
/usr/bin/codesign --force --sign - --identifier com.hakios.audio "$O/HakiosAudio"
cp "$D/audio_plugin.plist" "$O/Info.plist"
plutil -lint "$O/Info.plist"
shasum -a 256 "$O"/*
