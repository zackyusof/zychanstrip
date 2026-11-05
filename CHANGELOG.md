# Changelog

All notable changes to **zychanstrip** are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Version numbers, tags and releases are the maintainer's decision; this file records what changed, not what
has been published.

## [Unreleased]

### Fixed — 1.1.1-beta.5 Enhance core repair (2026-11-05)

- Correct full-processor Auto Gain's half-correction error by measuring uncorrected output and applying matching after every rack module, before master/mix. Keep standalone module matching and ceiling safety.
- Correct physical Threshold direction and show ceiling minus input gain, preserving all existing host parameter IDs/ranges/defaults and stored values.
- Replace NAM's muffling full tape blend with a bounded level-dependent residual from identical captures at x and 0.5x. Common linear EQ/phase cancels; dry remains intact. An extra stereo reference model increases CPU without adding latency.
- Add actual MATCH correction / CEILING headroom status and processor-level threshold, irregular-block, headroom and harmonic regressions. Active 24-order tests now require actual level matching, not just bounded audio.

### Fixed — 1.1.1-beta.4 local evaluation (2026-11-05)

- NAM Enhance now controls a conservative 0–30% blend of the already pushed tape capture at a fixed relative drive. The former simultaneous +12 dB drive/full crossfade caused spectral dips and rebounds as the slider moved.
- Preserve real input gain interaction, all parameter ranges/defaults, ALGO/CLEAN laws, smoothing, safety ceiling and latency. Existing NAM sessions retain slider values with the revised, gentler tone law.
- Add frequency/level progression measurements and moving Enhance tests through every active NAM rack order with Auto Gain on/off.

### Fixed — 1.1.1-beta.3 local evaluation (2026-11-05)

- Ramp Enhance input gain, colour drive/blend, detector ceiling and release over 20 ms (final ceiling safety remains immediate) during slider automation; reset initializes controls immediately.
- Delay the ungained input directly for the limiter Auto Gain fallback, avoiding a mismatched gain division during movement.
- Keep Enhance fader caps, rims and shadows inside their components across the whole travel; retain fractional grip coordinates during dragging.
- Add multi-rate, multi-block forward/reverse slider transition regressions and endpoint geometry/snapshots. Numeric AU version remains 1.1.1.

### Added — 1.1.1-beta.2 local evaluation (2026-11-04)

- Master ALGO / NAM / CLEAN selector on rack and meter headers, append-only automation and old-session migration.
- Four fixed A2-Lite profiles with independent stereo state, fixed level calibration, DC correction and offline resources.
- Prepared polyphase Blackman-sinc conversion around 48 kHz NAM stages, 16-frame adapter, matched bypass/colour delays and 20 ms switching.
- Measured six-rate latency/CPU, all 24 active/bypassed orders, maximum Enhance ceiling, SRC rejection and mode/GUI tests: 297/297 passed.
- Local-only T3K capture setup with SHA256-pinned downloads; third-party notices packaged in AU. No restricted weights/binary in Git.
- Updated README feature inventory, routing/interaction docs, screenshots and current research harness linking.

### Changed

- All modes now report the fixed colour-engine plus limiter delay. At 48 kHz total latency is 135 samples (2.81 ms); 96 kHz is 991 (10.32 ms).
- NAM remains warm in ALGO/CLEAN. CPU at high rates is substantial; profile audition and DAW soak remain beta work.
- Enhance CLEAN removes extra colour drive while retaining limiter input gain/ceiling. Limiter fallback dry reference includes colour delay.
- Earlier GUI work retained: horizontal white Enhance fader stripes and softened LA-2A silhouette/contact shadows with stationary lighting.

### Fixed

- Explicit WaveNet factory linkage prevents the AU static archive from discarding neural model initialization code retained by the headless harness.

- **LA-2A INPUT GAIN / PEAK REDUCTION knobs were drawn in a transformed frame inside an
  already-transformed parent**, so the dome rendered as a crescent at most values and vanished
  entirely at the min/max endpoints. The rack canvas carries its own `canvasTransform`; adding a
  rotation on top made every coordinate ambiguous. The knob is now drawn entirely in absolute
  component coordinates — rotating parts are built as local paths and rotated explicitly about the
  centre — with no `g.addTransform` at all. Verified at 0, 50 and 100.
- **LA-2A INPUT GAIN / PEAK REDUCTION knobs no longer contradict their own engraved scale.**
  The tick marks and 0–100 numerals were drawn on a hardcoded 1.25π–2.75π arc while the knob index
  rotated on the slider's actual −0.75π–0.75π travel, so the pointer sat a tenth of a turn (≈11°) off
  the number it was pointing at. Both now derive from one travel constant (`1.25π`), and the renderer
  receives the slider's real rotary bounds instead of a second private copy of them.
- LA-2A knob numerals were drawn on a ring that the knob body covered at mid travel (only the 80/90
  labels survived). The ring is now outside the ticks and clears the full knob silhouette at every angle.
