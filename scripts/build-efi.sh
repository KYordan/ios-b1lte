#!/usr/bin/env bash
# Build standalone applications; the firmware platform is an external dependency.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${WORKSPACE:?Set WORKSPACE to the prepared Mu/EDK II workspace}"
export WORKSPACE
export EDK_TOOLS_PATH="${EDK_TOOLS_PATH:-$WORKSPACE/Mu_Basecore/BaseTools}"
export EDK_TOOLS_BIN="${EDK_TOOLS_BIN:-$EDK_TOOLS_PATH/Source/C/bin}"
export PYTHON_COMMAND="${PYTHON_COMMAND:-python3}"
: "${CLANG_BIN:?Set CLANG_BIN to a directory containing clang, llvm-lib, llvm-rc and lld-link}"
export CLANG_BIN="${CLANG_BIN%/}/"
export PACKAGES_PATH="$WORKSPACE/Mu_Basecore${PACKAGES_PATH:+:$PACKAGES_PATH}"
export PATH="$EDK_TOOLS_PATH/BinWrappers/PosixLike:$EDK_TOOLS_BIN:$CLANG_BIN:$PATH"
for tool in clang llvm-lib llvm-rc lld-link; do
  [[ -x "$CLANG_BIN$tool" ]] || { echo "Missing tool: $CLANG_BIN$tool" >&2; exit 1; }
done
[[ -x "$EDK_TOOLS_BIN/GenFw" ]] || { echo 'Build native BaseTools first.' >&2; exit 1; }
mkdir -p "$WORKSPACE/B2LXnuPkg"
rsync -a "$repo_root/uefi/" "$WORKSPACE/B2LXnuPkg/"
rsync -a "$repo_root/common/cache/" "$WORKSPACE/B2LXnuPkg/XnuLoader/"
rsync -a "$repo_root/common/smp/" "$WORKSPACE/B2LXnuPkg/XnuLoader/"
rsync -a "$repo_root/common/memory/" "$WORKSPACE/B2LXnuPkg/XnuLoader/"
# B2L_BOARD (scripts/b2l_board.py) supplies the board's RAM ceiling and its
# default display profile; B2L_FB_LOGICAL / B2L_DISPLAY_SCALE set by hand win.
display_options=()
if [[ -n "${B2L_BOARD:-}" ]]; then
  board_profile="$("$PYTHON_COMMAND" "$repo_root/scripts/b2l_board.py" shell)" || exit 1
  board_value() { sed -n "s/^$1=//p" <<<"$board_profile"; }
  display_options+=(-D "B2L_RAM_ALLOW_END=$(board_value B2L_RAM_ALLOW_END)")
  : "${B2L_FB_LOGICAL:=$(board_value B2L_FB_LOGICAL)}"
  : "${B2L_DISPLAY_SCALE:=$(board_value B2L_DISPLAY_SCALE)}"
  echo "board $B2L_BOARD: RAM end $(board_value B2L_RAM_ALLOW_END), display ${B2L_FB_LOGICAL:-1:1} @${B2L_DISPLAY_SCALE}x" >&2
fi
# Optional board display profile for the loader, e.g. B2L_FB_LOGICAL=880x1912 B2L_DISPLAY_SCALE=2.
if [[ -n "${B2L_FB_LOGICAL:-}" ]]; then
  [[ "$B2L_FB_LOGICAL" =~ ^([0-9]+)x([0-9]+)$ ]] || { echo "B2L_FB_LOGICAL must be WxH" >&2; exit 1; }
  display_options+=(-D "B2L_FB_LOGICAL_W=${BASH_REMATCH[1]}" -D "B2L_FB_LOGICAL_H=${BASH_REMATCH[2]}")
fi
if [[ -n "${B2L_DISPLAY_SCALE:-}" ]]; then display_options+=(-D "B2L_DISPLAY_SCALE=$B2L_DISPLAY_SCALE"); fi
cd "$WORKSPACE"
exec "$EDK_TOOLS_PATH/BinWrappers/PosixLike/build" \
  -a AARCH64 -t CLANGPDB -b RELEASE -n "${BUILD_JOBS:-4}" \
  -p "${DSC_PATH:-B2LXnuPkg/B2LXnuPkg.dsc}" ${display_options[@]+"${display_options[@]}"} "$@"
