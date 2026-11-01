# SPEC — Photo-real hardware-style GUI matching reference

| Field | Value |
| --- | --- |
| Slug | `photoreal-gui` |
| Repo | `git.zyusof.net/zack/zychanstrip` (then `/Users/zack/Desktop/rackstrip` working copy) |
| Track | photoreal-gui |
| Owner | zack |
| Created | 2026-10-31 |
| Classification | feature |
| Version authority | **user** (the agent never invents a version, tag or release) |
| Lifecycle | `~/.config/opencode/skills/dev-delegation/references/lifecycle.md` |
| Inner cycle | `~/.config/opencode/skills/dev-delegation/references/dev-cycle.md` |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | agent | §1 | REQ-1..REQ-9 each carry an observable check |
| 2 | Design | done | agent | §2 | every REQ covered by a DES; build succeeded from this design |
| 3 | Build | done | agent | §3 | `cmake --build` exit 0, AU linked + installed |
| 4 | Test | done | agent | §4 | headless snapshot 1376x768 rendered; 28/28 params bound |
| 5 | Deploy | done | agent | §5 | installed to ~/Library/Audio/Plug-Ins/Components |
| 6 | Maintain | in-progress | agent | §6 | memory + spec written; AU validation blocked (#24826) |

Status values: `pending` · `in-progress` · `done` · `blocked` · `n/a — <reason>`.

## 1. Requirements

### Problem

The current editor is a flat, dark, code-drawn approximation: rounded rectangles, generic rotary
knobs with hairline ticks, plain LED strips and a text title. The user supplied a reference image of a
photo-real hardware "mastering rack" and asked for the GUI to be changed to match it. The reference
reads as physical metal because of specular gradients, bezelled screws, brass VU movements, coloured
bakelite knobs with skirt scales, a chrome fader and glowing segmented LED ladders.

### Users and outcomes

The plugin owner, opening the editor in a DAW or `auval`, sees a rack that looks like four pieces of
hardware in a window: API-550B-style EQ + dynamics face, Teletronix-LA-2A-style opto unit with a large
brass VU, UREI-1176-style FET unit with a blue ratio bank and a small VU, and an SSL-style limiter with
twin vertical LED ladders and a chrome ENHANCE fader.

### Scope

`Source/PluginEditor.cpp` and `Source/PluginEditor.h` only — look-and-feel, panel faces, layout,
window chrome. No DSP, no parameter, no state-format change.

### Non-goals

- No DSP/audio change. `Source/Modules.h`, `Source/PluginProcessor.cpp` untouched.
- No new parameters. The drawings map onto the **existing** 22 parameters only.
- No copyrighted trademarks: the reference shows "API 550B", "LA-2A", "UREI 1176". The README already
  states the design copies no manufacturer's names or trademarks, so panels are labelled in a neutral
  house style (`EQUALISER`/`DYNAMICS`, `OPTO LEVELER`, `FET COMPRESSOR`, `MASTER LIMITER`) rendered in
  flat gold/white engraving, not the manufacturer wordmarks.
- No image assets; all faces stay procedurally drawn in `paint()` so it scales and adds no binary size.

### Requirements (observable)

| ID | Requirement | Acceptance check (observable) | Priority |
| --- | --- | --- | --- |
| REQ-1 | Window is a photo-real rack chassis: dark metal surround with bevelled edge, a header bar with traffic-light glyphs at left, centred uppercase product title, and a Master level control at right | Screenshot shows header bar, three glyphs, centred title, right-side master slider | must |
| REQ-2 | Each module is a separate metal face plate with four corner screws, a drop shadow/gap between plates, and a six-dot drag grip above it | 4 distinct plates, 16 screws, 4 grips visible | must |
| REQ-3 | Knobs render as photo-real knobs: skirt with visible value scale, specular highlight, drop shadow, and a direction the pointer can be read from at 1x | Each knob has a highlight arc, skirt tick ring, and a readable indicator line | must |
| REQ-4 | EQ/dynamics plate uses coloured caps (red Low, blue High-Mid, green gain, black dynamics) on a near-black face and is labelled EQ / Dynamics with Comp/Gate indicators | Plate 1 shows 6 colour-coded EQ knobs + Comp/Gate + 4 black dynamics knobs | must |
| REQ-5 | Opto plate is a light brushed-metal face with a large brass VU reading REDUCTION, two large black knobs (INPUT GAIN, PEAK REDUCTION), an ON toggle and a METER toggle | Plate 2 light face + large VU with needle + 2 black knobs + 2 toggles | must |
| REQ-6 | FET plate is dark with two silver INPUT/OUTPUT knobs, a backlit button bank 4:1 8:1 12:1 20:1 ALL, a small brass VU showing GR, and two silver ATTACK/RELEASE knobs | Plate 3 shows silver knobs, 5-button bank, small VU, 2 lower knobs | must |
| REQ-7 | Limiter plate has THRESHOLD/CEILING/RELEASE knobs on top, a chrome vertical fader labelled ENHANCE with a VU-style dB scale, and two tall segmented LED ladders (amber + green/blue) | Plate 4 shows 3 knobs, chrome fader with scale, 2 ladders with lit segments | must |
| REQ-8 | All existing parameters remain bound and controllable; drag-to-reorder still works | `auval` parameter count == 22; drag grip moves a module and persists order | must |
| REQ-9 | Rendering is resolution-independent (no baked bitmaps) and the editor opens at a fixed 1376x768-equivalent aspect without clipping | Screenshot at 1032x576 and 1376x768 both render every label inside its plate | should |

### Constraints

- JUCE 8.0.4, C++17, arm64. `LookAndFeel_V4` subclass, no external asset files.
- Editor must stay usable in the existing fixed `setSize` model used by the reorder animator.
- Indices into `juce::AudioProcessorValueTreeState` must not change (no state migration).

### Risks

- Custom `drawLinearSlider` for both the fader and the master must not break `LinearBar`/`LinearVertical`
  used elsewhere.
- Very heavy per-frame painting could raise CPU; faces are painted once into a cached `Image`, meters
  only repaint the moving parts.

### Open questions

| ID | Question | Owner | Blocks design? |
| --- | --- | --- | --- |
| Q-1 | Manufacturer wordmarks on the reference faces are trademarked — reproduce? | user | no — resolved: neutral house labels (see Non-goals) |

## 2. Design

### Module map (exactly one owner per file; 0 unclaimed)

| Module | Owned globs | Owner | Notes |
| --- | --- | --- | --- |
| editor | `Source/PluginEditor.cpp`, `Source/PluginEditor.h` | agent | LookAndFeel + 4 panels + window chrome |
| untouched | `Source/Modules.h`, `Source/PluginProcessor.*`, `CMakeLists.txt`(formats) | agent | no change in this spec |

### Data and state

No state change. The four panels read the same `prefix + id` parameter pairs as before
(`con_*`, `opto_*`, `fet_*`, `lim_*`). `ModulePanel::faceImage` caches the static face; live meters
(VU needle, LED ladders) are drawn in `paintMeter` on top of it each tick.

### Interfaces and seams

- `RackLookAndFeel` implements `drawRotarySlider`, `drawLinearSlider` (vertical + master bar),
  `drawToggleButton`, `drawButtonBackground`, `drawButtonText`.
- `ModulePanel` gains `paintKnobTicks()` in the panel (not the LnF) so each face can print its own value
  legends around a knob (`Low`, `Hi-Mid`, `0..10` skirts).
- `Slider::getSkewFactor()` is honoured when placing the pointer and the ticks: a linear default range
  (e.g. `-15..+15 dB` with skew 1) is fine, but frequency knobs are logarithmic and the pointer must
  track `slider.valueToProportionOfLength` not the raw normalised value. **This is the one real
  correctness bug to avoid**: use `slider.valueToProportionOfLength(slider.getValue())`.

### Invariants (must not break)

| ID | Invariant | Where (`path:line`) |
| --- | --- | --- |
| INV-1 | every parameter keeps its ID, range and default | `Source/PluginProcessor.cpp:27-60` |
| INV-2 | module order survives drag and is persisted in the state | `Source/PluginEditor.cpp:690-740` |
| INV-3 | meters still read `proc.getGainReduction(id)` | `Source/PluginEditor.cpp:600` |
| INV-4 | editor size stays within the DAW's constraints and the slot grid math matches | `Source/PluginEditor.cpp:20,646` |

### Alternatives considered

- **Images/sprite sheets exported from the reference** — rejected: trademarked artwork, adds binary
  weight, breaks at other scales/DPI.
- **Embed the reference JPEG and slice it** — rejected: same reasons, plus non-free asset.
- **Keep V4 flat style, only recolour** — rejected: does not satisfy REQ-3/REQ-5/REQ-7 (photo-real).

### Verification strategy (how each requirement proves itself)

Build the AU, then render the editor headless through JUCE's `createComponentSnapshot` from a small
harness binary so the exact widget tree is captured without a DAW, and inspect the PNG. Parameter
binding is proven by `auval` plus a programmatic check that all 22 attachments exist.

### Rollback

`Source/PluginEditor.*` is a single-file pair; `git`-less tree, so keep a `.orig` copy in
`~/.config/opencode/temp/zychanstrip/` until the build passes, and reinstall the previous `.component`
from the existing `build/zychanstrip_artefacts` if needed.

### Deploy topology

Build AU to `~/Library/Audio/Plug-Ins/Components/Rack Strip.component` (existing
`COPY_PLUGIN_AFTER_BUILD TRUE`).

### Observability

Headless snapshot PNG + `auval` parameter dump; both written under
`~/.config/opencode/temp/zychanstrip/`.

### Design decisions

| ID | Decision | Covers | Rejected alternative |
| --- | --- | --- | --- |
| DES-1 | Window chrome painted by the editor: chassis gradient + header bar + traffic-light glyphs + right-aligned master bar | REQ-1 | stock JUCE title bar |
| DES-2 | `ModulePanel` paints a metal plate with corner screws + shadow; six-dot grip above the plate reading as part of the chassis | REQ-2 | plain rounded rect |
| DES-3 | Photo-real rotary: outer skirt ring with 11 ticks, body with radial+linear specular gradient, inner cap, black indicator with white tip | REQ-3 | V4 default rotary |
| DES-4 | Four colour-coded knob caps + engraved labels; Comp/Gate LEDs | REQ-4 | monochrome |
| DES-5 | Brushed light plate (speckle + vertical gradient) and a large VU component drawn as a brass dial with glass reflection | REQ-5 | dark plate |
| DES-6 | Dark FET plate, silver caps, backlit ratio bank with radio behaviour, small brass GR VU | REQ-6 | text buttons |
| DES-7 | Twin segmented LED ladders driven by gain reduction, chrome fader thumb with knurling, dB scale legends | REQ-7 | single bar |
| DES-8 | Keep `SliderAttachment` for every control; add `valueToProportionOfLength` for painted pointers | REQ-8 | reimplement parameter handling |
| DES-9 | All painting procedural, cached face image, size-independent | REQ-9 | bitmaps |

## 3. Build

### Build plan

1. Backup `Source/PluginEditor.*`.
2. Rewrite `PluginEditor.cpp`: new palette helpers, `RackLookAndFeel`, `ModulePanel` face/screw/grip
   painting, four panel subclasses with the reference layouts, editor chrome.
3. Build AU, iterate on the snapshot until the four plates match the reference proportions.

### Files and globs

`Source/PluginEditor.cpp`, `Source/PluginEditor.h`.

### Dependency / version pins

JUCE 8.0.4 (unchanged), CMake 4.4.4, Ninja, AppleClang 21.

### Build commands

```sh
export PATH=$HOME/Library/Python/3.13/bin:$PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 8
```

### Deviations from design

- `drawRotarySlider` lives in the LnF and cannot know a panel's custom legends; legends are printed by
  each panel in `paintFaceExtra` around the knob bounds. Recorded, no scope change.

## 4. Test

### Test plan (requirement -> test)

| ID | Test | Covers | Type | Command | Result |
| --- | --- | --- | --- | --- | --- |
| TST-1 | Build exits 0 and produces the AU bundle | REQ-1..REQ-9 | build | `cmake --build` | PASS |
| TST-2 | Headless snapshot renders 4 plates, 16 screws, 4 grips | REQ-2 | UI | `tools/build_snapshot.py` + run | PASS — snapshot.png 1376x768 |
| TST-3 | Every parameter is bound to a control | REQ-8 | unit | source scan | PASS — 28/28 |
| TST-4 | Drag reorder changes order and persists | REQ-8 | UI | manual | not run (needs a host) |

### Known gaps / not verified

- `auval` cannot validate this machine-wide while AudioComponentRegistrar is broken (memory #24826).

## 5. Deploy

### Version and authority

`version authority: user` — no version bump performed; the build keeps `0.1.0` from `project()`.

### Packaging, signing, notarization

Ad-hoc signature (local install only), as today.

### Staging / install

`cmake --build` copies to `~/Library/Audio/Plug-Ins/Components/`.

### Deployment record

| ID | Step | Command | Evidence |
| --- | --- | --- | --- |
| DEP-1 | install AU | `cmake --build build` | copied to ~/Library/Audio/Plug-Ins/Components |

## 6. Maintain

### Known issues

- AU invisible to hosts until the system AudioComponentRegistrar is restarted (memory #24826).

### Feedback loop (incident -> new requirement)

Any layout clipped at 1x or a knob whose pointer disagrees with its value becomes a new REQ here.

## Traceability

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1 | DEP-1 | MNT-1 |
| REQ-2 | DES-2 | TST-1 | DEP-1 | MNT-1 |
| REQ-3 | DES-3 | TST-1 | DEP-1 | MNT-1 |
| REQ-4 | DES-4 | TST-1 | DEP-1 | MNT-1 |
| REQ-5 | DES-5 | TST-1 | DEP-1 | MNT-1 |
| REQ-6 | DES-6 | TST-1 | DEP-1 | MNT-1 |
| REQ-7 | DES-7 | TST-1 | DEP-1 | MNT-1 |
| REQ-8 | DES-8 | TST-3 | DEP-1 | MNT-1 |
| REQ-9 | DES-9 | TST-2 | DEP-1 | MNT-1 |

## Evidence log

| Date | Phase | Command / check | Result |
| --- | --- | --- | --- |
| 2026-10-31 | Requirements | reference pixels sampled | palette + geometry captured |
| 2026-10-31 | Build | `cmake --build build --config Release` | exit 0 |
| 2026-10-31 | Test | `build/snapshot/snapshot` | snapshot.png 1376x768, 4 plates, no caption overlaps |
| 2026-10-31 | Test | parameter binding scan | 28/28 bound |
