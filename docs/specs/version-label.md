# SPEC — Version / channel label in the GUI header

| Field | Value |
| --- | --- |
| Slug | `version-label` |
| Repo | `/Users/projects/zychanstrip` (local working copy `~/projects/zychanstrip`) |
| Owner | zack |
| Created | 2026-11-03 |
| Classification | feature (GUI presentation + build metadata only) |
| Version authority | **user** — the agent never picks a version, tag or release |
| Lifecycle | `~/.config/opencode/skills/dev-delegation/references/lifecycle.md` |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | agent | §1 | REQ-1..REQ-4 with observable checks |
| 2 | Design | done | agent | §2 | DES-1..DES-3 cover every REQ |
| 3 | Build | done | agent | §3 | `cmake` configure prints the label; `ninja -C build` 0 errors |
| 4 | Test | done | agent | §4 | `RESULT 253/253`; header snapshots for stable **and** beta |
| 5 | Deploy | done | agent | §5 | `tools/install_global.sh` — global copy only, `codesign -v` PASS |
| 6 | Maintain | done | agent | §6 | README + CHANGELOG + this spec; memory entry |

## 1. Requirements

| ID | Requirement | Acceptance check (observable) | Priority |
| --- | --- | --- | --- |
| REQ-1 | The GUI header shows the build's version immediately after the `zychanstrip` wordmark, top-left | Snapshot shows `zychanstrip` then a `v…` pill, left of the ZY monogram | must |
| REQ-2 | A prerelease build is unmistakable: the label carries the prerelease suffix and a distinct (amber) treatment | `-DZYCHANSTRIP_BETA=beta.1` renders `v1.0.1-beta.1` in amber; stable renders `v1.0.1` in neutral grey | must |
| REQ-3 | The label can never disagree with the build: it comes from the CMake project version, not a hand-typed string | `cmake` prints `zychanstrip GUI version label: v<PROJECT_VERSION>[-<BETA>]`; `strings`-level source is the compile definition | must |
| REQ-4 | No regression: no header element overlaps another, harness stays green | `RESULT 253/253`; header snapshot contains no truncated/overlapping text | must |

## 2. Design

| ID | Decision | Covers |
| --- | --- | --- |
| DES-1 | CMake owns the label: `ZYCHANSTRIP_GUI_VERSION = v${PROJECT_VERSION}[-${ZYCHANSTRIP_BETA}]`, published as the `ZYCHANSTRIP_VERSION_LABEL` compile definition; `PROJECT_VERSION` stays numeric for the bundle plist | REQ-2, REQ-3 |
| DES-2 | `ZychanStripEditor::paint` draws a recessed pill sized from `GlyphArrangement` measurement of the actual string, positioned after the measured wordmark; the ZY monogram is placed clear of the pill instead of at a fixed offset | REQ-1 |
| DES-3 | Channel styling: `containsIgnoreCase("beta")` selects amber (`#ffc45c` ink / `#e8a33d` border) over neutral (`#d8dde3` / `engraveDim`) | REQ-2 |

The pre-existing `MASTERING & TRACKING STRIP` tagline used to start right after the monogram and ran
underneath the RACK/METER toggle (only `MAST` and `RIP` were legible). It is now bounded from the
toggle's right edge to just before the power jewel, with ellipsis as the fallback.

## 3. Build

- `CMakeLists.txt`: `ZYCHANSTRIP_BETA` cache string → `ZYCHANSTRIP_GUI_VERSION` → `target_compile_definitions`.
- `Source/PluginEditor.cpp`: `#ifndef ZYCHANSTRIP_VERSION_LABEL` fallback to `JucePlugin_VersionString`.
- Verified configure output: `-- zychanstrip GUI version label: v1.0.1` (stable) and `v1.0.1-beta.1` (beta).

## 4. Test

| ID | Test | Result |
| --- | --- | --- |
| TST-1 | Harness `RESULT n/n` after the change | **253/253** |
| TST-2 | Stable header render (1620×1140 snapshot crop) | `v1.0.1` pill, neutral, no overlap |
| TST-3 | Beta header render (`-DZYCHANSTRIP_BETA=beta.1`) | `v1.0.1-beta.1` pill, amber, no overlap |
| TST-4 | Tagline placement | `MASTERING & TRACKING STRIP` fully legible between toggle and jewel |

Note: the version literal cannot be proven with `strings`/`grep` on the Mach-O — ld64 pools and merges
string-literal fragments (`v1.0` + a shared `1` tail), so an exact byte search returns 0 for a string
the GUI demonstrably renders. The render is the evidence, not the byte scan.

## 5. Deploy

- `tools/install_global.sh` → `/Library/Audio/Plug-Ins/Components/zychanstrip.component` (global only).
- Verified: `root:wheel`, `codesign -v` PASS, one copy, plist `1.0.1`, no user-space copy.

## 6. Maintain

- Setting a beta for a build: `cmake -S . -B build -DZYCHANSTRIP_BETA=beta.1` (empty = stable).
- The label follows `PROJECT_VERSION`; bumping the version is the maintainer's decision.
