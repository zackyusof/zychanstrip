#!/usr/bin/env bash
# Uses the current macOS plugin build for its NAM engine objects.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$here/build_current.py" permute
