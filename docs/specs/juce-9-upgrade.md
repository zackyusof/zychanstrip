# SPEC — Upgrade to the latest JUCE (9.0.4) and rebuild the AU

| Field | Value |
| --- | --- |
| Slug | `juce-9-upgrade` |
| Repo | `/Users/zack/projects/rackstrip` |
| Track | juce-9-upgrade |
| Owner | zack |
| Created | 2026-11-01 |
| Classification | dependency upgrade (spike → patch) |
| Version authority | **user** (the agent never invents a version, tag or release) |
| Lifecycle | `~/.config/opencode/skills/dev-delegation/references/lifecycle.md` |
| Inner cycle | `~/.config/opencode/skills/dev-delegation/references/dev-cycle.md` |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | orchestrator | §1 | user request + JUCE 9.0.4 tag list |
| 2 | Design | done | orchestrator | §2 | 1 breaking API identified from `BREAKING_CHANGES.md` |
| 3 | Build | done | inline (single module) | §3 | 0 errors, 0 warnings; AU linked |
| 4 | Test | done | inline | §4 | `RESULT 230/230 passed`; factory load test ok |
| 5 | Deploy | done | orchestrator | §5 | md5 `38c7361a…` in `/Library`, one copy only |
| 6 | Maintain | done | orchestrator | §6 | README policy + closed issue + memory |

## 1. Requirements

### Problem
The plugin is pinned to JUCE 8.0.4 (`FetchContent`, `CMakeLists.txt`). The user asked whether the
latest JUCE can be used and, if so, to build an AU and install it.

### Users and outcomes
The user wants the AU running on the latest framework, installed once, on this Mac.

### Scope
- Move the JUCE pin to the latest release (9.0.4).
- Make the source compile against it with the smallest possible change.
- Build the AU, install it system-wide, remove the stale duplicate.
- Keep the DSP/GUI behaviour identical (the repo harness is the gate).

### Non-goals
- No version-number change (stays 1.0.0 — the user owns versions/tags).
- No DSP, parameter, GUI or behaviour work.
- No `auval` pass: blocked by the host, see §4.

### Requirements (observable)

| ID | Requirement | Acceptance check (observable) | Priority |
| --- | --- | --- | --- |
| REQ-1 | Build against the latest JUCE | `cmake --build` exits 0 with `GIT_TAG 9.0.4`; `JUCE_MAJOR_VERSION 9` | must |
| REQ-2 | No behaviour change | `tools/build_snapshot.py` harness prints `RESULT 230/230 passed` | must |
| REQ-3 | No new warnings | forced recompile of `Source/*.cpp` emits 0 warnings | must |
| REQ-4 | AU is installable and loads | factory `Open`/`Close` return 0 via direct dlopen harness | must |
| REQ-5 | Exactly one installed copy, system-wide | only `/Library/Audio/Plug-Ins/Components/zychanstrip.component` exists | must |
| REQ-6 | Docs match reality | README/AGENTS name JUCE 9; install policy documented | should |

### Constraints
- Host has **no CMake, no Ninja, no Xcode** (CLT only) — both tools are downloaded to task scratch and
  removed afterwards.
- `sudo` is denied by session policy; system-wide writes need the native auth prompt (`osascript … with
  administrator privileges`).

### Risks
- JUCE 9 is a **major** release: `Drawable` left `juce_gui_basics`, `Drawable` no longer derives from
  `Component`, the SVG parser was replaced by lunasvg.
- JUCE 9 carries a new EULA (`juce-9-licence`); the plugin is private/personal, and the modules remain
  dual-licensed AGPLv3-or-commercial. No distribution is implied by this upgrade.

### Open questions

| ID | Question | Owner | Blocks design? |
| --- | --- | --- | --- |
| Q-1 | Does the user want the version bumped? | user | no — left at 1.0.0 |

## 2. Design

### Module map (exactly one owner per file; 0 unclaimed)

| Module | Owned globs | Owner | Notes |
| --- | --- | --- | --- |
| build+deps | `CMakeLists.txt` | orchestrator | JUCE pin only |
| source-compat | `Source/PluginEditor.cpp` | orchestrator | the single call site |
| docs | `README.md`, `AGENTS.md`, `docs/specs/juce-9-upgrade.md` | orchestrator | |

One module, two code lines → the fleet is not warranted (stated inline, per `dev-delegation` §5).

### Interfaces and seams
Only one JUCE API is removed that this plugin touches:

| ID | Decision | Rationale |
| --- | --- | --- |
| DES-1 | Pin `GIT_TAG 9.0.4` | latest release; the only blocker was one API call |
| DES-2 | Replace the SVG call with `Drawable::createFromSVGString (kZyMarkSvg)` | removes the `XmlDocument` round-trip; returns null on bad markup, identical to the old guard |
| DES-3 | Install system-wide, delete the user copy | one registered subtype; closes `rackstrip-dupe-au` |
| DES-4 | Leave the version at 1.0.0 | version/tag/release is the user's decision |

| JUCE 8 | JUCE 9 |
| --- | --- |
| `Drawable::createFromSVG (const XmlElement&)` | `Drawable::createFromSVGString (const String&)` → `unique_ptr<DrawableComposite>` |

`createFromSVGString` returns `{}` when the document does not parse (`juce_SVGParser.cpp:679-694`), which
preserves the old `if (auto xml = XmlDocument::parse(...))` guard exactly.

