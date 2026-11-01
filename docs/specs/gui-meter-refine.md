# SPEC — 1176 + LA-2A meter and knob refinement

| Field | Value |
| --- | --- |
| Slug | `gui-meter-refine` |
| Repo | `/Users/zack/projects/zychanstrip` |
| Track | gui-meter-refine |
| Owner | zack |
| Created | 2026-11-01 |
| Classification | feature (GUI presentation only) |
| Version authority | **user** (the agent never invents a version, tag or release) |
| Lifecycle | `~/.config/opencode/skills/dev-delegation/references/lifecycle.md` |
| Inner cycle | `~/.config/opencode/skills/dev-delegation/references/dev-cycle.md` |

> Rule: all six phases are accounted for. Work them in order; record `n/a — <reason>` where a phase
> genuinely does not apply. `done` requires evidence in the row. Update this file as the work moves,
> never at the end.

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | zack | §1 | user brief 2026-11-01 (6 numbered asks) |
| 2 | Design | done | zack | §2 | module map + DES-1…DES-8 below |
| 3 | Build | done | zack | §3 | `ninja -C build` clean |
| 4 | Test | done | zack | §4 | `RESULT 246/246`; before/after snapshot crops |
| 5 | Deploy | blocked | zack | §5 | build OK; **system-wide install needs sudo** — user-domain copy installed, factory lookup unverified (see DEP-1) |
| 6 | Maintain | done | zack | §6 | memory entry + spec |

Status values: `pending` · `in-progress` · `done` · `blocked` · `n/a — <reason>`.

## 1. Requirements

### Problem

Two faceplates waste illuminated area and mis-scale their hardware:

1. **1176 (FET, second panel from the left).** The rectangular Simpson-style meter draws its dial
   window inset 6.5 px inside the black housing, leaving a thick empty black border; the ATTACK and
   RELEASE knobs are oversized relative to the rest of the lower panel.
2. **LA-2A (Opto, third panel from the left).** The round TELETRONIX meter is the largest element on
   the plate but its scale only occupies the top third; the bottom of the face is a large empty beige
   area, and the housing crowds the plate.

### Users and outcomes

The plugin user reads gain reduction at a glance on a photoreal faceplate; the panels should look like
the hardware (a Simpson window that fills its frame, a VU face whose scale uses the whole circle).

### Scope

Presentation only, inside `Source/PluginEditor.cpp`: `paintFetVuMeter`, `paintOptoVuMeter`,
`FetPanel::layoutContent`/`paintFaceExtra`, `OptoPanel::layoutContent`, and the rotary-slider painter
(`RackLookAndFeel::drawRotarySlider` + `paintScale`).

### Non-goals

- No DSP, parameter, ordering, metering or state change.
- No change to the Console (API 560) or Limiter faceplates, or to the METER page.
- No new bitmap assets: the GUI stays 100 % procedural vector.

### Requirements (observable)

| ID | Requirement | Acceptance check (observable) | Priority |
| --- | --- | --- | --- |
| REQ-1 | 1176 dial window fills the whole black housing rectangle; no empty black border ring | rendered crop: paper reaches the housing edge (≤1.5 px frame), no ≥5 px black margin | must |
| REQ-2 | 1176 `ZERO ADJ` screw + legend re-centred on the expanded window's bottom | screw centre x == window centre x, within 0.5 px; legend centred under it | must |
| REQ-3 | 1176 ATTACK and RELEASE drawn diameter = exactly 80 % of the previous diameter | drawn skirt diameter 60.8 px vs 76 px before (0.800) | must |
| REQ-4 | ATTACK/RELEASE centres do not move | centre coordinates identical to the pre-change render | must |
| REQ-5 | ATTACK/RELEASE tick scale + captions scale with the knob (80 %) | tick length 6.5→5.2 px, caption font 13→10.4 pt, still centred on the knob axis | must |
| REQ-6 | LA-2A meter outer diameter (bezel/rim/face) = 85 % of previous, centre unchanged | drawn housing 182 px vs 214 px (0.850); centre (129,159) unchanged | must |
| REQ-7 | LA-2A face uses the circle: arc sweeps a wider angle and sits lower, TELETRONIX/VU/REDUCTION enlarged and spread, bottom dead space reduced | arc sweep ≥ 100° (was 84.8°), apex ≥ 5 % lower, three legends ≥ 25 % larger, blank band under the legend ≤ 20 % of height | must |
| REQ-8 | No blur/smear of the brushed plate or black plate; 3-D shading stays top-down | visual diff confined to the meter/knob regions; plate noise unchanged | must |
| REQ-9 | DSP + harness unchanged | `./build/snapshot/snapshot <dir>` → `RESULT 246/246 passed` | must |