- LA-2A knob cast shadow used a screen-space offset of +2.3/+3.0 px, so its horizontal component
  vanished at minimum and doubled at maximum — the light direction visibly rotated with the knob. The
  offset is now applied to the already-rotated silhouette, keeping one coherent lower-right light.
- Meter page scale button showed `LINEAR`; the label is `SETTLE`.

### Changed

- **31-band analyser averaging control now works.** `setRtaAveraging()` was never called and the paint
  path ignored `rtaAverage`, so the SETTLE/FAST/MED/AVG indicator was cosmetic. Bands are smoothed on
  the GUI thread at the panel tick rate with coefficients 1.00 / 0.55 / 0.25 / 0.10; the DSP metering
  tap stays read-only and untouched.

### Added

- `docs/dsp/module-console.md`, `module-opto.md`, `module-fet.md` and `interaction.md` — the four pages
  the DSP reference index linked to but which did not exist. Written from source with `file:line`
  citations; derived claims are marked *(inference)*.

## [1.1.1-beta.1] — 2026-11-03

Beta build from the current Gitea `main` delta since `v1.0.1`, as explicitly confirmed by the user.
This is a prerelease only: no stable tag or release is created here.

### Added

- Final-stage stereo-linked UNITY loudness matching after rack, master level and dry/wet mix, with smoothed bounded correction, silence handling, and a live gain readout.
- A documented Hardware Neutral Starting Profile for RESET without changing existing parameter IDs, ranges, or declared defaults.
- Regression coverage for UNITY matching/toggle/state behavior and reset-profile values.

### Changed

- Corrected LA-2A knob rotation direction and added procedural shadowing; redrew 1176 scales and knob shadows procedurally from the supplied inline references.
- Updated signal-flow documentation and parameter count (42).

### Added

- **Installer enforces "build the latest".** `tools/install_global.sh` now refuses to install a
  build that isn't reproducible from a commit: it fails on a dirty tree (tracked changes or
  untracked files under `Source/`/`tools/`), when local `HEAD` is behind `origin/main`, and when
  the artefact is older than the `HEAD` commit — and it prints the commit it installed. Rationale:
  the 2026-11-03 install carried uncommitted limiter work, so no commit produced the running
  binary. `ZYCH_ALLOW_STALE=1` overrides for a deliberate experiment.

- **Version / channel label in the GUI header.** The editor now draws the build's version in a recessed
  pill immediately after the `zychanstrip` wordmark: `v1.1.1` for a stable build, `v1.1.1-beta.1` in
  amber for a prerelease. The string comes from `ZYCHANSTRIP_GUI_VERSION` in `CMakeLists.txt`
  (`PROJECT_VERSION` + optional `-DZYCHANSTRIP_BETA=beta.1`), so the GUI cannot disagree with the
  artefact; `configure` prints it. No parameter, state or DSP change.

### Changed

