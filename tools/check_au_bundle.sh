#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
probe_dir="$(mktemp -d /tmp/zychanstrip-au-probe.XXXXXX)"
trap 'rm -rf "$probe_dir"' EXIT
clang++ -std=c++17 "$here/au_probe.cpp" -framework AudioToolbox -framework CoreFoundation -o "$probe_dir/probe"
"$probe_dir/probe" "${1:-/Library/Audio/Plug-Ins/Components/zychanstrip.component/Contents/MacOS/zychanstrip}"