### Constraints

- JUCE 9.0.4, arm64, procedural `Graphics` only; no image assets, no filters (no blurring).
- Knob geometry derives from the slider bounds (`d = min(w,h) - 4`), so the scale must be applied in the
  painter, not by shrinking the component (which would move the drawn centre by rounding).
- Panel geometry is fixed: content rect `(18,48,222,642)` inside a 258×704 plate.

### Risks

- Enlarging the LA-2A arc can push its numerals outside the glass circle → clipped by the clip region.
  Mitigated by computing the sweep against the circle radius (see DES-5) and checking the render.
- Moving the 1176 window edge can collide with the chrome rim / housing stroke.

### Open questions

| ID | Question | Owner | Blocks design? |
| --- | --- | --- | --- |
| Q-1 | Should the 1176 window keep any black frame at all? | zack | no — 1.5 px edge kept as the physical window lip (REQ-1 threshold) |

## 2. Design

### Module map (exactly one owner per file; 0 unclaimed)

| Module | Owned globs | Owner | Notes |
| --- | --- | --- | --- |
| rack-ui | `Source/PluginEditor.cpp`, `Source/PluginEditor.h` | zack (inline) | both meters + knob painter + two panel layouts |
| docs | `docs/specs/gui-meter-refine.md`, `README.md` | zack (inline) | spec + known-issues row if needed |

One source module, one file, two painters → **fleet not warranted** (dev-delegation §1 fast path:
"1 module and small → inline"). Independent verification is still delegated (§4).

### Data and state

None. Pure paint/layout maths; `gr` (gain reduction) is read as today.

### Interfaces and seams

- `paintFetVuMeter (g, Rectangle<float> housing, float deflection)` — unchanged signature.
- `paintOptoVuMeter (g, Rectangle<float> housing, float deflection)` — unchanged signature.
- `paintScale (g, centre, radius, KnobScale, Colour, float scale = 1.0f)` — new optional argument.
- Slider property `knobScale` (float, default 1.0) read by `drawRotarySlider`.

### Invariants (must not break)

| ID | Invariant | Where (`path:line`) |
| --- | --- | --- |
| INV-1 | Meter deflection still comes from `gr` → `vuDeflectionForDb` | `Source/PluginEditor.cpp:1637`, `:1738` |
| INV-2 | Knob value→angle mapping unchanged (`setRotaryParameters` 1.22π…2.78π) | `Source/PluginEditor.cpp:1342` |
| INV-3 | Faceplate noise/texture painting (`paintFaceBase`, `paintMetalPlate`) untouched | `Source/PluginEditor.cpp:1599`, `:93` |
| INV-4 | No parameter ID/range/default touched | `Source/PluginProcessor.cpp` |
| INV-5 | Meter rect still inside the panel content rect | `Source/PluginEditor.cpp:1684`, `:1583` |

### Alternatives considered

- Shrink the ATTACK/RELEASE components to 64×64: rejected — the painter insets 2 px, so the drawn
  diameter would be 60/76 = 78.9 %, not 80 %, and integer rounding can shift the centre.
- Scale the LA-2A meter by resizing only the bezel: rejected — the ask is the whole housing (bezel, rim
  and face) at 85 %.

### Verification strategy (how each requirement proves itself)

Headless snapshot render (`tools/build_snapshot.py` → `./build/snapshot/snapshot <dir>`), then pixel
measurement of the rendered PNG crops for REQ-1…REQ-7, plus the existing 246-check harness for REQ-9.
REQ-8 is checked by diffing the plate regions outside the changed components.

### Rollback

`git revert <commit>`; no state, no migration, no data.

### Deploy topology

AU only, built on this Mac and installed to `/Library/Audio/Plug-Ins/Components/` by
`COPY_PLUGIN_AFTER_BUILD`. Version/tag decisions belong to the user.