- **Install wiring is global-only.** `COPY_PLUGIN_AFTER_BUILD` is now `FALSE`, so a build no longer
  copies the AU into `~/Library/Audio/Plug-Ins/Components` (JUCE's default). The single sanctioned
  install path is the new `tools/install_global.sh` / `ninja -C build install-global`, which installs
  to `/Library/Audio/Plug-Ins/Components/zychanstrip.component`, chowns `root:wheel`, verifies the
  installed md5 against the built artefact and deletes any user-domain copy. Two bundles of one
  subtype confuse the AudioComponentRegistrar and a stale user copy shadows a newer global build.
  The installer also re-seals the bundle ad-hoc (`codesign --force --sign -`) because JUCE's
  linker-signed AU fails `codesign -v` on the bundle; it aborts if verification still fails.
  No parameter, state, DSP or GUI change.

- **Project renamed** from RackStrip to ZychanStrip, repo included (`zack/zychanstrip` on Gitea). Code
  identifiers follow: `RackStripProcessor` → `ZychanStripProcessor`, `RackStripEditor` → `ZychanStripEditor`,
  the snapshot harness reports `ZychanStripSnapshot`. No parameter ID, range, default, state schema, bundle
  identifier (`net.zyusof.zychanstrip`), AU subtype (`Zych`) or DSP behaviour changed; the harness still
  reports 246/246.

## [1.0.1] — 2026-11-01

Bug-fix release over `v1.0`. No parameter, state-format or ABI changes; the JUCE 8 → 9 upgrade is
source-compatible for hosts.

### Fixed

- **Dry/wet phase cancellation** (`zychanstrip-mix-phase`). The wet path runs through the rack's look-ahead
  delay while the dry copy was summed raw, so a partial mix comb-filtered and a 1 kHz tone cancelled by
  ~24 dB at 50 %. The dry path now passes through a pre-sized ring sized to `latency + 1` before the blend.
  A 50 % mix with a bypassed rack reproduces the wet signal to **3e-8**. Fully wet never sums the dry path
  and fully dry bypasses the ring, so both extremes are bit-exact against 1.0.0 behaviour.
- **Order-dependent Auto Gain** (`zychanstrip-autogain-order`). The correction integrated energy entirely
  inside the limiter, so it was blind to upstream Console/FET/Opto gain: identical settings gave a **7.6 dB**
  spread depending on rack slot. Auto Gain now divides the **chain input** (measured before any module runs)
  by the **rack output** (after every module, before master gain and mix), published together as the previous
  block's pair so both sides always describe one signal. Measured spread across all **24 rack orders:
  0.000 dB**.
- **Auto Gain no longer chases the master volume.** Using the post-master signal as the output reference
  created a feedback loop: a −6 dB master pulled +2.9 dB of compensation. The reference is now taken before
  master gain, so the correction normalises the rack only.
- **Editor teardown safety** (`zychanstrip-verify-quit-crash`). The meter panel is a member destroyed after the
  editor's destructor body, so its `onBack`/`onReset` callbacks (both capturing `this`) could fire against a
  half-destroyed editor. The destructor now clears those, the page toggle, the reset button and every panel
  drag hook before releasing members.
- **README**: removed a duplicated copy-paste block of RESET bullets.

### Changed

- **Framework: JUCE 8.0.4 → 9.0.4.** The only source change required was
  `Drawable::createFromSVG(const XmlElement&)`, removed in JUCE 9, replaced by
  `Drawable::createFromSVGString` (which returns null on invalid markup, preserving the previous guard).
  Verified unaffected: `Font`, `GlyphArrangement::addLineOfText`, the C++17 module baseline and the
  `juce_add_plugin` arguments.
- **Limiter panel relaid out as a 2×2 grid.** The control area is now THRESHOLD / CEILING on the top row and
  RELEASE / ENHANCE on the bottom; the two vertical LED ladders keep the full panel height beside the grid
  rather than inside it. Captions span their grid cell so long names are no longer clipped.
- **Install policy: one copy, system-wide.** The AU installs to
  `/Library/Audio/Plug-Ins/Components/zychanstrip.component` and the duplicate in `~/Library` is removed,
  closing `zychanstrip-dupe-au`.

### Added

- **16 new harness checks** (230 → **246**), all 230 originals unchanged:
  latency-aligned dry to 3e-8, no cancellation at 50 % mix, fully-dry path still bit-exact, reported
  look-ahead latency, all 24 permutations enumerated, Auto Gain order-independence (0.000 dB), Auto Gain
  correction independent of master level, and the Limiter 2×2 grid geometry with equal row heights at
  864×608, 1080×760 and 1620×1140.
- Specs: `docs/specs/juce-9-upgrade.md` and `docs/specs/zychanstrip-five-fixes.md` (all six lifecycle phases,
  including the release-diff-first classification of every reported symptom).

### Documentation

- New hero screenshot showing the 2×2 limiter grid.
- README documents the latency compensation, the 2×2 limiter layout, the one-copy install policy and the
  new harness baseline. `zychanstrip-mix-phase` and `zychanstrip-autogain-order` are marked fixed;
  `zychanstrip-dupe-au` is closed.

### Notes

- **AU validation is the maintainer's job.** Builds are proven by `codesign -v`, `PlistBuddy` readback and a
  factory `Open`/`Close` load test; `auval`/`pluginval` results are not reported.
- Reported but **not reproduced**: the README shields badges were said to 404. All five `img.shields.io` URLs
  return HTTP 200 with valid SVG and none references Gitea, so no change was made.

## [1.0.0] — 2026-11-01

First tagged release (`v1.0`, commit `abc9872`).

### Added

- Audio Unit (`aufx` / `Zych` / `ZYUS`) hosting four classic outboard modules in a re-orderable rack:
  API 560 console EQ, UREI 1176 FET, Teletronix LA-2A opto and an enhance/limiter stage.
- Drag-and-drop rack re-ordering by the faceplate grip, persisted in the session state.
- Studio-grade metering page: peak/hold, RTA, phase scope, correlation and band energy.
- Parallel dry/wet mix and a host-automatable master level.
- 41 automated parameters (31 explicit plus the ten EQ560 bands), `RESET` to factory state, and
  session-state migration for older sessions.
- Headless snapshot harness (`tools/build_snapshot.py`) rendering the real editor with genuine metering.

### Known issues at 1.0.0

- `zychanstrip-mix-phase` — dry/wet phase error (fixed in 1.0.1).
- `zychanstrip-autogain-order` — order-dependent Auto Gain (fixed in 1.0.1).
- `zychanstrip-verify-quit-crash` — quit-crash hardening unverified against a repro.
- `zychanstrip-rta-averaging` — the RTA averaging control is cosmetic.
- `zychanstrip-vu-ppm` — analog VU/PPM panel not built.
- `zychanstrip-dupe-au` — duplicate AU install (closed in 1.0.1).
