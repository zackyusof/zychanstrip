#!/usr/bin/env bash
# Build the headless DSP benchmark + null harness.
# Like build.sh this needs only a C++17 compiler — no JUCE, Xcode or plugin build.
# Output: build/bench (and build/bench_equiv.cpp via --emit-equiv)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
mkdir -p "$root/build"

compiler="${CXX:-c++}"
"$compiler" -std=c++17 -O3 -DNDEBUG -Wall -Wextra \
    -I "$here/shim" \
    -o "$root/build/bench" \
    "$here/bench.cpp"

echo "built $root/build/bench"
