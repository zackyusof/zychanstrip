<p align="center">
  <img src="docs/screenshots/zychanstrip-rack.png" alt="zychanstrip — Mastering & Tracking Rack" width="100%">
</p>

# zychanstrip

### The Ultimate Analog Mastering & Tracking Rack for macOS

[![Version](https://img.shields.io/badge/Version-1.1.1--beta.2-orange.svg)](#build-and-install)
[![Audio Unit](https://img.shields.io/badge/Format-AUv2%20(Audio%20Unit)-blue.svg)](#build-and-install)
[![Architecture](https://img.shields.io/badge/Architecture-Apple%20Silicon%20(arm64)-orange.svg)](#build-and-install)
[![Framework](https://img.shields.io/badge/Built%20with-JUCE%209-green.svg)](#architecture)
[![Zero Bitmaps](https://img.shields.io/badge/GUI-100%25%20Procedural%20Vector-purple.svg)](#architecture)

**zychanstrip** is four classic outboard processors in one rack plus a Studio-Grade metering page, delivered as a high-performance **Audio Unit (AU)** effect.

Two pages live side by side, switched by the **RACK / METER** toggle in the header:

- **RACK** — four re-orderable hardware faceplates: API 560 graphic EQ, UREI 1176 FET compressor, Teletronix LA-2A opto leveler, and an Enhance/master limiter with parallel dry/wet.
- **METER** — an SSL-style analyser page: dual true-peak/RMS meters, Lissajous phase scope, correlation bar and a 31-band real-time analyser.

---

## Current beta: colour engines and complete feature overview

**1.1.1-beta.2 is a local evaluation beta.** The embedded API/IGS captures require creator
permission before distributing the binary. Source and documentation can be shared; model
weights are downloaded into ignored build storage. See [profile attribution](THIRD_PARTY_NOTICES.md).

- Four drag-reorderable modules: 10-band graphic EQ, fast FET compression, opto levelling,
  and Enhance with look-ahead brick-wall limiting, ceiling, release and linked auto gain.
- One master **COLOUR — ALGO / NAM / CLEAN** selector, visible on RACK and METER pages,
  host-automatable and saved with sessions. ALGO preserves existing saturation; NAM uses
  fixed embedded hardware captures; CLEAN removes added colour while retaining EQ, gain,
  compression and limiting. Twenty-millisecond mode crossfades; constant mode/bypass latency.
- Four author-supplied A2-Lite profiles: API 512c gain 3, IGS ONE LA with zero gain reduction,
  UREI 1176 Rev A clean colour, and Studer A807 tape. No profile loader, login or runtime
  network connection. Fixed calibration preserves gain interaction between modules.
- Streaming sample-rate conversion around each NAM stage, to/from its native 48 kHz;
  EQ, compressor envelopes and limiter keep the DAW sample rate. Native 48 kHz skips SRC.
  ALGO and CLEAN have matching delays; NAM remains warm in every mode in this beta.
- Master output level, parallel dry/wet mix, UNITY level matching and one-click RESET to the
  Hardware Neutral Starting Profile. Individual module bypass and persistent rack order.
- Meter page with true-peak/RMS, loudness, stereo phase/Lissajous scope, correlation and
  31-band spectrum with RTA averaging; rack VU/GR and Enhance GR/output LED meters.
- Resizable 864×608 to 1620×1140 interface, metal faceplates, drag handles, horizontal
  Enhance fader index stripes, softened LA-2A contact/cast shadows and fixed lighting.
  Mouse wheel, Shift fine adjustment and double-click parameter defaults.
- Apple Silicon macOS AUv2 effect, JUCE 9; global-only install with source/hash/signature
  verification. Existing parameter IDs/ranges/defaults are preserved; old sessions use ALGO.

For routing, actual latency/CPU measurements and validation limits see
[NAM colour DSP](docs/dsp/nam-colour.md) and [implementation specification](docs/specs/embedded-nam-colour.md).
This beta has not been auditioned by a human and is not a stable release.

## Contents

- [Quick start for a new agent](#quick-start-for-a-new-agent)
- [Architecture](#architecture)
- [The Four Modules](#the-four-modules)
- [The Metering Page](#the-metering-page)
- [Signal Flow, Latency and Phase](#signal-flow-latency-and-phase)
- [Parameters](#parameters)
- [Build and Install](#build-and-install)
- [Development & Verification Tools](#development--verification-tools)
- [Known issues / next work](#known-issues--next-work)
- [Conventions & traps](#conventions--traps)
- [Changelog](#changelog)

---

## Quick start for a new agent

Read this before touching code. It is the fastest route from a fresh clone to a working build.

```bash
git clone https://git.zyusof.net/zack/zychanstrip.git
cd zychanstrip

# CMake and Ninja may be installed by pip on macOS, not in PATH by default:
export PATH="$HOME/Library/Python/3.13/bin:$PATH"

python3 tools/nam/prepare_profiles.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZYCHANSTRIP_BETA=beta.2
ninja -C build                       # builds only; install globally with tools/install_global.sh
```

Verify:

```bash
python3 tools/build_snapshot.py      # builds the headless GUI/DSP harness (reuses Ninja flags)
./build/snapshot/snapshot /tmp/shots # runs all tests, writes PNGs
tools/check_au_bundle.sh "$PWD/build/zychanstrip_artefacts/Release/AU/zychanstrip.component/Contents/MacOS/zychanstrip" # packaged factory/render test
# expect: RESULT <current total>/<current total> passed
```

**Where the code lives**

| Concern | File(s) |
| --- | --- |
| DSP for all four modules | `Source/Modules.h` |
| API 560 EQ core (MIT, adapted) | `Source/Eq560.h` |
| Processor, parameter tree, dry/wet mix, metering tap | `Source/PluginProcessor.{h,cpp}` |
| Metering measurement (FFT, peaks, correlation, LUFS) | `Source/Meters.h` |
| Meter page drawing/behaviour | `Source/MeterPanel.{h,cpp}` |
| Rack UI, panels, LookAndFeel, page switch | `Source/PluginEditor.{h,cpp}` |
| Headless test + snapshot harness | `tools/snapshot_main.cpp`, `tools/dsp_checks.h`, `tools/build_snapshot.py` |

The NAM dependency is fetched at a pinned revision with its Eigen submodule. Profile setup
requires network once; plugin use is offline. T3K profiles are for a local evaluation build;
obtain creator permission before publishing any binary containing them.

**Before you change anything**

1. `git pull` — work from the latest `main`, not a stale copy.
2. Run the harness first and confirm the current documented result.
3. After ANY change: `ninja -C build` then re-run the harness.
4. Commit and push. Install globally only from the clean latest commit using the install policy below.

---

## Architecture

**Plugin shell.** `ZychanStripProcessor` is an `AudioProcessor` with a fixed four-slot rack. It is not a synth, needs no MIDI, and presents AU type `aufx`, subtype `Zych`, manufacturer `ZYUS` (`net.zyusof.zychanstrip`).

**Module model.** Every module derives from `rack::Module` (`Source/Modules.h`) and exposes `process(AudioBuffer&)`, plus `getLatencySamples()`, an atomic `grDb` for its meter, and (limiter only) `outPeak` / `gainCompDb`. Modules are processed **in rack order**, which is stored as a single packed `uint32_t` that the audio thread reads once per block — re-ordering can never tear mid-block.

```cpp
enum ModuleId { Console = 0, Opto = 1, Fet = 2, Limiter = 3, NumModules = 4 };
// slot i holds module id byte i of packedOrder; default 0x03010200 => [0,2,1,3]
```

**Parameters.** A single `AudioProcessorValueTreeState` with **43** parameters (see [Parameters](#parameters)). Adding one is a one-line `addF`/`addB` call in `createLayout()`; **never reorder or re-range an existing ID** — host automation and saved sessions depend on them. New parameters must be appended at the end.

**State migration.** `setStateInformation` patches older session blobs by inserting any parameter that is missing (e.g. `lim_auto_gain`, the EQ560 bands, `master_mix`). If you add a parameter, add it to that migration list too, or old sessions will silently miss it.

**Editor.** `ZychanStripEditor` holds a `canvas` component that draws the chassis and hosts the four `ModulePanel` children plus the header controls. A second child, `MeterPanel`, fills the window when the METER page is selected.

---

## The Four Modules

### 1. API 560 10-band graphic EQ (`Console`, prefix `con_`)
Proportional-Q bands at 16k, 8k, 4k, 2k, 1k, 500, 250, 125, 63, 31 Hz, ±12 dB, with 2× oversampling and 2520-style op-amp / 2503-transformer saturation. Ten extra legacy 4-band controls remain registered for automation compatibility but are inactive.

### 2. UREI 1176 LN FET compressor (`Fet`, prefix `fet_`)
Input/output/attack/release plus a 4:1 / 8:1 / 12:1 / 20:1 / ALL ratio bank. Fast attack, fixed threshold driven by input gain, Class-A FET colour.

### 3. Teletronix LA-2A opto leveler (`Opto`, prefix `opto_`)
Gain, peak reduction, and a compress/limit `mode` switch. Program-dependent release, 12AX7/12BH7 tube colour.

The Gain and Peak Reduction knobs use the Davies 1470/Daka-Ware face, with a correctly aligned 0–100 numbered scale and a cast shadow that follows the control.

### 4. Enhance / master limiter (`Limiter`, prefix `lim_`)
Look-ahead brickwall limiter with tape/console saturation (`lim_enhance`), and a stereo-linked auto-gain that matches its own output level back to its input so you hear tone changes rather than loudness shifts.

Its four fader channels (threshold, ceiling, release, enhance) sit inside one shared, meter-style recessed faceplate around the full 2×2 bank. The plate has a dark metallic bezel, black inset, and opposing light/shadow bevels; individual channels retain their narrow recessed slots (`LimiterPanel::paintFaceBase`, `RackLookAndFeel::drawChromeFader`).

**Default rack order is 560 → 1176 → LA-2A → Enhance** (`[0,2,1,3]`). Any saved session keeps its own order — the default only applies to fresh instances.

### UNITY and RESET
`UNITY` sits beside `RESET` in the header. When enabled, it stereo-links a smoothed short-term RMS matcher at the very end of the signal path, after the rack, master level and dry/wet blend. It matches finished output loudness to host input while limiting correction to ±18 dB, holding through near-silence, and smoothly ramping gain. The readout shows the current correction. This is level matching, not sample-accurate nulling; nonlinear processing and latency remain audible.

`RESET` in the header (and `RESET` on the metering page) restores the documented **Hardware Neutral Starting Profile** in one action:

* broad EQs and colour controls return to neutral, the API threshold/ratio/attack/release return to 0 dB / 3:1 / 10 ms / 200 ms, LA-2A peak reduction is at minimum and gain at 50% (an approximate unity-gain starting point), 1176 input is at 12 dB with output at 0 dB and attack/release at 3/4, while bypasses, limiter, master and dry/wet return to the profile’s neutral/start values;
* the rack order returns to `[0,2,1,3]` and the faceplates animate back to those slots;
* all metering (peaks, holds, RTA, scope, correlation) is cleared.

Both RESET pills, and the metering page `BACK`, share one look (`Source/UiStyle.h`): the RACK/METER teal gradient at rest and matte black while held, so the momentary action visibly bottoms out.

Parameter values are written on the message thread with real gestures, so host automation follows the reset. Module DSP state (EQ biquads, compressor envelopes, limiter delay and energy rings) is cleared at the start of the next audio block via a single atomic request, so the reset is real-time safe and leaves no stale look-ahead tail. `ZychanStripProcessor::resetToDefaults()` is the one implementation; both buttons call it through `ZychanStripEditor::resetRackToDefaults()`.

The reset profile is deliberately separate from each parameter's host-declared default: existing IDs, ranges and declared defaults are unchanged for saved-session and automation compatibility. 1176 input/output marks are source-dependent calibration points, and LA-2A Gain has no universal exact setting, so these are operational starting values rather than claims of exact hardware unity.

### Parallel dry/wet
`master_mix` (0–100 %, default 100) blends the untouched input against the finished master output for parallel processing. It is applied **after** master volume.

The dry copy is **latency-compensated**: it passes through a ring sized to `getLatencySamples() + 1` before
the blend, so partial mixes sum in phase instead of comb-filtering. At 100 % wet the dry path is not summed
at all, and at 0 % it bypasses the ring, so both extremes are bit-exact against the pre-fix behaviour.

---

## The Metering Page

Selected with the header **RACK / METER** toggle; the METER page has its own **`< RACK`** button to return, so the way back is always present.

**It is a pure analyser.** It observes the signal after UNITY and never modifies audio. All numbers come from `RackMeters` (`Source/Meters.h`), which publishes measurements through atomics from the audio thread and is read by the UI at 30 Hz. Nothing on this page is simulated.

| Measurement | Implementation |
| --- | --- |
| Peak | per-channel sample peak per block |
| Max-hold | 2 s hold then slow release |
| RMS | ~300 ms smoothed block RMS |
| Short-term loudness | K-weighted (2nd-order HP ~38 Hz + shelf) over a 400 ms window, shown in LUFS |
| Correlation | block-wise L/R correlation, −1…+1 |
| Phase scope | decimated L/R pairs rotated 45° into a Lissajous |
| 31-band analyser | 2048-point FFT, Hann window, folded into 31 log-spaced bands with per-band peak-hold |

**Hooks:** `ZychanStripEditor::timerCallback()` calls `MeterPanel::tick()` only while the meter page is visible; `ZychanStripEditor::showPage(bool)` swaps pages.

---

## Signal Flow, Latency and Phase

```
Host input
   │
   ├── copy to dryBuffer (untouched, for parallel mix)
   │
   ▼
slot 0 → slot 1 → slot 2 → slot 3     (any permutation, atomic per block)
   │
   ▼
master gain  (master_level, −60…+6 dB)
   │
   ▼
dry/wet blend (master_mix)
    │
    ▼
UNITY level match (optional, stereo-linked)
    │
    ▼
metering tap (read-only)
   │
   ▼
Host output
```

**Latency (beta.2).** Each colour stage reports its fixed neural/SRC delay in every mode,
including CLEAN and bypass. The rack adds those four delays to the limiter's `ceil(0.0015·sr)-1`
look-ahead. At 48 kHz the total is **135 samples / 2.81 ms**. Other rates use longer FIR
conversions; see the measured table in [NAM colour DSP](docs/dsp/nam-colour.md). This supersedes
the earlier prototype latency estimates. The host is notified once during preparation.

**Phase.** The partial dry/wet path is delayed by the reported rack latency. NAM captures have
their own frequency-dependent phase response, which is intentional colour; latency compensation
cannot remove that. At 0% wet, the existing untouched-input/no-added-delay behaviour is retained.

---

## Parameters

43 automated parameters. `bypass` is `false` = module active.

| Module | Prefix | Parameters |
| --- | --- | --- |
| Colour | `colour_` | `mode`: ALGO / NAM / CLEAN |
| Console | `con_` | `bypass`, `eq_16k`, `eq_8k`, `eq_4k`, `eq_2k`, `eq_1k`, `eq_500`, `eq_250`, `eq_125`, `eq_63`, `eq_31`, `hpf`, `lf`, `lmf_f`, `lmf`, `hmf_f`, `hmf`, `hf`, `thresh`, `ratio`, `attack`, `release`, `out` |
| Opto | `opto_` | `bypass`, `gain`, `peak`, `mode` |
| FET | `fet_` | `bypass`, `input`, `output`, `attack`, `release`, `ratio` |
| Limiter | `lim_` | `bypass`, `gain`, `ceiling`, `release`, `enhance`, `auto_gain` |
| Master | — | `master_level`, `master_mix`, `unity` |

**Interaction.** Every knob, fader and slider supports mouse-wheel scrolling; hold ⇧ for fine steps; double-click restores the host-declared default. Header RESET instead restores the documented Hardware Neutral Starting Profile.

---

## Build and Install

**Requirements (verified on this machine):** macOS with Xcode Command Line Tools (`xcode-select --install`), CMake ≥ 3.22, Ninja, Apple Silicon (arm64 target is forced in `CMakeLists.txt`). JUCE 9.0.4 is fetched automatically by `FetchContent` into `build/_deps/juce-src` on first configure.

```bash
export PATH="$HOME/Library/Python/3.13/bin:$PATH"   # pip-installed cmake/ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

`COPY_PLUGIN_AFTER_BUILD` is **off**: a build installs nothing anywhere. The bundle is left in

```
build/zychanstrip_artefacts/Release/AU/zychanstrip.component
```

**Install policy (2026-11-03, user directive): GLOBAL ONLY — never user space.**
One command installs system-wide, chowns `root:wheel`, verifies the installed md5 against the
built one, and removes any `~/Library` copy:

```bash
tools/install_global.sh          # or: ninja -C build install-global
```

`tools/install_global.sh` is the only sanctioned install path. It refuses to source from or write
to `~/Library/Audio/Plug-Ins`, and fails if more than one copy exists afterwards. Two bundles of
the same subtype confuse the AudioComponentRegistrar, and a stale user copy silently shadows a
newer global build — which is exactly how `zychanstrip-dupe-au` came back once.

It also **re-seals the bundle ad-hoc after copying**: JUCE links the AU with a linker-signed
signature whose stale `Contents/_CodeSignature` makes `codesign -v` fail on the bundle
(`code has no resources but signature indicates they must be present`). The installer runs
`codesign --force --sign -` and refuses to finish unless `codesign -v` then passes.

**Build the latest, always.** The installer refuses an install that isn't reproducible from a
commit — it fails if tracked source is modified, if local `HEAD` is behind `origin/main`, or if
the artefact is older than the `HEAD` commit, and it prints the commit it installed. So the
sequence is always: **commit → `ninja -C build` → `tools/install_global.sh`**. An AU built from a
dirty tree has no commit that reproduces it, which is exactly what happened on 2026-11-03 when an
installed build carried uncommitted limiter work. `ZYCH_ALLOW_STALE=1 tools/install_global.sh`
overrides the guard for a deliberate experiment and prints a warning.

```bash
# what the script does, in full
sudo /usr/bin/ditto build/zychanstrip_artefacts/Release/AU/zychanstrip.component \
     /Library/Audio/Plug-Ins/Components/zychanstrip.component
sudo chown -R root:wheel /Library/Audio/Plug-Ins/Components/zychanstrip.component
rm -rf ~/Library/Audio/Plug-Ins/Components/zychanstrip.component
```

Never install to `~/Library/Audio/Plug-Ins/Components/` — not "just to test", not "both locations
work". A build only lands in `build/`; installing means `tools/install_global.sh`.

**Validation is the user's job.** A build is done when the AU is installed and *proven loadable*:

- the bundle is in place, owned `root:wheel`;
- `codesign -v` passes;
- `PlistBuddy` shows `aufx` / `Zych` / `ZYUS` and the expected version;
- the component constructs through its own `zychanstripAUFactory` (`Open`/`Close` return 0).

Do **not** run `auval` or `pluginval` and do not report their result — the user validates in their host.
On this Mac `auval` cannot validate any third-party AU anyway: it lists Apple's 30 and none of the several
hundred third-party components installed here, so a missing entry says nothing about this build.

**Build directory gotcha.** `build/` is configured against an absolute source path. If you move or rename the repo, `ninja` fails with `rebuilding 'build.ninja': subcommand failed`. Fix: `rm -rf build` and re-run `cmake -S . -B build ...` from the new path.

## Version and channel label

The header shows the version immediately after the `zychanstrip` wordmark — the build's only
version display, taken from CMake so it cannot drift from the artefact:

| Build | Label | Treatment |
| --- | --- | --- |
| stable | `v1.1.1` | neutral grey |
| prerelease | `v1.1.1-beta.2` | amber |

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZYCHANSTRIP_BETA=beta.2  # v1.1.1-beta.2
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZYCHANSTRIP_BETA=beta.2   # …-beta.2
```

`PROJECT_VERSION` in `CMakeLists.txt` stays numeric (the bundle plist needs it); the prerelease
suffix lives only in the `ZYCHANSTRIP_BETA` cache string, and `configure` prints the resulting label.
The amber treatment means a beta screenshot can never be mistaken for a release.


---

## Development & Verification Tools

### Headless harness

`tools/build_snapshot.py` compiles `tools/snapshot_main.cpp` against the same objects Ninja produced, so it exercises exactly the shipped code — no separate build to drift out of sync.

```bash
python3 tools/build_snapshot.py
./build/snapshot/snapshot <output-dir>
```

It runs **259 checks** and writes PNGs to `<output-dir>`:

- **DSP** (`tools/dsp_checks.h`): EQ band response, limiter behaviour, auto-gain, channel handling, master gain ramp, state roundtrip, legacy-session migration, dry/wet mix law.
- **GUI**: page toggle clicks (real `mouseDown`, not a test seam), BACK from the meter page, control containment inside every panel at 864×608 / 1080×760 / 1620×1140, reorder by grip drag, aspect-ratio lock.
- **Snapshots**: `rack-*.png` and `meter-*.png`; the meter ones are rendered with real audio pushed through the chain so the meters, scope and analyser show genuine values.

**When you add a feature, add a check.** A test that only asserts "it did something" is how real bugs ship: the dry/wet phase inversion and the auto-gain order dependence both passed the suite as originally written.

### Limiter 2×2 fader bank

The Enhance/Limiter panel (slot 3) splits its content into a **control area** (left) and a **metering area**
(right). The control area is a 2×2 grid:

| | left | right |
| --- | --- | --- |
| **top row** | THRESHOLD (`lim_gain`) | CEILING (`lim_ceiling`) |
| **bottom row** | RELEASE (`lim_release`) | ENHANCE (`lim_enhance`) |

The two vertical LED ladders (GR, OUT) keep the full panel height beside the grid, never inside it.
Captions span the whole grid cell, so long names are not clipped. The harness asserts the grid geometry at
864×608, 1080×760 and 1620×1140.

### Teardown check (important)

The harness does **not** exercise editor destruction, which is where crashes live. Use this pattern manually when touching UI lifecycle:

```cpp
auto* ed = static_cast<ZychanStripEditor*>(proc.createEditor());
ed->setSize (1080, 760);
delete ed;                    // any slider/attachment ordering bug fires here
```

---

## Known issues / next work

| ID | Issue | Detail |
| --- | --- | --- |
| `zychanstrip-enhance-slider-steps` | **Open — Enhance automation has abrupt transitions** | Beta.2 reads gain, Enhance and ceiling once per block without a parameter ramp. A temporary production-DSP probe at 48 kHz / 128 frames, Auto Gain off, constant 0.2 input measured a 0.199052 single-sample jump for gain 0→6 dB and Enhance 0→50% jumps of 0.193975 (ALGO) / 0.108966 (NAM). These synthetic step tests establish discontinuities, not a human listening verdict. Existing 297 checks pass and do not cover these gestures. |
| `zychanstrip-enhance-cap-clipping` | **Open — Enhance fader caps clip** | `drawChromeFader` paints 30 px wide caps inside 26 px wide slider components, clipping the rim/shadow. The 46 px cap height also extends beyond the component near travel endpoints. Visible in the beta.2 rack snapshot; live drag/repaint behaviour still needs host confirmation. |
| `zychanstrip-au-discovery` | **System discovery unresolved on this host** | `auval` cannot find this component after a registrar rescan. The direct packaged-AU test passes open, initialize and real rendering in all three colour modes at 44.1/48/96/192 kHz. DAW discovery/rescan remains to be confirmed. |
| `zychanstrip-nam-rights` | **Local evaluation only** | API/IGS profiles use T3K. Do not distribute model files or this beta binary without creator permission. Source/manifest are shareable. |
| `zychanstrip-nam-validation` | **Beta validation limits** | No human listening, hardware null reference, live DAW automation soak, or audio-thread allocation instrumentation yet. NAM stays warm in all modes; CPU is not reduced by CLEAN/ALGO. Longer SRC latency outside 48 kHz is deliberate. |
| `zychanstrip-mix-phase` | ~~**Dry/wet has a phase error**~~ **FIXED** | The dry copy is now delayed by `getLatencySamples()` through a pre-sized ring before the blend. 50 % mix with a bypassed rack reproduces the wet signal to 3e-8; the old code cancelled a 1 kHz tone by ~24 dB. Harness: *"50% mix sums latency-aligned dry"*. |
| `zychanstrip-autogain-order` | ~~**Auto-gain is order-dependent**~~ **FIXED** | Auto Gain now divides the energy of the **chain input** (measured before any module) by the **rack output** (after every module, before master gain), both captured as one pair from the previous block. Measured spread across all 24 permutations: **0.000 dB** (was 7.6 dB). Harness: *"Auto Gain is order-independent across 24 permutations"*. |
| `zychanstrip-dupe-au` | ~~Duplicate AU install~~ **CLOSED 2026-11-01, re-closed 2026-11-03** | Policy is one copy, **global only** (`/Library/Audio/Plug-Ins/Components`). The user copy reappeared because a spec's deploy step copied there; builds no longer install anywhere (`COPY_PLUGIN_AFTER_BUILD FALSE`) and `tools/install_global.sh` removes any user-space copy and fails if two copies exist. |

Closed work is recorded with evidence; anything unverified stays open deliberately.

---

## Conventions & traps

1. **Latest release is the base.** Branch from the newest tag/`main`, never a stale local copy.
2. **Never change existing parameter IDs, ranges or defaults.** Append only. Preserve the state migration path.
3. **Editor lifecycle.** Declare `Slider`s **before** the `SliderParameterAttachment`s that reference them — members are destroyed in reverse, and the wrong order calls `removeListener()` on freed memory (`EXC_BAD_ACCESS`, fixed in `ef1aa29`).
4. **Component naming.** Setting a name in a constructor can be lost when the component is added; set it at registration if anything looks it up by name.
5. **Click handling.** The rack `canvas` covers the whole editor and consumes clicks; editor-level `mouseDown` will not fire for anything drawn into the canvas. Add a child component instead (as the page toggle does).
6. **Draw order.** `ModulePanel::paint` draws the face image, then `paintMeter`, then composites the caption layer on top. Anything that must sit *behind* a live meter belongs in the face image (`paintFaceBase`), not the caption layer.
7. **Repaint hygiene.** Meter ticks run at 30 Hz; only repaint while `isShowing()`. A timer firing during teardown can post into a dying message queue.
8. **Bitmaps are banned.** All graphics are procedural `juce::Graphics` calls so the UI stays sharp at any scale (864×608 → 1620×1140, 27:19 locked).

### Repo conventions
- Remote: `https://git.zyusof.net/zack/zychanstrip` (Gitea). `main` is the trunk.
- Commit and push every change; verify with `git status` and `git log --oneline origin/main..HEAD` (empty = pushed).
- Keep the harness green before pushing.

---

## Changelog

### 2026-11-04 — 1.1.1-beta.2 local NAM evaluation

- Added global ALGO / NAM / CLEAN colour selection, automation, session migration and smooth transitions.
- Embedded four pinned A2-Lite hardware colour profiles with fixed calibration and DC correction.
- Added streaming sinc sample-rate conversion, SIMD FIR processing, matched bypass/dry paths and fixed latency.
- Preserved existing EQ/compressor/limiter controls; CLEAN removes extra colour and Enhance drive.
- Retained the horizontal Enhance cap stripes and improved LA-2A shadows/lighting from today's earlier commits.
- Updated profile attribution, routing, measured latency/CPU, tests, build helpers and the rack screenshot.
- Local evaluation binary only: API/IGS redistribution rights and human listening validation remain open.


### 2026-11-03 — Limiter fader bank bezel

- Added one shared meter-style recessed frame around the limiter's complete 2×2 fader bank; individual faders remain unframed within it.
- Preserved all controls, parameter IDs, ranges, defaults, and DSP behavior. Build/install evidence is recorded in the task report.

Every notable change since the `v1.0` tag — framework upgrades, DSP fixes, UI changes, harness growth
and the reasons behind them — is recorded in [CHANGELOG.md](CHANGELOG.md), following Keep a Changelog.
Current beta target: **1.1.1-beta.2** (user-confirmed). No stable release is authorized; promotion remains the user's decision.
