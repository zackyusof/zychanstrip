#!/usr/bin/env bash
# Build the headless reorder-interaction harness.
# Works on any Linux/macOS host with a C++17 compiler — it does NOT need JUCE,
# Xcode or the plugin build. Output: build/permute
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
mkdir -p "$root/build"

compiler="${CXX:-c++}"
"$compiler" -std=c++17 -O2 -Wall -Wextra \
    -I "$here/shim" \
    -o "$root/build/permute" \
    "$here/permute.cpp"

echo "built $root/build/permute"
