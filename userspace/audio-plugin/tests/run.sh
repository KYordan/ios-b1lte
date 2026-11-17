#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
H=$(cd "$(dirname "$0")/../../.." && pwd)/xnu-overlay/iokit/Kernel/arm
clang++ -std=c++17 -Wall -Wextra -Wno-unused-function -g -fsanitize=address,undefined -I"$H" test4.cpp -o /tmp/b2l-audio-test4 && /tmp/b2l-audio-test4 > out4.txt 2>&1 && tail -1 out4.txt
