# RackStrip

A four-module channel strip for macOS as an **Audio Unit (AU) effect**, built with JUCE 8 and designed
to feel like four pieces of outboard gear bolted into one rack. Every module is a separate face plate
with its own character, and the plates can be dragged into any order — the signal chain re-wires to
match, and the order is saved with your session.

The interface is a photo-real rendering of that rack: brushed and anodised metal faces, bevelled
screws, machined knobs with printed scales, a brass VU movement and segmented LED ladders. There are
no bitmap assets — everything is drawn procedurally, so the panel stays sharp at any scale.

---

## Contents

- [The four modules](#the-four-modules)
- [Signal flow and audio routing](#signal-flow-and-audio-routing)
- [Latency and phase](#latency-and-phase)
- [Channel handling](#channel-handling)
- [Re-ordering the chain](#re-ordering-the-chain)
- [Parameters](#parameters)
- [Build and install](#build-and-install)
- [Development tools](#development-tools)
- [Design notes and limits](#design-notes-and-limits)

---

## The four modules

### 1. Console strip — EQ and dynamics

The broad "console channel" stage. A high-pass, a four-band EQ and a bus-style compressor, laid out as
two sections.

**EQ**

| Control | Type | Default | Notes |
| --- | --- | --- | --- |
| HPF | 20 – 300 Hz | 20 Hz | Fixed ~12 dB/oct high-pass, Q 0.707 |
| LF | ±15 dB | 0 dB | Low shelf at 100 Hz, Q 0.7 |
| LMF Hz / LMF dB | 200 – 2500 Hz / ±15 dB | 600 Hz / 0 dB | Peaking, Q 0.9 |
| HMF Hz / HMF dB | 1500 – 8000 Hz / ±15 dB | 3 kHz / 0 dB | Peaking, Q 0.9 |
| HF | ±15 dB | 0 dB | High shelf at 8 kHz, Q 0.7 |

**Dynamics**

| Control | Range | Default | Notes |
| --- | --- | --- | --- |
| Threshold | −40 – 0 dB | 0 dB | 4 dB soft knee |
| Ratio | 1.5:1 – 10:1 | 3:1 | |
| Attack | 0.3 – 30 ms | 10 ms | |
| Release | 50 – 1200 ms | 200 ms | |
| Out | ±12 dB | 0 dB | Make-up gain |

The detector is stereo-linked (the loudest channel drives both). The output stage has a very gentle
saturation that is transparent below 0.7 of full scale and eases into a soft curve above it — it is a
stand-in for analog output-stage behaviour, not a distortion effect. The two lamps labelled **Comp**
and **Gate** are faceplate indicators from the reference design; this build has no gate, and neither
lamp changes the audio.

### 2. Opto leveler

An optical-style leveller with a program-dependent, two-stage release — designed to breathe rather than
pump.

| Control | Range | Default | Notes |
| --- | --- | --- | --- |
| Gain | 0 – 100 | 0 | Make-up, up to +30 dB |
| Peak Reduction | 0 – 100 | 0 | Drives a threshold from 0 down to −40 dB |
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

| Control | Range | Default | Notes |
| --- | --- | --- | --- |
| Input | 0 – 30 dB | 0 dB | Drives the input stage into a fixed −10 dB threshold |
| Output | −20 – +20 dB | 0 dB | |
| Attack | 1 – 7 | 3 | Mapped to 0.8 ms (1) … 0.02 ms (7) — higher is faster |
| Release | 1 – 7 | 4 | Mapped to 1100 ms (1) … 50 ms (7) |
| Ratio | 4 / 8 / 12 / 20 / 30 | 4:1 | Backlit button bank (labelled `4:1 … ALL`) |

Attack and release are deliberately inverted against their panel numbers to match the hardware
convention (a higher number is a faster position). The small backlit dial is a gain-reduction meter.

### 4. Master limiter

A look-ahead brick-wall limiter, intended as the last stage.

| Control | Range | Default | Notes |
| --- | --- | --- | --- |
| Gain | 0 – 18 dB | 0 dB | Drives the limiter |
| Ceiling | −12 – 0 dB | −0.3 dB | Absolute output ceiling |
| Release | 10 – 1000 ms | 100 ms | |
| Enhance | 0 – 100 | 0 | Adds up to 12 dB of drive plus a harmonic blend |

The limiter uses a sliding-minimum over the look-ahead window followed by a moving average, so gain
reduction ramps in *before* the peak arrives instead of clamping after it — the standard way to get a
transparent brick-wall. **Enhance is not a separate effect module**: it raises the drive into the
limiter and mixes in `tanh(x) − x`, i.e. saturation harmonics. Unlike the drive, the harmonic blend is
applied *after* the look-ahead, so heavy Enhance settings can add harmonics that slightly exceed the
ceiling. Keep it low if you are mastering to a strict delivery spec.

The two tall ladders show gain reduction (amber) and output level (green through blue). They always
show activity so the panel reads like the hardware even when nothing is being limited.

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
host output
```

Each module processes **in place** — it reads and writes the same buffer and returns it — so the chain
is a straight serial insert. `processBlock` is simply:

```cpp
for (int id : getOrder())
    modules[id]->run (buffer);
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

## Channel handling

The plugin is an **effect** (`aufx`), and accepts stereo in/stereo out or mono in/mono out — the input
and output channel sets must match, which `isBusesLayoutSupported` enforces:

- **Stereophonic layout.** The EQ runs two independent filter instances (one per channel) so the left
  and right channels are filtered separately but with identical coefficients. All the compressors use a
  **stereo-linked** detector — the peak across channels drives one gain value — so the stereo image
  never collapses. This is the normal, correct choice for a master bus.
- **Mono layout.** A single filter instance and a single detector; the processors operate on channel 0
  only (`std::min(2, numChannels)` guards this everywhere).
- The plugin is **not** a synth, declares no MIDI input or output, and does not report a tail beyond its
  latency.

The AU declares itself as `kAudioUnitType_Effect` with subtype `Rkst` and manufacturer `Ycmp`, so hosts
list it under effects rather than instruments.

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

28 automated parameters in total.

| Module | Prefix | Parameters |
| --- | --- | --- |
| Console | `con_` | `bypass`, `hpf`, `lf`, `lmf_f`, `lmf`, `hmf_f`, `hmf`, `hf`, `thresh`, `ratio`, `attack`, `release`, `out` |
| Opto | `opto_` | `bypass`, `peak`, `gain`, `mode` |
| FET | `fet_` | `bypass`, `input`, `output`, `attack`, `release`, `ratio` (choice) |
| Limiter | `lim_` | `bypass`, `gain`, `ceiling`, `release`, `enhance` |

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
~/Library/Audio/Plug-Ins/Components/Rack Strip.component
```

If you have no system CMake, a pip install works and lands here:

```bash
pip3 install cmake ninja
export PATH="$HOME/Library/Python/3.13/bin:$PATH"
```

### Validating the AU

```bash
auval -v aufx Rkst Ycmp
```

If a host or `auval` reports *"didn't find the component"* while the bundle is definitely installed, the
system audio-component registrar may need a restart — this is a machine-level condition, not a plugin
build problem:

```bash
sudo launchctl kickstart -k system/com.apple.audio.AudioComponentRegistrar
```

### Changing the identity before shipping

Set `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` (exactly four characters, at least one uppercase) and
`PLUGIN_CODE` in `CMakeLists.txt`. Distributing AU builds outside your own machine needs code signing
and notarization.

---

## Development tools

### Headless GUI snapshot

`tools/build_snapshot.py` links a small application against the objects the main build already produced
and renders the editor to `snapshot.png`, so the interface can be reviewed without a DAW — which is
essential when AU discovery is unavailable.

```bash
cmake --build build --config Release        # produce the module objects once
python3 tools/build_snapshot.py             # build the snapshot tool
cd build/snapshot && ./snapshot             # writes snapshot.png (1376 × 768)
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
- **The master bar in the header** is a display element of the rack chrome; it does not alter the audio.
  Use the limiter for level control.
