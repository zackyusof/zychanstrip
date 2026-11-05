# SPEC — UNITY level matching, hardware reset defaults and reference knobs

> Beta.7 supersedes the original RESET gain choices: Opto Gain 0 and FET input/output 0 dB provide explicit software unity manual gain. The old Gain 50 claim was unsupported. See [UA operating reference](ua-operating-reference.md); the phase records below are historical.

| Field | Value |
| --- | --- |
| Slug | `unity-real-defaults-knobs` |
| Repo | `/Users/zack/projects/zychanstrip` |
| Track | unity-real-defaults-knobs |
| Owner | zack |
| Created | 2026-11-03 |
| Classification | feature |
| Version authority | **user confirmed beta 1.1.1-beta.1; stable promotion remains user-controlled** |
| Lifecycle | `~/.config/opencode/skills/dev-delegation/references/lifecycle.md` |
| Inner cycle | `~/.config/opencode/skills/dev-delegation/references/dev-cycle.md` |

> Rule: all six phases are accounted for. Work them in order; record `n/a — <reason>` where a phase
> genuinely does not apply. `done` requires evidence in the row. Update this file as the work moves,
> never at the end.

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | zack | §1 | Feature requirements; user confirmed 1.1.1-beta.1 for the current main delta, not stable promotion. |
| 2 | Design | done | zack | §2 + `unity-real-defaults-knobs.map.json` | `dev_modules.py`: 49 files, 0 unclaimed, 0 overlaps. |
| 3 | Build | done | zack | §3 | `cmake`/Ninja configure and snapshot harness compiled for arm64; beta label configure verified. |
| 4 | Test | done | zack | §4 | Full snapshot suite `RESULT 259/259 passed`; see evidence log. |
| 5 | Deploy | done | zack | §5 | Pushed clean HEAD, built arm64 AU, installed globally; codesign, plist, factory Open/Close and hashes verified. |
| 6 | Maintain | done | zack | §6 | Support intake is the README/project issue path; beta soak begins from commit `25a9200`. |

Status values: `pending` · `in-progress` · `done` · `blocked` · `n/a — <reason>`.

## 1. Requirements

### Problem
The rack lacks a global, switchable level-matching stage. RESET currently restores generic parameter defaults rather than a documented hardware-neutral operating profile. LA-2A artwork rotates opposite the slider and lacks the requested accurate shadow; 1176 knobs do not match the supplied 1179/1–7 references.

### Users and outcomes
The operator can toggle UNITY next to RESET to level-match the completed wet chain against its input, use RESET to return all controls to a documented hardware-neutral baseline, and see the supplied-style hardware controls with correct rotation and shadows.

### Scope
Append a host-automatable UNITY parameter while preserving all existing parameter identities/ranges/defaults; add a final stereo-linked loudness matching stage after chain/master/mix; define explicit RESET profile values without changing parameter-declared defaults; fix LA-2A control orientation/shadow; redraw 1176 controls to match the inline references; add regression checks, update DSP docs and README.

### Non-goals
No parameter ID/range/default changes for existing parameters; no invented stable hardware gain setting where hardware marks are arbitrary; no stable promotion/tag/release in this task; no per-sample waveform null guarantee (UNITY matches smoothed short-term RMS/energy); no public GitHub publication.

### Requirements (observable)

| ID | Requirement | Acceptance check (observable) | Priority |
| --- | --- | --- | --- |
| REQ-1 | UNITY is a clickable toggle next to RESET and acts after all modules, master and dry/wet. | Harness verifies disabled is bit-equivalent to existing path; enabled converges input/output stereo-linked short-term RMS, holds through silence, smoothly limits gain and prevents non-finite output; host parameter appended and legacy state migration defaults it off. | must |
| REQ-2 | RESET restores a researched, hardware-neutral profile for all controls without modifying existing declared defaults. | Harness reads every APVTS value after reset and compares against a named, documented profile; existing parameter IDs/ranges/defaults remain unchanged; DSP states, order and meters reset. | must |
| REQ-3 | LA-2A knobs rotate in the same direction as their value and have a drop shadow; 1176 input/output and attack/release follow the two inline reference images. | Snapshot renders show pointer/tick orientation, 1176 numeral scales and shadow; geometry assertions retain panel containment; no bitmap files required (attachments not present on disk). | must |
| REQ-4 | Maintain documentation and regression coverage. | Updated full harness result, README/source/docs agree on signal order and reset profile, spec contains all phase evidence. | must |

