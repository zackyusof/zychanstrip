# Changelog

All notable changes to **zychanstrip** are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Version numbers, tags and releases are the maintainer's decision; this file records what changed, not what
has been published.

## [Unreleased]

### Changed

- **Install wiring is global-only.** `COPY_PLUGIN_AFTER_BUILD` is now `FALSE`, so a build no longer
  copies the AU into `~/Library/Audio/Plug-Ins/Components` (JUCE's default). The single sanctioned
  install path is the new `tools/install_global.sh` / `ninja -C build install-global`, which installs
  to `/Library/Audio/Plug-Ins/Components/zychanstrip.component`, chowns `root:wheel`, verifies the
  installed md5 against the built artefact and deletes any user-domain copy. Two bundles of one
  subtype confuse the AudioComponentRegistrar and a stale user copy shadows a newer global build.
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
