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

die() { printf 'ERROR: %s\n' "$1" >&2; exit "${2:-1}"; }

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

# ---- GUI version-label guard (2026-11-07, user directive) ----------------------------
# ZYCHANSTRIP_BETA is a CMake cache variable; an old build/CMakeCache.txt can leave the
# GUI labeled beta.N-1 even after CMakeLists.txt and the tag say beta.N. The installer
# now refuses a label/tag mismatch so a mislabeled AU can never be installed.
if git -C "$REPO" rev-parse --git-dir >/dev/null 2>&1 && [ -f "$REPO/build/CMakeCache.txt" ]; then
    CACHE_BETA="$(grep -E '^ZYCHANSTRIP_BETA:' "$REPO/build/CMakeCache.txt" | head -1 | sed 's/^[^=]*=//' | tr -d '[:space:]')"
    LATEST_TAG="$(git -C "$REPO" describe --tags --abbrev=0 --match '*beta*' 2>/dev/null || true)"
    if [ -n "$CACHE_BETA" ] && [ -n "$LATEST_TAG" ]; then
        TAG_BETA="${LATEST_TAG##*-beta.}"
        if [ "$CACHE_BETA" != "beta.$TAG_BETA" ]; then
            die "GUI version label mismatch: build cache ZYCHANSTRIP_BETA=$CACHE_BETA but latest tag is $LATEST_TAG (beta.$TAG_BETA). Reconfigure with -DZYCHANSTRIP_BETA=beta.$TAG_BETA (or delete build/CMakeCache.txt), rebuild, then install." 1
        fi
        printf 'version : GUI label matches tag %s (ZYCHANSTRIP_BETA=%s)\n' "$LATEST_TAG" "$CACHE_BETA" >&2
    fi
fi

# ---- "Latest always" guard (2026-11-03, user directive) -------------------------------
# An installed AU must be reproducible from a commit: build the newest code, not whatever
# happened to be in the tree when someone last ran ninja. Measured failure this prevents:
# the 2026-11-03 install carried an uncommitted limiter optimization, so no commit produced
# the binary that was running.
#   * tracked files modified (or untracked files under Source/ tools/) -> refuse
#   * local HEAD behind refs/remotes/origin/main                        -> refuse
#   * artefact older than the HEAD commit                              -> refuse
# Escape hatch for deliberate experiments: ZYCH_ALLOW_STALE=1 (prints a loud warning).
if git -C "$REPO" rev-parse --git-dir >/dev/null 2>&1; then
    GIT_HEAD_SHA="$(git -C "$REPO" rev-parse HEAD)"
    GIT_HEAD_SUBJECT="$(git -C "$REPO" log -1 --format=%s | cut -c1-72)"
    GIT_HEAD_TS="$(git -C "$REPO" log -1 --format=%ct)"

    dirty="$(git -C "$REPO" status --porcelain --untracked-files=normal -- Source tools CMakeLists.txt)"
    if [ -n "$dirty" ]; then
        if [ "${ZYCH_ALLOW_STALE:-0}" = "1" ]; then
            echo "WARNING: building from a DIRTY tree (ZYCH_ALLOW_STALE=1) — not reproducible:" >&2
            printf '%s\n' "$dirty" >&2
        else
            printf 'ERROR: tracked source is modified — a build from a dirty tree is not reproducible.\n%s\n' "$dirty" >&2
            die "commit (or stash) the source first, then rebuild. Override only for a deliberate experiment: ZYCH_ALLOW_STALE=1 $0" 1
        fi
    fi

    if origin_sha="$(git -C "$REPO" rev-parse --verify --quiet refs/remotes/origin/main)"; then
        if [ "$origin_sha" != "$GIT_HEAD_SHA" ]; then
            if git -C "$REPO" merge-base --is-ancestor "$GIT_HEAD_SHA" "$origin_sha" 2>/dev/null; then
                printf 'ERROR: local HEAD %s is BEHIND origin/main %s — fetch/pull, rebuild, then install.\n' \
                    "$(git -C "$REPO" rev-parse --short HEAD)" "$(git -C "$REPO" rev-parse --short "$origin_sha")" >&2
                die "refusing to install a stale build (overridable with ZYCH_ALLOW_STALE=1)" 1
            fi
            echo "WARNING: HEAD is ahead of origin/main — this build is not on the remote yet." >&2
        fi
    fi

    SRC_TS="$(stat -f %m "$SRC/Contents/MacOS/zychanstrip")"
    if [ "$SRC_TS" -lt "$GIT_HEAD_TS" ]; then
        if [ "${ZYCH_ALLOW_STALE:-0}" = "1" ]; then
            echo "WARNING: artefact predates HEAD (ZYCH_ALLOW_STALE=1)." >&2
        else
            die "artefact is older than HEAD ($(git -C "$REPO" rev-parse --short HEAD)) — run 'ninja -C build' and retry." 1
        fi
    fi

    printf 'git    : %s %s\n' "$(git -C "$REPO" rev-parse --short HEAD)" "$GIT_HEAD_SUBJECT"
else
    echo "WARNING: not a git checkout — cannot verify the build matches the latest commit." >&2
fi

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