### Constraints
Latest stable base v1.0.1; user confirmed full current `main` delta for beta `1.1.1-beta.1`. Numeric project/AU version is 1.1.1, with beta suffix in the GUI and changelog only. Append new APVTS parameter only. Latest-only global install flow is commit → Ninja → installer. JUCE 9 / C++17 / macOS arm64. AU validation is user's job.

### Risks
Level matching can pump or boost silence unless guarded; a level match is metric/window based, not exact sample identity. UNITY after mix includes master and dry mix by design. UI references are inline only and source image files unavailable; procedural art must closely match visible design but may not be pixel-identical.

### Open questions

| ID | Question | Owner | Blocks design? |
| --- | --- | --- | --- |
| Q-1 | None; proceed with explicit RMS/energy definition and documented estimated hardware baselines. | zack | no |

## 2. Design

### Module map (exactly one owner per file; 0 unclaimed)

| Module | Owned globs | Owner | Notes |
| --- | --- | --- | --- |
| engine | `Source/Modules.h`, `Source/PluginProcessor.{h,cpp}`, `tools/dsp_checks.h` | inline orchestrator | UNITY DSP stage, reset profile, regression tests |
| gui | `Source/PluginEditor.{h,cpp}`, `tools/snapshot_main.cpp` | inline orchestrator | Header toggle and control artwork/snapshots |
| docs | `README.md`, this spec, `docs/dsp/signal-path.md` | inline orchestrator | User-facing behavior and signal-flow docs |
| project-and-remaining | All other tracked files, explicitly enumerated in the map | inline orchestrator | Unchanged infrastructure and existing assets/tests |

### Data and state
Append `unity` bool to APVTS; migrate older states to disabled. UNITY smoother state is runtime-only (stereo-linked linear gain, input/output energy followers) and resets to 0 dB on prepare/reset. RESET uses a separate immutable reset-profile table keyed by current parameter IDs plus explicit choice/toggle handling; it does not alter `getDefaultValue()`, so host double-click defaults and old sessions remain compatible. New profile is named **Hardware Neutral Starting Profile** and documented as a recommended starting preset, not a claim that hardware has one universal setting.

### Interfaces and seams
`processBlock` owns the last-stage signal change after master and parallel blend, then metering remains read-only after the new stage. The custom header control binds `unity` through host gesture callbacks. `resetToDefaults()` applies the explicit reset table, then retains existing order/meter/DSP-state clearing. Existing limiter auto gain remains separate and unchanged.

### Invariants (must not break)

| ID | Invariant | Where (`path:line`) |
| --- | --- | --- |
| INV-1 | No existing APVTS parameter ID, range or declared default changes; only new `unity` is appended. | `Source/PluginProcessor.cpp:5-75` |
| INV-2 | UNITY is after full rack, master and dry/wet; meters observe final output without modifying it. | `Source/PluginProcessor.cpp:131-265` |
| INV-3 | Reset parameter writes stay message-thread; DSP state reset is requested atomically and applied audio-thread. | `Source/PluginProcessor.cpp:349-392`, `136-146` |
| INV-4 | Host latency/rack permutation and global-only install policy remain invariant. | `Source/PluginProcessor.cpp:98-106`, `314-346`; repo `AGENTS.md` |

### Alternatives considered
Changing existing `createLayout()` defaults was rejected (host/session compatibility rule). Reusing limiter Auto Gain rejected because it is limiter-dependent and operates within module chain; final UNITY is independent after master/mix. Waveform sample equality rejected because nonlinear processors alter waveform; short-term stereo-linked RMS level matching is the meaningful interpretation.

### Verification strategy (how each requirement proves itself)
Extend DSP harness for reset profile values, UNITY on/off, convergence, silence and bounded correction; existing UI snapshot suite for visible labels, knob position, panel containment; build AU + existing factory/signature/plist checks after deploy; user validates in DAW.

### Rollback
Revert the feature commit(s); global installer has the existing backup/guard procedure documented in its script. No migration schema mutation; old project states load UNITY off.

### Deploy topology
Gitea main only. Beta label `1.1.1-beta.1` confirmed by user; no stable release/tag. Latest-only flow: commit clean tree, build latest commit with `-DZYCHANSTRIP_BETA=beta.1`, `tools/install_global.sh` global only. The installer replaces the existing bundle; verify the current artifact before deployment and retain its identity/hash for rollback documentation.

