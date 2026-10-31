# zychanstrip

A four-module channel strip for macOS as an **Audio Unit (AU) effect**, built with JUCE 8 and designed
to feel like four pieces of outboard gear bolted into one rack. Every module is a separate face plate
with its own character, and the plates can be dragged into any order — the signal chain re-wires to
match, and the order is saved with your session.

The interface is a photo-real rendering of that rack: brushed and anodised metal faces, bevelled
screws, machined knobs with printed scales, a brass VU movement and segmented LED ladders. There are
no bitmap assets — everything is drawn procedurally, so the panel stays sharp at any scale.
The interface defaults to its smallest compact size (**864 × 608**) and resizes up to **1620 × 1140** while
keeping its fixed 27:19 aspect ratio; drag the lower-right corner to resize it.

---

## Contents

- [The four modules & analog color](#the-four-modules--analog-color)
- [Signal flow and audio routing](#signal-flow-and-audio-routing)
- [Latency and phase](#latency-and-phase)
- [Channel handling](#channel-handling)
- [Re-ordering the chain](#re-ordering-the-chain)
- [Parameters](#parameters)
- [Build and install](#build-and-install)
- [Development tools](#development-tools)
- [Design notes and limits](#design-notes-and-limits)

---

## The four modules & analog color

Each module features an authentic analog color and saturation model capturing the non-linearities,
transformer iron, tube warmth, and op-amp drive of classic hardware:

### 1. Dedicated API 560 Graphic EQ

The first plate is an authentic hardware-styled recreation of the classic ten-band graphic EQ:
full-height vertical fader travel, brushed dark faceplate, authentic API twin op-amp chevron emblem,
mounting screw cutouts, and a hardware **IN** toggle switch.

**Analog Color Model:**
- **API 2520 Discrete Op-Amp & 2503 Transformer**: Models the discrete op-amp's punchy, forward midrange
  with subtle second-harmonic asymmetry and third-harmonic transformer iron core saturation (`apiConsoleColor`),
  producing the signature American console drive when pushed.

**EQ Bands**

| Band | Frequency | Range | Default | Notes |
| --- | --- | --- | --- | --- |
| Band 1–10 | 16k, 8k, 4k, 2k, 1k, 500, 250, 125, 63, 31 Hz | ±12 dB each | 0.0 dB | Ten fixed proportional-Q bands; red cap on 1 kHz |
| IN Switch | – | on / off | on | Hardware bypass switch for the 560 module |

The EQ core is a C++ adaptation of [chokehold’s EQ 560](https://github.com/chkhld/jsfx/blob/f6d175d8253375a51832fd52aa86482c191caadc/Equalizer/eq_560.jsfx),
from `chkhld/jsfx` (MIT license). It ports the state-variable peak filters, proportional-Q curve (bandwidth
narrows as boost/cut increases) and 2× Butterworth oversampling. Attribution and license are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
and the installed AU resources.

*Note on legacy dynamics parameters:* The original channel strip dynamics controls (`con_thresh`, `con_ratio`,
`con_attack`, `con_release`, `con_out`, `con_hpf`) and four-band parameters (`con_lf`, etc.) remain registered in the host
parameter tree at their original indices so existing host sessions and DAW automations do not break. On the current
faceplate, the module is dedicated purely to the API 560 graphic EQ.

### 2. Opto leveler

An optical-style leveller with a program-dependent, two-stage release — designed to breathe rather than
pump.

**Analog Color Model:**
- **12AX7 / 12BH7 Triode Tube & UTC Transformer**: Models the warm, rich even-order harmonic distortion
  (predominantly 2nd harmonic) and gentle musical saturation of the classic Teletronix LA-2A tube circuit
  (`optoTubeColor`), giving vocals and bass full, thick analog body.

| Control | Range | Default | Notes |
| --- | --- | --- | --- |
| Gain | 0 – 100 | 0.0 | Make-up, up to +30 dB |
| Peak Reduction | 0 – 100 | 0.0 | Drives a threshold from 0 down to −40 dB |
| ON (LIMIT) | on / off | off | **Compress (3:1) ⇄ Limit (12:1)** — a real parameter (`opto_mode`) |
| Meter | – | on | Faceplate lamp only; this unit always reads reduction |

Compression is roughly 3:1 (or 12:1 with **ON** engaged) with a 10 dB soft knee, so the same Peak
Reduction setting produces far more control in limit position. The large VU reads **reduction** in dB
(0–20) and is driven by the same value the processor publishes to the host.
Release blends a fast (~60 ms) and a slow (~1.8 s) time constant, so short transients recover quickly
while sustained level is held — the behaviour that gives optical compressors their feel.

### 3. FET compressor

A fast FET-style compressor: **input drives a fixed threshold**, which is how these units are actually
used — you push the input to get more compression and pull the output back to compensate.

**Analog Color Model:**
- **Class-A FET & 11754 Transformer**: Models the aggressive odd-order harmonics (predominantly 3rd harmonic)
  and fast transient bite of the 1176 LN revision (`fetColor`), providing punch, snap, and grit when driven hard.

| Control | Range | Default | Notes |
| --- | --- | --- | --- |
| Input | 0 – 30 dB | 0.0 dB | Drives the input stage into a fixed −10 dB threshold |
| Output | −20 – +20 dB | 0.0 dB | Output makeup |
| Attack | 1 – 7 | 3.0 | Mapped to 0.8 ms (1) … 0.02 ms (7) — higher is faster |
| Release | 1 – 7 | 4.0 | Mapped to 1100 ms (1) … 50 ms (7) |
| Ratio | 4 / 8 / 12 / 20 / 30 | 4:1 | Backlit button bank (labelled `4:1 … ALL`) |

Attack and release are deliberately inverted against their panel numbers to match the hardware
convention (a higher number is a faster position). The small backlit dial is a gain-reduction meter.

### 4. Master limiter

A look-ahead brick-wall limiter, intended as the last stage.

**Analog Color Model:**
- **Master Console & Analog Tape Saturation**: Blends analog tape odd-harmonic compression and console bus
  warmth with the Enhance drive circuit (`analogMasterSaturation`), avoiding harsh digital clipping while
  maintaining precise peak ceilings.

| Control | Range | Default | Notes |
| --- | --- | --- | --- |
| Gain | 0 – 18 dB | 0.0 dB | Drives the limiter |
| Ceiling | −12 – 0 dB | −0.3 dB | Absolute output ceiling |
| Release | 10 – 1000 ms | 100.0 ms | Limiter recovery |
| Enhance | 0 – 100 | 0.0 | Adds up to 12 dB of drive plus harmonic tape blend |
| Auto Gain | on/off | on | Compensates this module’s input/output RMS level |

The limiter uses a sliding minimum over the look-ahead window followed by a moving average.
Enhance adds drive and saturation before limiting; the final output is held to the selected peak ceiling.

**Auto Gain** compares the Enhance module’s latency-aligned input and processed output using linked
stereo RMS estimates, then applies smoothed compensation. It compensates changes within this module,
including Enhance and drive; it does not compensate earlier EQ/compressor stages or the manual Master.
It is enabled in new instances and can be switched off. Older saved sessions restore with Auto Gain off.
Compensation is bounded to ±36 dB, with 200 ms RMS estimation and 100 ms correction smoothing.

**Meters & Idle Blanking:**
The two tall ladders show gain reduction (amber, Ladder A) and true audio output level (green through cyan, Ladder B).
The meters feature active idle blanking: when no audio is passing through or when the module is not processing,
both ladders completely extinguish (0 LEDs lit) and the reduction readout blanks, keeping the rack quiet
and uncluttered during pauses. When limiting, Ladder A tracks reduction and displays `"-X.X dB"`, while Ladder B
tracks real output peak level.

---

## Signal flow and audio routing

### The chain

```
host input (stereo or mono)
        │
        ▼
┌───────────────────┐   order[] is a permutation of { Console, Opto, FET, Limiter }
│  slot 0           │   ── whichever module you dragged here first
│  slot 1           │
│  slot 2           │
│  slot 3           │
└───────────────────┘
        │
        ▼
master output trim (−60 … +6 dB)
        │
        ▼
host output
```

Each module processes **in place** — it reads and writes the same buffer and returns it — so the chain
is a straight serial insert. After the modules, a smoothed master gain controls the final output:

```cpp
for (int id : getOrder())
    modules[id]->run (buffer);
masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterLevel->load()));
masterGain.applyGain (buffer, buffer.getNumSamples());
```

`getOrder()` is a `std::array<int,4>` where `order[slot] = moduleId`, published atomically as a packed
`uint32_t` (one byte per slot). The audio thread reads it once per block, so dragging a plate in the GUI
cannot tear the chain mid-block.

**There is no parallel path, no sidechain, no mid/side split and no separate wet/dry.** RackStrip is an
insert effect: what comes out is one continuous chain from the first plate to the last. If you want a
parallel or sidechain version of it in your DAW, use the DAW's routing (bus + send, or a host-side
sidechain bus), not the plugin.

### Where it sits in a project

A typical mastering or tracking workflow:

```
[source] → RackStrip → [metering / dither / delivery]
```

- **Tracking or mixing a channel:** usually just the console strip and perhaps the FET, with the limiter
  bypassed.
- **Mastering:** console strip for tone, opto for gentle levelling, limiter last with a ceiling around
  −0.3 dB (or −1.0 dB for lossy delivery).
- The plate order is what matters, not which plate you think of as "first" — a limiter dragged to slot 0
  really will limit before the EQ.

### Bypass

Each module has its own bypass. A bypassed module is skipped **entirely** — no filtering, no gain, and
its gain-reduction meter drops to zero — *except* that the limiter keeps its delay line running when
bypassed (see below). Bypass is a parameter, so it is automated and saved like everything else.

---

## Latency and phase

The limiter's look-ahead is the only source of latency:

```
lookahead = ceil(0.0015 × sampleRate) samples      // ≈ 1.5 ms
reported  = lookahead − 1                          // what the host is told
```

At 48 kHz that is **66 samples ≈ 1.375 ms**.

The important property is that the reported latency is **constant regardless of module order or bypass
state**. That is deliberate: it means re-ordering plates or bypassing a module never shifts the track
in time relative to other tracks, so you can A/B and re-order without re-aligning anything.

This works because the limiter keeps its delay line running even when bypassed (`runLimiter(buf, false)`
still pushes samples through the delay). Without that, bypassing the limiter would remove 1.4 ms from
the chain and every other track in the session would drift.

The EQ and compressor stages are minimum-phase IIR filters and add no latency. The console output stage
and the FET/opto saturation are memoryless waveshapers.

---

## Channel handling and audio path verification

The plugin is an **Audio Unit Effect** (`aufx`) supporting both full stereo and pure mono tracks. The channel paths have been verified against both the DSP implementation and automated test suites:

### 1. Bus negotiation & layout constraints
- **Supported configurations**: Stereo in / Stereo out (`2 → 2`) and Mono in / Mono out (`1 → 1`).
- **Asymmetric bus protection**: Enforced strictly by `isBusesLayoutSupported()`. Asymmetric routing requests (e.g. `1 → 2` or `2 → 1`) are explicitly rejected by the host negotiation, preventing unintended panning offsets or buffer overrun bugs.

### 2. Module-by-module mono vs. stereo architecture

| Module | Mono Processing (`1ch`) | Stereo Processing (`2ch`) | Stereo Image & Phase Integrity |
| :--- | :--- | :--- | :--- |
| **API 560 Graphic EQ** | `channel[0]` is processed; `channel[1]` remains idle with zero state activity. | Two completely independent filter engines: 4th-order Butterworth upsampling + 10-band peaking SVF + 4th-order Butterworth downsampling per channel. | **Zero crosstalk**: L and R audio streams are completely isolated (tested with isolated single-channel input). Filter coefficients are smoothed once per native sample frame in `beginSample()`, guaranteeing identical frequency and phase responses across both channels. |
| **Opto Leveller** | Evaluates ch 0 peak; applies smooth program-dependent gain reduction to ch 0. | **Stereo-linked detector**: computes unified peak `pk = max(\|L\|, \|R\|)` and derives a single attenuation factor `g`. | **Zero stereo drift**: Applied scalar gain `g` is identical for both channels. Verified with `0.000000` L/R ratio error under compression; off-center transients never pull or collapse the stereo image. |
| **FET Compressor** | Drives input into fixed −10 dB threshold on ch 0. | **Stereo-linked detector**: evaluates peak across both channels after input drive gain is applied. | Fast attack/release envelope computes a unified gain reduction factor `g` applied equally to both channels before soft-clipping. |
| **Master Limiter & Auto Gain** | Uses `delay[0]` & `dryDelay[0]`; energy scaled by `1.0`. | Independent lookahead delay ring buffers (`delay[2]`, `dryDelay[2]`). Power scaled by `1.0 / nch` (`0.5`). | **Linked ceiling & auto-gain**: Auto Gain RMS power estimation operates on true mean channel power (0.000 dB RMS tracking accuracy in both 1ch and 2ch). Ceiling safety clamp (`safety = ceil / outputPeak`) is linked across both channels, maintaining exact channel balance. |
| **Master Output** | 20 ms smoothed gain ramp on ch 0. | Identical smoothing ramp applied to L and R. | Zero zipper noise, zero inter-channel phase shift. |

### 3. Latency & bypass phase consistency
- **Constant Lookahead Delay**: The Limiter delay line runs sample-exact (`ceil(0.0015 × sr) − 1` samples, ~1.375 ms at 48 kHz) in both active and bypassed states (`runLimiter(buf, false)` continues buffering).
- **Phase alignment in DAW mix**: Bypassing individual modules or dragging plates to change rack order does not alter plugin latency, completely eliminating comb filtering or timing drift against parallel tracks in your DAW.
- The plugin is **not** a synth, declares no MIDI input or output, and does not report a tail beyond its latency. It declares itself as `kAudioUnitType_Effect` with subtype `Zych` and manufacturer `ZYUS`.

---

## Re-ordering the chain

Drag the **six-dot grip** that sits on the chassis above any plate. The plate lifts, the others slide
aside to show where it will land, and on release the new order is committed to the processor and saved
with the session.

The order is stored as the `order` property on the plugin state (`"0,1,2,3"` style), validated on load:
a state that is not a permutation of the four module IDs is rejected rather than applied, so a corrupt
preset cannot produce a chain with a missing or duplicated stage.

---

## Parameters

40 automated parameters in total, including ten graphic-EQ bands and Enhance Auto Gain.
All floating-point parameter values and slider popup displays are cleanly rounded to **1 decimal place**
(e.g. `20.0 Hz`, `-0.3 dB`, `100.0 ms`, `4.0 : 1`).

| Module | Prefix | Parameters |
| --- | --- | --- |
| Console | `con_` | `bypass`, `hpf`, `thresh`, `ratio`, `attack`, `release`, `out`; `eq_16k`, `eq_8k`, `eq_4k`, `eq_2k`, `eq_1k`, `eq_500`, `eq_250`, `eq_125`, `eq_63`, `eq_31` |
| Opto | `opto_` | `bypass`, `peak`, `gain`, `mode` |
| FET | `fet_` | `bypass`, `input`, `output`, `attack`, `release`, `ratio` (choice) |
| Limiter | `lim_` | `bypass`, `gain`, `ceiling`, `release`, `enhance`, `auto_gain` |
| Master | `master_` | `level` (−60 … +6 dB, default 0.0 dB) |

The six old four-band EQ parameters (`con_lf`, `con_lmf_f`, `con_lmf`, `con_hmf_f`,
`con_hmf`, `con_hf`) remain registered at their existing host indices but are inactive after replacement.
HPF remains active. Existing sessions start the new ten-band EQ flat.

The **ON** switch on the opto plate is wired to the `opto_mode` parameter (compress ⇄ limit). The
**METER** lamp and the **Comp**/**Gate** lamps on the console plate are faceplate indicators only — this
build has no gate, and the meter always displays reduction.

---

## Build and install

Requires CMake 3.22+, a C++17 compiler (Xcode command-line tools on macOS) and Python 3 for the helper
scripts. JUCE 8.0.4 is downloaded automatically on the first configure.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 8
```

The build targets **AU only** and is configured for **arm64** (see `CMakeLists.txt`). With
`COPY_PLUGIN_AFTER_BUILD` enabled the bundle is installed to:

```
~/Library/Audio/Plug-Ins/Components/zychanstrip.component
```

If you have no system CMake, a pip install works and lands here:

```bash
pip3 install cmake ninja
export PATH="$HOME/Library/Python/3.13/bin:$PATH"
```

### Validating the AU

```bash
auval -v aufx Zych ZYUS
```

If a host or `auval` reports *"didn't find the component"* while the bundle is definitely installed, the
system audio-component registrar may need a restart — this is a machine-level condition, not a plugin
build problem:

```bash
sudo launchctl kickstart -k system/com.apple.audio.AudioComponentRegistrar
```

### Identity and configuration

The plugin ID is configured as `zychanstrip`:
- `PRODUCT_NAME`: `zychanstrip`
- `BUNDLE_ID`: `net.zyusof.zychanstrip`
- `COMPANY_NAME`: `ZYUSOF`
- `PLUGIN_MANUFACTURER_CODE`: `ZYUS`
- `PLUGIN_CODE`: `Zych`

---

## Development tools

### Headless GUI snapshot

`tools/build_snapshot.py` links a small application against the objects the main build already produced
and renders the editor at minimum, default and maximum sizes. It also checks master gain in mono
and stereo, smoothing, parameter attachment, state recall, fixed-ratio resizing and module dragging.

```bash
cmake --build build --config Release        # produce the module objects once
python3 tools/build_snapshot.py             # build the snapshot tool
build/snapshot/snapshot /path/to/output     # checks + three rack-WxH.png snapshots
# For an out-of-tree Ninja build, pass --build-dir /path/to/build to build_snapshot.py.
```

It reuses the exact `DEFINES`, `FLAGS` and `INCLUDES` that Ninja used for `PluginEditor.cpp.o`, so the
snapshot matches the real build rather than approximating it.

### Spec

`docs/specs/photoreal-gui.md` records the six-phase lifecycle for the interface work — requirements,
design, build, test, deploy and maintain — and validates with:

```bash
python3 ~/.config/opencode/scripts/dev_spec.py lint docs/specs/photoreal-gui.md
```

---

## Design notes and limits

- **It is an original design.** The four plates are *inspired by* the classic console, optical, FET and
  mastering-limiter topologies, and the reference hardware used while designing the panel is credited by
  the model badges on the plates. No manufacturer's code, circuit models, artwork or trademarks are
  reproduced.
- **The DSP is a clean first pass.** Sensible next steps would be oversampling the saturation stages,
  a sidechain filter, and removing the small allocation that happens when EQ coefficients change.
- **The meters are cosmetic read-outs of real data.** They show gain reduction and levels actually
  reported by the processor, but they are not calibrated reference meters — use a proper metering plugin
  for LUFS and true peak when mastering.
- **The master bar in the header** controls the final output after all modules. It ranges from −60 to
  +6 dB, saves with the session and supports host automation. Double-click to restore 0 dB. Changes
  ramp over 20 ms; sessions saved before this control existed restore at unity. Positive master gain
  can raise the output above the limiter ceiling because it follows the limiter.


## Latest update: EQ560 and Enhance Auto Gain

- Replaced the four-band EQ controls with ten 560-style horizontal faders, using the supplied GUI reference.
- Ported the MIT-licensed [EQ560 DSP core](https://github.com/chkhld/jsfx/blob/f6d175d8253375a51832fd52aa86482c191caadc/Equalizer/eq_560.jsfx), preserving its proportional-Q filter and 2× resampling design.
- Added automatic input/output RMS compensation inside the Enhance module, with a visible on/off control.
- Kept the compact proportional resizing, reorderable modules and working manual Master output.
- Added response, gain-matching, state compatibility and GUI regression checks; bundled upstream attribution in the AU.

The EQ560 source credits [Andrew Simper’s state-variable filter design](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf)
and [Exstrom’s Butterworth design](https://www.exstrom.com/journal/sigproc/). RackStrip’s limiter and Auto Gain code is
in `Source/Modules.h`; it is implemented in this project rather than copied from the EQ560 source.
