#!/usr/bin/env bash
# Test original compatibility sources; all generated outputs stay temporary.
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
xnu="${1:?Pass the prepared pinned XNU source directory}"
source_dir="$repo/xnu-overlay/osfmk/arm64"
work="$(mktemp -d /private/tmp/b2l-arm64e-tests.XXXXXX)"
trap 'rm -rf "$work"' EXIT INT TERM
cc="${CLANG:-$(xcrun --find clang)}"
llvm_bin="${LLVM_BIN:-$HOME/Developer/tools/mamba/envs/s10/bin}"
skipped=()
mkdir "$work/core"
cp "$repo/tests/arm64e/core/"* "$work/core/"
cp "$source_dir/b2l_compat.c" "$source_dir/b2l_compat.h" "$work/core/"
cp "$source_dir/b2l_compat_fast.s" "$work/core/b2l_compat_fast.S"
LLVM_MC="$llvm_bin/llvm-mc" python3 "$work/core/generate-fixtures.py"
flags=(-isysroot "$(xcrun --sdk macosx --show-sdk-path)" -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer)
"$cc" "${flags[@]}" "$work/core/b2l_compat.c" "$work/core/test_compat.c" -o "$work/core/reference"
"$work/core/reference"
if [[ "$(uname -m)" == arm64 ]]; then
  "$cc" "${flags[@]}" -DB2L_COMPAT_USE_ASM "$work/core/b2l_compat.c" "$work/core/b2l_compat_fast.S" "$work/core/test_compat.c" -o "$work/core/assembly"
  "$work/core/assembly"
else
  skipped+=("native assembly differential test")
  echo 'SKIPPED: native assembly differential test needs an arm64 host'
fi
python3 "$repo/tests/test-pacga-runtime.py" "$xnu"
python3 "$repo/tests/test_arm64e_grade.py" "$xnu"
python3 "$repo/tests/arm64e/pager/run.py" "$xnu" --negative-shadow-regression
if (( ${#skipped[@]} == 0 )); then
  echo 'ARM64e core, native assembly, wrapper, grade and mixed-page gates passed'
else
  echo "ARM64e core, wrapper, grade and mixed-page gates passed; SKIPPED: $(IFS=', '; echo "${skipped[*]}")"
fi