### Observability
UNITY gain meter/readout optionally exposed from atomic runtime dB state; tests assert bounded target and finite samples. Installer logs installed commit and hash.

### Design decisions

| ID | Decision | Covers | Rejected alternative |
| --- | --- | --- | --- |
| DES-1 | Add final-stage stereo-linked RMS level matcher using attack/release-smoothed energy, silence hold, bounded gain, and sample-smooth gain ramp; toggle is host-automatable. | REQ-1 | Limiter-only auto gain |
| DES-2 | Add named Hardware Neutral Starting Profile separate from APVTS declared defaults, preserving compatibility. Use conservative positions based on official UA manuals and visible control scales; explicitly label compressor points as suggested starting positions. | REQ-2 | Rewrite existing parameter defaults |
| DES-3 | Correct LA-2A image rotation sign and build cast shadow beneath silhouette; procedurally draw 1176 0–48/∞ scale and 1–7 scales to match inline references, plus shadows. | REQ-3 | Embed inaccessible Desktop images |
| DES-4 | Update tests/docs and identify the beta as 1.1.1-beta.1; do not create a stable tag/release. | REQ-4 | Publish beta as stable |

## 3. Build

### Build plan
Preserve all pre-existing APVTS parameter IDs, ranges, defaults and index order; append `unity` after `lim_auto_gain`. Add independent processor-level stereo RMS tracking and sample-smoothed gain at the final output; append a named Hardware Neutral Starting Profile in `resetToDefaults()` and explicitly apply `fet_ratio` choice. Add a hand-drawn host-automatable UNITY pill beside RESET. Correct LA-2A rotation sign and shadow; add 1176 reference scales. Tests cover reset profile, attachments, final output matching, silence and unchanged disabled path. Update README and `docs/dsp/signal-path.md`.

### Files and globs
`Source/PluginProcessor.{h,cpp}`, `Source/PluginEditor.{h,cpp}`, `tools/dsp_checks.h`, `tools/snapshot_main.cpp`, `README.md`, `docs/dsp/signal-path.md`, this spec. No CMake or dependency changes anticipated.

### Dependency / version pins
No dependency changes; JUCE pin unchanged. Numeric project/AU bundle version `1.1.1`; GUI beta label `v1.1.1-beta.1`. C++17/macOS arm64.

### Build commands
`ninja -C build`; then `python3 tools/build_snapshot.py` and `./build/snapshot/snapshot <task-owned-output-dir>` expecting a green complete count. Installation only after clean commit and push, via `tools/install_global.sh`.

```sh
# exact command(s); paste real output under Evidence log
```

### Deviations from design

## 4. Test

### Test plan (requirement -> test)

| ID | Test | Covers | Type | Command | Result |
| --- | --- | --- | --- | --- | --- |
| TST-1 | Final UNITY matches input after -9 dB master trim; legacy state migration and silence guard. | REQ-1 | DSP + migration | `$HOME/.config/opencode/temp/zychanstrip-beta-111/shots` | PASS — `-0.205 dB`; silence peak `0`; toggle attachment passes |
| TST-2 | Reset applies all 42 explicit profile values, order/DSP/meters reset; existing parameter defaults unchanged. | REQ-2 | DSP/state | `$HOME/.config/opencode/temp/zychanstrip-beta-111/shots` | PASS — all 42 profile entries checked and order/DSP/meters assertions pass |
| TST-3 | GUI snapshots/control tests; LA-2A rotation sign corrected; 1176 numeric scales and drop shadows rendered. | REQ-3 | GUI/snapshot | `$HOME/.config/opencode/temp/zychanstrip-beta-111/shots` | PASS — whole GUI snapshot/geometry suite passes; procedural scales rendered in snapshots |
| TST-4 | Whole suite green and README/signal flow agree. | REQ-4 | Integration | `build/snapshot/snapshot` | PASS — RESULT 259/259 |

### Red-first evidence (bugs only)
N/A — new feature, not a reported defect. Existing LA-2A rotation direction is corrected as part of explicit visual acceptance.

### Independent verification
No separate verifier lane was run; the Herdr skill forbids extra agent/pane creation without explicit authorization. The orchestrator reviewed source and reran the full harness; independent verification remains a gap.