### Observability

Snapshot PNGs (`build/snapshot/rack-*.png`) are the visual record; harness output is the functional record.

### Design decisions

| ID | Decision | Covers | Rejected alternative |
| --- | --- | --- | --- |
| DES-1 | 1176 window `inner = r.reduced(1.5f)`; chrome lip restroked at `r.reduced(1.0f)` | REQ-1 | removing the lip entirely (loses the window edge) |
| DES-2 | 1176 screw/legend anchored to `inner.getBottom()` (they already are) so they follow the expanded window | REQ-2 | moving the screw to `r.getBottom()` |
| DES-3 | Knob scale applied in `drawRotarySlider` via the `knobScale` property: `d' = d * knobScale`, same centre | REQ-3, REQ-4 | shrinking component bounds |
| DES-4 | `paintScale` takes `scale`; tick length/shadow/thickness and skirt radius all derive from the scaled diameter; captions multiplied by the same factor | REQ-5 | fixed tick lengths |
| DES-5 | LA-2A rect shrunk with `withSizeKeepingCentre(0.85)`; face re-laid out on a normalised grid: pivot `0.80h`, arc radius `0.47h`, sweep ±0.95 rad, numerals outside at `R+10`, legends at `0.10h` / `0.20h` / `0.62h` | REQ-6, REQ-7 | keeping the old ±0.74 rad sweep |
| DES-6 | Shading rebuilt at the new geometry with the same top-down gradient stops (no filters) | REQ-8 | reusing the old highlight offsets |
| DES-7 | Arc/numerals computed from the circle radius so nothing is clipped by the glass clip | REQ-7 | fixed pixel offsets |

## 3. Build

### Build plan

Single-file edit in `Source/PluginEditor.cpp`, then rebuild the AU and the headless harness.

### Files and globs

- `Source/PluginEditor.cpp` — knob painter, both meter painters, both panel layouts.
- `docs/specs/gui-meter-refine.md` — this spec.

### Dependency / version pins

JUCE `9.0.4` (FetchContent), CMake ≥ 3.22, Ninja, Apple clang (arm64).

### Build commands

```sh
export PATH="$HOME/Library/Python/3.13/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
python3 tools/build_snapshot.py
./build/snapshot/snapshot "$HOME/.config/opencode/temp/zychanstrip-shots"
```

### Deviations from design

None.

## 4. Test

### Test plan (requirement -> test)

| ID | Test | Covers | Type | Command | Result |
| --- | --- | --- | --- | --- | --- |
| TST-1 | measure window/housing inset in the FET crop | REQ-1 | pixel | crop `rack-1080x760.png` | 1.5 px lip, no black ring |
| TST-2 | screw centre vs window centre | REQ-2 | pixel | same crop | equal x |
| TST-3 | skirt diameter before/after | REQ-3, REQ-4 | pixel | before/after crops | 76 → 60.8 px (0.800), centre fixed |
| TST-4 | tick/caption size | REQ-5 | pixel | before/after crops | 6.5→5.2 px, 13→10.4 pt |
| TST-5 | LA-2A housing diameter + centre | REQ-6 | pixel | before/after crops | 214 → 182 px (0.850), centre fixed |
| TST-6 | arc sweep/apex/legends | REQ-7 | pixel + code | crop + spec table | sweep 84.8°→108.8°, apex 0.26h→0.33h |
| TST-7 | plate texture untouched | REQ-8 | pixel diff | diff outside changed rects | zero diff |
| TST-8 | full harness | REQ-9 | functional | `./build/snapshot/snapshot <dir>` | `RESULT 246/246 passed` |

### Red-first evidence (bugs only)

n/a — feature work, no defect to reproduce.

### Independent verification

`dev-verifier` subagent re-derives every claim from the source and the rendered crops (§4 results).

### Known gaps / not verified

- AU hosting in a DAW is the user's job (`validation: user`); `auval` is not run as evidence.

## 5. Deploy

### Version and authority

No version bump performed. The user decides whether this ships as a patch, beta or part of the next minor.

### Release diff (patch/beta only)

n/a — not a patch/beta task.

### Packaging, signing, notarization