Checked and confirmed **unaffected**: `Font` still exists (`juce_Font.h:49`), `GlyphArrangement::addLineOfText
(const Font&, …)` unchanged (`juce_GlyphArrangement.h:172`), C++17 is still the module baseline
(`JUCEModuleSupport.cmake:340`), and `juce_add_plugin` keeps `AU_MAIN_TYPE` / `VST3_CATEGORIES` /
`COPY_PLUGIN_AFTER_BUILD`.

### Invariants (must not break)

| ID | Invariant | Where (`path:line`) |
| --- | --- | --- |
| INV-1 | Parameter IDs/ranges/defaults unchanged | `Source/PluginProcessor.cpp` `createLayout()` — untouched |
| INV-2 | Rack order packing + permutation check unchanged | `Source/PluginProcessor.cpp` — untouched |
| INV-3 | Latency still reported once from `prepareToPlay()` | `Source/PluginProcessor.cpp` — untouched |
| INV-4 | ZY logo still renders (SVG) | `Source/PluginEditor.cpp:20` markup, `drawZyLogo()` |

### Alternatives considered
- **Stay on 8.x** (`8.0.16-security` exists): rejected — the user asked for the latest, and the only
  blocker was a single API call.
- **Version-guard the SVG call** (`#if JUCE_MAJOR_VERSION >= 9`): rejected as unnecessary complexity —
  the pin moves forward, so there is nothing to keep compatible.

### Verification strategy
REQ-1 build exit status · REQ-2 repo harness · REQ-3 warning scan · REQ-4 dlopen factory harness ·
REQ-5 filesystem check · REQ-6 doc grep.

### Rollback
`git revert` the two-line patch (pin + call site) and rebuild; the previous AU binary is reproducible from
tag `v1.0`.

## 3. Build

| Item | Value |
| --- | --- |
| JUCE | `9.0.4` (commit-pinned by tag, shallow clone) |
| CMake / Ninja | 4.4.5 / 1.13.2, downloaded to task scratch (host has neither) |
| Compiler | AppleClang 21.0.0, arm64, C++17, deployment target 10.13 |
| Configure | `cmake -S . -B build-juce9 -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE=<juce-src>` |
| Build | `cmake --build build-juce9 --target zychanstrip_AU -j 10` |

Deviations: the JUCE source came from a local shallow clone via `FETCHCONTENT_SOURCE_DIR_JUCE` (avoids a
second network fetch); the committed pin is the normal `FetchContent` tag, which is what a clean clone uses.

## 4. Test

| REQ | TST | Test | Result |
| --- | --- | --- | --- |
| REQ-1 | TST-1 | configure + build with 9.0.4 | PASS — `[54/54]`, exit 0, `JUCE_MAJOR_VERSION 9 / MINOR 0 / BUILD 4` |
| REQ-2 | TST-2 | `tools/build_snapshot.py --build-dir build-juce9` then run snapshot | PASS — `RESULT 230/230 passed` |
| REQ-3 | TST-3 | `touch Source/*.cpp` + rebuild, grep `warning:` | PASS — 0 warnings |
| REQ-4 | TST-4 | dlopen bundle + `zychanstripAUFactory` + `Open`/`Close` | PASS — `Open (construct) status = 0`, `Close status = 0`, `RESULT ok` |
| REQ-5 | TST-5 | `ls` both Components dirs | PASS — only `/Library` exists |
| REQ-6 | TST-6 | grep docs | PASS — README badge/requirements + AGENTS.md say JUCE 9 |

Known gap — **`auval` cannot validate this host**: `auval -a` lists 30 Apple AUs and **0 of the several
hundred third-party AUs** installed in `/Library/Audio/Plug-Ins/Components`, i.e. third-party AU
registration is broken host-wide (pre-existing; `tools/build_snapshot.py` already documents "without a
working AudioComponentRegistrar"). Cache reset and registrar restart (`launchctl kickstart` is SIP-blocked)
did not change it. Verification therefore uses the direct factory load test above, which exercises the
same construction path `auval` would.

## 5. Deploy

| Item | Value |
| --- | --- |
| DEP-1 | Installed system-wide at `/Library/Audio/Plug-Ins/Components/zychanstrip.component`; user copy removed |
| Artifact | `/Library/Audio/Plug-Ins/Components/zychanstrip.component` |
| md5 | `38c7361ab7704751a45b9a5450a8404e` (previous JUCE 8 build: `03e48d6a050db467b349944550ececa9`) |
| Owner / mode | `root:wheel`, `go-w` |
| Signature | ad-hoc, `codesign -v` valid |
| Arch | arm64 |
| Version | `1.0.0` (unchanged — user's decision) |
| Duplicates | user-level copy removed; exactly one install |
| Backup of replaced build | task scratch (removed at cleanup; reproducible from tag `v1.0`) |
| Rollback | rebuild tag `v1.0`, or revert the patch and rebuild |

## 6. Maintain

- **MNT-1**: regression gate stays `tools/build_snapshot.py` → `RESULT n/n`; defects enter the README
  "Known issues" table with an ID; next review on the next framework bump.
- **Monitoring**: no runtime telemetry; the repo harness (`RESULT n/n`) stays the regression gate.
- **Support intake**: README "Known issues" table; new defects get an ID there.
- **Docs**: README badge + requirements + install policy updated; AGENTS.md updated; this spec.
- **Host caveat recorded**: third-party AU registration is broken on this Mac — if a DAW cannot see the
  plugin, that is the first thing to check, not the plugin.
- **Open follow-ups**: `rackstrip-mix-phase`, `rackstrip-autogain-order`, `rackstrip-rta-averaging`,
  `rackstrip-vu-ppm`, `rackstrip-dsp-docs`, `rackstrip-verify-quit-crash` (unchanged, see task ledger).
- **Review date**: next framework bump.