### Known gaps / not verified
Exact supplied images were absent at the advertised Desktop paths; procedural scales/shadows follow the visible inline references but are not pixel-matched. DAW host validation is user responsibility. AU install/signature/loadability not yet run; installer requires a clean pushed HEAD.

### Required Maintain status
Lifecycle state: release task in progress; finish Maintain with the installed-artifact evidence and support/feedback path.

## 5. Deploy

### Version and authority
User explicitly confirmed beta `1.1.1-beta.1`; numeric project/AU version is `1.1.1` and the GUI/changelog carry the beta suffix. No stable promotion, tag, or release is authorized.

### Release diff (patch/beta only)
Full current Gitea `main` delta since latest stable `v1.0.1`, explicitly accepted by user for beta `1.1.1-beta.1`.

### Packaging, signing, notarization
Build `AU` format; global install script performs ad-hoc re-seal and codesign verification. No notarization/public distribution.

### Staging / install
Commit and push `main` to Gitea, rebuild clean latest `HEAD`, then use only `tools/install_global.sh`; never install from dirty/uncommitted work or to `~/Library`.

### Smoke on the real artifact
After install verify global component hash, `codesign -v`, plist codes/version and AU factory construction/open-close. DAW/auval validation is the user’s job.

### Rollout and rollback
Global-only. Rollback: retain existing bundle backup and restore it if smoke checks fail; installer’s built-in protections remain authoritative.

### Notes and changelog
Documented in README and CHANGELOG.md under 1.1.1-beta.1.

### Distribution identity / anonymity gate (public targets)
N/A — private Gitea only; no GitHub publication.

### Deployment record
Beta requested: commit and push `main` to Gitea, then build that clean latest HEAD with the beta suffix, install globally and verify the installed artifact. No stable tag/release.

| ID | Step | Command | Evidence |
| --- | --- | --- | --- |
| DEP-1 | Build/test | `ninja -C build`; snapshot 259/259 | PASS; pre-beta feature baseline |
| DEP-2 | Global install latest committed build | `tools/install_global.sh` via administrator prompt | PASS — source md5 `ad5ff34a123e2ef215d89d135f7d1813`; installed md5 after ad-hoc reseal `58f5270819355944aec3e684e700155a`; root:wheel; user copy absent |
| DEP-3 | Installed AU identity and loadability | `codesign -v`; PlistBuddy; direct `zychanstripAUFactory` probe on installed executable | PASS — codesign valid; `net.zyusof.zychanstrip`, `1.1.1`, `aufx`/`Zych`/`ZYUS`, component version `65793`, arm64; `Open=0`, `Close=0`. `validation: user` for DAW/host behavior |

## 6. Maintain

### Monitoring and alerts
UNITY is observable via its live correction readout; no new service telemetry required.

### Support intake
Normal project issue/support channel; new failure reports become scoped requirements and regressions are diffed against the latest release.

### Known issues
Inline knob images were not accessible; artwork is procedurally recreated and may differ in exact detailing.

### Deprecation
No deprecation introduced.

### Docs and skill updates
README and signal-flow docs updated; no skill change required.

### Feedback loop (incident -> new requirement)
User DAW feedback on loudness stability and knob fidelity feeds the next explicit requirement; preserve current compatibility invariants.

### Maintenance record

| ID | Item | Owner | Cadence | Last verified |
| --- | --- | --- | --- | --- |
| MNT-1 | Review user UNITY/knob feedback; do not alter IDs/defaults without explicit compatibility review. | project owner | On feedback | 2026-11-03 |

## Traceability

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1 | DEP-2 | MNT-1 |
| REQ-2 | DES-2 | TST-2 | DEP-2 | MNT-1 |
| REQ-3 | DES-3 | TST-3 | DEP-2 | MNT-1 |
| REQ-4 | DES-4 | TST-4 | DEP-1 | MNT-1 |

## Evidence log

| Date | Phase | Command / check | Result |
| --- | --- | --- | --- |
| 2026-11-03 | Design | `python3 ~/.config/opencode/scripts/dev_modules.py --repo . --spec docs/specs/unity-real-defaults-knobs.map.json` | 49 files; 0 unclaimed; 0 overlaps |
| 2026-11-03 | Test | `ninja -C build`; `python3 tools/build_snapshot.py`; `build/snapshot/snapshot …` | build passed; RESULT 259/259 |
| 2026-11-03 | Cleanup | Removed `$HOME/.config/opencode/temp/zychanstrip-unity` snapshot output | task-owned temporary snapshots removed |
