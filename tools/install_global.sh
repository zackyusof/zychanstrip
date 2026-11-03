#!/usr/bin/env bash
#
# install_global.sh — the ONLY sanctioned way to install the zychanstrip Audio Unit.
#
# Install policy (2026-11-03, user directive): GLOBAL ONLY.
#   * Destination is always /Library/Audio/Plug-Ins/Components/zychanstrip.component.
#   * ~/Library/Audio/Plug-Ins is never written; an existing user-domain copy is removed
#     after the global install succeeds (it would otherwise shadow a newer global build
#     and confuse the AudioComponentRegistrar).
#   * Builds themselves install nothing (COPY_PLUGIN_AFTER_BUILD FALSE in CMakeLists.txt).
#
# Usage:
#   tools/install_global.sh                      # install build/…/Release/AU bundle
#   tools/install_global.sh <path/to/bundle>     # install a specific bundle
#   ninja -C build install-global                # same, via the CMake target
#
# Exit codes: 0 ok · 1 precondition failed · 2 install/verification failed
set -euo pipefail

DEST_DIR="/Library/Audio/Plug-Ins/Components"
DEST="$DEST_DIR/zychanstrip.component"
USER_DIR="$HOME/Library/Audio/Plug-Ins/Components"
USER_COPY="$USER_DIR/zychanstrip.component"

die() { printf 'ERROR: %s\n' "$*" >&2; exit "${2:-1}"; }

[ "$(uname -s)" = "Darwin" ] || die "macOS only (this installs an Audio Unit)."

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${1:-}"
if [ -z "$SRC" ]; then
    for cand in \
        "$REPO/build/zychanstrip_artefacts/Release/AU/zychanstrip.component" \
        "$REPO/build/zychanstrip_artefacts/Debug/AU/zychanstrip.component"
    do
        [ -d "$cand" ] && { SRC="$cand"; break; }
    done
fi
[ -n "$SRC" ] || die "no built AU found — run 'ninja -C build' first (looked in build/zychanstrip_artefacts/*/AU/)."
SRC="$(cd "$(dirname "$SRC")" && pwd)/$(basename "$SRC")"
[ -d "$SRC" ] || die "source bundle not found: $SRC"
[ -x "$SRC/Contents/MacOS/zychanstrip" ] || die "not an AU bundle (no Contents/MacOS/zychanstrip): $SRC"

# Refuse to source from, or target, user space — the whole point of this script.
case "$SRC" in
    "$USER_DIR"/*) die "refusing to install FROM user space ($SRC). Build produces the artefact under build/." ;;
esac

SRC_MD5="$(md5 -q "$SRC/Contents/MacOS/zychanstrip")"
printf 'source : %s\nmd5    : %s\n' "$SRC" "$SRC_MD5"

echo "==> installing to $DEST (sudo password required)"
sudo mkdir -p "$DEST_DIR"
sudo rm -rf "$DEST"
sudo /usr/bin/ditto "$SRC" "$DEST"
sudo chown -R root:wheel "$DEST"
sudo chmod -R go-w "$DEST"

# Verify the bytes actually landed (before signing — codesign rewrites the Mach-O).
[ -d "$DEST" ] || die "install did not create $DEST" 2
DEST_MD5="$(md5 -q "$DEST/Contents/MacOS/zychanstrip")"
[ "$DEST_MD5" = "$SRC_MD5" ] || die "md5 mismatch after install: built=$SRC_MD5 installed=$DEST_MD5" 2
printf 'copied md5    : %s (matches source)\n' "$DEST_MD5"

# JUCE's build leaves a linker-signed, ad-hoc Mach-O whose Contents/_CodeSignature is
# stale, so `codesign -v` on the BUNDLE fails with "code has no resources but signature
# indicates they must be present". Re-seal the installed bundle ad-hoc; README requires
# codesign -v to pass before the build counts as installed.
echo "==> ad-hoc sealing the installed bundle"
sudo codesign --force --sign - "$DEST"
sudo chown -R root:wheel "$DEST"
sudo chmod -R go-w "$DEST"
if codesign -v "$DEST" 2>/dev/null; then
    echo "codesign      : valid (adhoc, sealed)"
else
    die "codesign -v still fails on $DEST — do not ship this install" 2
fi
printf 'installed md5 : %s\n' "$(md5 -q "$DEST/Contents/MacOS/zychanstrip")"
printf 'ownership     : %s\n' "$(stat -f '%Su:%Sg' "$DEST")"

# Now remove the user-space copy, if any, so exactly one bundle is registered.
if [ -e "$USER_COPY" ]; then
    echo "==> removing user-space copy $USER_COPY"
    rm -rf "$USER_COPY"
fi
[ -e "$USER_COPY" ] && die "user-space copy still present: $USER_COPY" 2

# Enforce the invariant: one copy, global only.
copies=0
[ -d "$DEST" ] && copies=$((copies + 1))
[ -d "$USER_COPY" ] && copies=$((copies + 1))
[ "$copies" -eq 1 ] || die "expected exactly 1 installed copy, found $copies" 2

cat <<EOF

OK — global install only.
  installed : $DEST
  user copy : none
  md5       : $(md5 -q "$DEST/Contents/MacOS/zychanstrip")
  detail    : copied-from md5 $DEST_MD5, re-sealed ad-hoc
Next: restart the host/DAW (or killall -9 AudioComponentRegistrar) so the registrar rescans.
AU validation (auval / pluginval / DAW load) remains the maintainer's job.
EOF