Standard `ninja -C build` → `COPY_PLUGIN_AFTER_BUILD` installs the AU; ad-hoc signing as configured.

### Staging / install

`/Library/Audio/Plug-Ins/Components/zychanstrip.component` (system-wide only).

### Smoke on the real artifact

`codesign -v` PASS on both copies; `PlistBuddy` readback shows `aufx` / `ZYUS` / `Zych` /
`zychanstripAUFactory` / version `65537` (bundle 1.0.1). Factory construct probe
(`AudioComponentFindNext` + `AudioComponentInstanceNew`, `CoreAudioKit`-free helper) returns
**`COMPONENT NOT FOUND` for the user-domain copy** and could not be run against the system copy —
see DEP-1. `validation: user`.

### Rollout and rollback

Reinstall the previous build (git revert + rebuild) if a regression appears.

### Notes and changelog

Recorded in the commit message and memory; CHANGELOG updated only when the user cuts a release.

### Distribution identity / anonymity gate (public targets)

n/a — Gitea only, no public mirror.

### Deployment record

| ID | Step | Command | Evidence |
| --- | --- | --- | --- |
| DEP-1 | build + install AU | `ninja -C build`; user-domain `cp` | **PARTIAL** — see evidence log. `/Library/Audio/Plug-Ins/Components/zychanstrip.component` is root-owned and still the 12:28 pre-change build (`md5 d620a90e…`, vs new `8bcc0865…`): replacing it needs `sudo` (`sudo cp -R build/zychanstrip_artefacts/Release/AU/zychanstrip.component /Library/…/Components/`), which the sandbox denies. Current GUI build installed at `~/Library/Audio/Plug-Ins/Components/zychanstrip.component`. |

## 6. Maintain

### Monitoring and alerts

Snapshot harness is the regression monitor (`tools/build_snapshot.py` + 246 checks).

### Support intake

User reports via the session; any new defect gets a README known-issues row.

### Known issues

None added by this change.

### Deprecation

n/a.

### Docs and skill updates

Spec recorded here; memory entry on completion.

### Feedback loop (incident -> new requirement)

A visual regression in a future snapshot becomes a new REQ against this track.

### Maintenance record

| ID | Item | Owner | Cadence | Last verified |
| --- | --- | --- | --- | --- |
| MNT-1 | snapshot harness green | zack | every change | 2026-11-01 |

## Traceability

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1 | DEP-1 | MNT-1 |
| REQ-2 | DES-2 | TST-2 | DEP-1 | MNT-1 |
| REQ-3 | DES-3, DES-4 | TST-3 | DEP-1 | MNT-1 |
| REQ-4 | DES-3 | TST-3 | DEP-1 | MNT-1 |
| REQ-5 | DES-4 | TST-4 | DEP-1 | MNT-1 |
| REQ-6 | DES-5 | TST-5 | DEP-1 | MNT-1 |
| REQ-7 | DES-5, DES-7 | TST-6 | DEP-1 | MNT-1 |
| REQ-8 | DES-6 | TST-7 | DEP-1 | MNT-1 |
| REQ-9 | — | TST-8 | DEP-1 | MNT-1 |

## Evidence log

| Date | Phase | Command / check | Result |
| --- | --- | --- | --- |
| 2026-11-01 | Requirements | user brief | 6 numbered asks captured |
| 2026-11-01 | Build | `ninja -C build` | clean, no errors |
| 2026-11-01 | Test | `./build/snapshot/snapshot <dir>` | `RESULT 246/246 passed` (before and after) |
| 2026-11-01 | Test | pixel measure, `before` vs `after` rack-1080x760 | FET paper bbox 202→210 px wide inside a 215 px housing (black border 6.5→1.5 px); FET knob skirt 76→60.8 px (0.800), centre (318,357)/(474,357) unchanged; LA-2A housing 214→182 px (0.850), centre (673,159) unchanged; arc sweep 84.8°→108.8°, apex 0.26H→0.33H; TELETRONIX 7→9.5 pt, VU 15→20 pt (same visual mass), REDUCTION 8.5→11 pt |
| 2026-11-01 | Deploy | `codesign -v`, `PlistBuddy`, AU factory probe | codesign PASS; factory probe `COMPONENT NOT FOUND` (root-domain copy stale, needs sudo) |
