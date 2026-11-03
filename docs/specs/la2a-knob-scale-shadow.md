# SPEC — LA-2A knob scale, endpoints and shadow correction

| Field | Value |
| --- | --- |
| Slug | `la2a-knob-scale-shadow` |
| Repo | `/Users/zack/projects/zychanstrip` |
| Classification | bug (beta patch) |
| Version authority | User-owned; stay on current beta line; do not invent tag/version |
| Base | Gitea `main` `59074ac`, latest stable `v1.0.1` |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | zack | §1 | Exact user reference and official UA numbered LA-2A control graphic fetched; symptoms classified below. |
| 2 | Design | done | zack | §2 | Follow official UA diagram: clockwise 0–100 sweep ≈225°; rotate the single paddle image as the index, with labels outside and shadow from its true transparent silhouette. |
| 3 | Build | done | zack | `Source/PluginEditor.cpp` | One paddle image is the sole index; cached alpha-derived shadow rotates with it; original rotary drag direction retained. |
| 4 | Test | done | zack | `tools/build_snapshot.py`; min/mid/max snapshots | Full harness `RESULT 263/263 passed` after final drag-direction change; fresh endpoints inspected; `git diff --check` clean. |
| 5 | Deploy | pending | zack | §5 | Gitea commit/push, global-only install and AU factory/signature/plist checks. |
| 6 | Maintain | pending | zack | §6 | Support through project issue channel; beta soak after install. |

## 1. Requirements

| Symptom | In `v1.0.1..main`? | Classification | Minimal fix |
| --- | --- | --- | --- |
| LA-2A scale endpoints/start and end angles do not match faceplate/reference | Yes: `drawDavies1470`, `paintScale`, and slider `setRotaryParameters` changed | REGRESSION | Make custom scale use actual normalized slider proportion and correct arc endpoints. |
| Shadow does not move correctly with pointer/knob | Yes: bespoke LA-2A shadow is a static ellipse; `paintScale` shadows are always vertical | REGRESSION | Derive cast shadow from same value angle and translate its silhouette down/right. |
| Numbering like attached 0–100 LA-2A scale is absent | Yes: custom renderer draws ticks only | REGRESSION | Add radial 0–100 decade labels matching attached faceplate. |

### Requirements (observable)

| ID | Requirement | Acceptance check (observable) | Priority |
| --- | --- | --- | --- |
| REQ-1 | LA-2A dial scale and actual knob have matching correct endpoints and travel. | Snapshots show min/mid/max values with knob pointer aligned to the corresponding scale positions. | must |
| REQ-2 | LA-2A number labels match supplied 0–100 reference. | Render labels 0,10,…,100 around arc and verify ordering/placement in generated snapshot. | must |
| REQ-3 | Cast shadow moves with knob orientation, with coherent light direction. | At min/mid/max snapshots, offset silhouette is rotated with knob and consistently lower-right. | must |

Non-goals: no DSP/parameter/default changes; no unrelated 1176 changes; no stable release/tag; no Intel/VST3 build.

## 2. Design

Single-module renderer change in `Source/PluginEditor.cpp`; one normalized slider proportion controls scale, labels and knob orientation. The source paddle is the sole moving index. A cached shadow image is derived once from the PNG alpha and drawn with matching rotation followed by a fixed lower-right screen-space offset. No new dependency or utility.

Invariants: existing slider parameter mapping and direction remain correct; drawing is paint-only; all artwork remains inside component bounds; do not change other knob families.

| ID | Decision | Covers | Rejected alternative |
| --- | --- | --- | --- |
| DES-1 | Use one `s.valueToProportionOfLength` proportion for scale and source-knob rotation; the image paddle is the sole index; put labels outside the face and derive the cached offset shadow from source alpha. | REQ-1, REQ-3 | Drawing a duplicate radial pointer over the paddle or using JUCE fill-alpha tint, which also changes RGB |
| DES-2 | Render 0–100 decade numerals along the actual dial arc. | REQ-2 | Generic ticks-only `paintScale` |
| DES-3 | Rotate the alpha-derived cast shadow with knob angle and apply one fixed lower-right light offset. | REQ-3 | Static oval under knob |

### Tests

Red-first: reproduce current snapshot and inspect reference geometry. Then assert/inspect endpoints, monotonic label mapping and shadow orientation at minimum/mid/maximum values in generated snapshots. Run complete `tools/build_snapshot.py` suite.

## 3. Build

Commands: `ninja -C build`; `python3 tools/build_snapshot.py`; snapshot invocation to task-owned temp. Build clean committed latest `main`; routine target arm64 AU only.

## 4. Test

| ID | Requirement | Test | Covers | Type | Command | Result |
| --- | --- | --- | --- | --- | --- | --- |
| TST-1 | Endpoints + mapped values | inspect snapshots at minimum/middle/maximum | REQ-1 | GUI | `la2a-knobs-min/mid/max.png` | PASS — 0 endpoint at ~7:30, midpoint at top, 100 endpoint at ~4:30; paddle follows arc with original drag direction retained. |
| TST-2 | Number ring | inspect rendered LA-2A against exact fetched reference and official UA diagram | REQ-2 | GUI | same snapshots vs reference URL and UA manual graphic | PASS — 0–100 decade labels legible and clockwise. |
| TST-3 | Moving shadow | compare values at extremes and center | REQ-3 | GUI | same snapshots | PASS — alpha-derived shadow follows knob angle with fixed lower-right offset. |
| TST-4 | Regression | run full snapshot harness | REQ-1–3 | integration | `tools/build_snapshot.py`; snapshot | PASS — RESULT 263/263 passed. |

## 5. Deploy

No stable version or tag. Commit/push Gitea main, build current clean HEAD, install with `tools/install_global.sh`. Verify hash, `codesign -v`, plist AU identity/version/arm64 and direct AU factory `Open`/`Close`. `validation: user` for DAW behavior.

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1, TST-4 | DEP-1 | MNT-1 |
| REQ-2 | DES-2 | TST-2, TST-4 | DEP-1 | MNT-1 |
| REQ-3 | DES-1, DES-3 | TST-3, TST-4 | DEP-1 | MNT-1 |

| ID | Step | Command | Evidence |
| --- | --- | --- | --- |
| DEP-1 | Push/build/install and verify global arm64 AU | Gitea commit, `ninja -C build`, `tools/install_global.sh`, signature/plist/factory checks | pending |

## 6. Maintain

Owner: project maintainer. Intake: project support/issues. Beta soak starts after global install; user visual/DAW feedback becomes follow-up requirements.

| ID | Item | Owner | Cadence | Last verified |
| --- | --- | --- | --- | --- |
| MNT-1 | Review beta knob fidelity reports | project maintainer | On user feedback | pending |
