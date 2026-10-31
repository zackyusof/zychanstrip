# zychanstrip

### The Ultimate Analog Mastering & Tracking Rack for macOS

[![Audio Unit](https://img.shields.io/badge/Format-AUv2%20(Audio%20Unit)-blue.svg)](#build-and-install)
[![Architecture](https://img.shields.io/badge/Architecture-Apple%20Silicon%20(arm64)-orange.svg)](#build-and-install)
[![Framework](https://img.shields.io/badge/Built%20with-JUCE%208-green.svg)](#build-and-install)
[![Zero Bitmaps](https://img.shields.io/badge/GUI-100%25%20Procedural%20Vector-purple.svg)](#photo-real-modular-gui)

**zychanstrip** brings the sound, feel, and physical presence of four legendary pieces of outboard analog hardware directly into your DAW as a high-performance **Audio Unit (AU) effect**. Designed to recreate the experience of working on a top-flight studio console, every module is housed on its own distinct brushed metal face plate with tactile controls, authentic component-level saturation modeling, and museum-grade vintage broadcast meters.

Drag plates into any order with the chassis grips to instantly rewire your signal chain on the fly. Whether tracking lead vocals through an optical tube leveler, slamming drum busses with a lightning-fast Class-A FET limiter, sculpting surgical master curves with a proportional-Q 10-band graphic EQ, or gluing the mix bus with a tape-saturated brickwall limiter, **zychanstrip** delivers punch, warmth, and analog authority with zero compromises.

---

## Why zychanstrip?

In a world filled with flat, generic digital plugins, **zychanstrip** is built from the ground up for producers, mixing engineers, and mastering pros who demand both analog character and modern workflow:

* **Four Iconic Hardware Topologies in One Chassis**:
  * **API 560-Style 10-Band Graphic EQ**: Proportional-Q constant-bandwidth sculpting with 2520 discrete op-amp & 2503 transformer iron saturation.
  * **Teletronix LA-2A-Style Opto Leveler**: Silky, program-dependent optical compression powered by 12AX7/12BH7 tube warmth and UTC transformer weight.
  * **UREI 1176 LN-Style FET Compressor**: Blazing-fast transient control, all-buttons mode, and aggressive Class-A FET punch with 11754 output bite.
  * **Master Brickwall Limiter & Auto Gain**: True peak limiting with analog tape drive, master console saturation, and linked RMS loudness compensation.
* **Instant Drag-and-Drop Modular Reordering**: Want EQ before compression? Or opto leveling feeding aggressive FET limiting? Simply grab the six-dot chassis grip and drag. The audio engine switches order atomically on buffer boundaries—completely glitch-free, with the order saved right into your DAW session.
* **Rock-Solid Phase & Constant-Latency Guarantee**: Moving modules or toggling bypass states never changes plugin latency by even a single sample. Your DAW mix stays 100% phase-aligned with zero comb filtering or timing drift against parallel tracks.
* **True Stereo Link with Zero Stereo Drift**: Detectors evaluate unified stereo energy across both channels to apply identical scalar gain reduction, guaranteeing zero image tilt or center collapsing even under heavy compression.
* **100% Procedural Resolution-Independent Retina Graphics**: Zero bitmap PNGs or compressed image strips. Every knob skirt, bezel screw, brushed hair texture, specular reflection, and needle shadow is rendered procedurally in real-time. Resizes smoothly from compact **864 × 608** up to **1620 × 1140** while locking an optimal 27:19 hardware aspect ratio.
* **Studio-Grade Vintage Broadcast VU Meters**: Precision ANSI C16.5 / IEEE 152 ballistic meters with authentic logarithmic scaling, red overload saturation zones, knife-edge tapered needles, and incandescent tungsten illumination.

---

## Contents

- [The Four Modules & Analog Color](#the-four-modules--analog-color)
  - [1. API 560 10-Band Graphic Equalizer](#1-api-560-10-band-graphic-equalizer)
  - [2. Opto Leveler (Teletronix LA-2A Style)](#2-opto-leveler-teletronix-la-2a-style)
  - [3. FET Compressor (UREI 1176 LN Style)](#3-fet-compressor-urei-1176-ln-style)
  - [4. Master Limiter & Auto Gain](#4-master-limiter--auto-gain)
- [Vintage Broadcast VU Metering](#vintage-broadcast-vu-metering)
- [Signal Flow and Audio Routing](#signal-flow-and-audio-routing)
- [Latency and Phase Consistency](#latency-and-phase-consistency)
- [Channel Handling & Bus Architecture](#channel-handling--bus-architecture)
- [Re-Ordering the Chain](#re-ordering-the-chain)
- [Parameters](#parameters)
- [Build and Install](#build-and-install)
- [Development & Verification Tools](#development--verification-tools)
- [Latest Updates](#latest-updates)

---

## The Four Modules & Analog Color

Each module features a dedicated non-linear analog color model recreating the harmonic profiles, transformer cores, and circuit saturation of legendary analog classics:

### 1. API 560 10-Band Graphic Equalizer

An authentic recreation of the legendary American ten-band graphic equalizer. Built with full-height fader travel, brushed anodized dark plate, discrete op-amp chevron insignia, bezel mounting holes, and a hardware **IN** bypass switch.

* **Analog Color Model (API 2520 Discrete Op-Amp & 2503 Transformer)**:
  * Models the discrete op-amp's punchy, forward midrange response combined with asymmetric second-harmonic warmth and third-harmonic transformer iron core saturation (`apiConsoleColor`). Push the faders to add analog bite and drive without harshness.
* **Proportional-Q Response**:
  * Bandwidth automatically narrows as boost or cut increases, providing gentle shaping at moderate settings (±2–4 dB) and surgical control at extreme settings (±12 dB).
* **2× Butterworth Oversampling**:
  * Adapted from chokehold's acclaimed EQ 560 C++ DSP core, ensuring pristine high-frequency response up to 16 kHz with zero Nyquist cramping.

| Band | Frequency | Range | Default | Characteristic |
| :--- | :--- | :--- | :--- | :--- |
| **Bands 1–10** | 16k, 8k, 4k, 2k, 1k, 500, 250, 125, 63, 31 Hz | ±12.0 dB each | 0.0 dB | 10 proportional-Q bands; highlighted red cap on 1 kHz |
| **IN Switch** | – | on / off | on | True hardware bypass for the EQ module |

---

### 2. Opto Leveler (Teletronix LA-2A Style)

A faithful recreation of the revered electro-optical tube leveler. Celebrated worldwide for its velvety smooth, musical response, it uses a multi-stage program-dependent release that allows vocals, bass, and acoustic instruments to sit effortlessly in the mix without pumping.

* **Analog Color Model (12AX7 / 12BH7 Triode Tube & UTC Transformer)**:
  * Captures the rich even-order harmonic distortion (predominantly 2nd harmonic) and lush saturation of vintage tube gain stages (`optoTubeColor`), imparting immense weight and warmth.
* **Program-Dependent Release**:
  * Dual-time-constant release circuit blends a rapid ~60 ms transient recovery with a gentle ~1.8 s release for sustained passages.
* **Large Circular Vintage VU Meter**:
  * Dominated by a massive 214 × 214 px circular broadcast meter with 4 slotted quadrant screws, stepped brass retaining ring, aged ivory dial face, and incandescent tungsten illumination.

| Control | Range | Default | Function |
| :--- | :--- | :--- | :--- |
| **Gain** | 0.0 – 100.0 | 0.0 | Ultra-clean tube make-up gain, up to +30 dB |
| **Peak Reduction** | 0.0 – 100.0 | 0.0 | Drives input into optical attenuator (threshold 0 to −40 dB) |
| **ON (LIMIT)** | on / off | off | Toggles between smooth **Compress (3:1)** and authoritative **Limit (12:1)** |
| **METER** | on / off | on | Faceplate lamp and meter mode selector |

---

### 3. FET Compressor (UREI 1176 LN Style)

The definitive ultra-fast solid-state peak limiter. Known as the secret weapon for punchy drums, commanding vocals, and aggressive rock bass, it features lightning-fast attack times down to 20 microseconds and a fixed threshold topology driven by input gain.

* **Analog Color Model (Class-A FET & 11754 Output Transformer)**:
  * Generates the signature aggressive odd-order harmonics (predominantly 3rd harmonic) and transient edge of the classic Rev D/E solid-state circuit (`fetColor`), injecting energy and excitement into dull sources.
* **All-Buttons Mode ("British Mode")**:
  * Features the complete pushbutton ratio bank: **4:1**, **8:1**, **12:1**, **20:1**, and the iconic **ALL** mode for explosive drum room squash and extreme parallel compression.
* **Scaled-Up Simpson 29T Rectangular VU Meter**:
  * Sits prominently in the center of the module (216 × 154 px) with a stepped chrome bezel, signature lower curved bakelite apron mask, and centered mechanical **`ZERO ADJ`** slotted calibration screw.

| Control | Range | Default | Function |
| :--- | :--- | :--- | :--- |
| **Input** | 0.0 – 30.0 dB | 0.0 dB | Drives signal into fixed −10 dB threshold (more input = more compression) |
| **Output** | −20.0 – +20.0 dB | 0.0 dB | Clean output makeup gain |
| **Attack** | 1.0 – 7.0 | 3.0 | 0.8 ms (pos 1) down to 20 µs (pos 7) — higher is faster |
| **Release** | 1.0 – 7.0 | 4.0 | 1100 ms (pos 1) down to 50 ms (pos 7) — higher is faster |
| **Ratio Bank** | 4 / 8 / 12 / 20 / ALL | 4:1 | Backlit pushbutton selector |

---

### 4. Master Limiter & Auto Gain

A precision look-ahead brickwall mastering limiter designed to maximize loudness while safeguarding pristine audio fidelity.

* **Analog Color Model (Master Console & Analog Tape Saturation)**:
  * Combines analog tape odd-harmonic compression and console bus saturation (`analogMasterSaturation`) with the Enhance drive circuit to round off harsh transients before brickwall limiting.
* **Enhance Drive Fader**:
  * Adds up to 12 dB of saturation and tape coloration to thicken masters and glue mixes together.
* **Stereo-Linked Enhance Auto Gain**:
  * Continuously evaluates latency-aligned RMS input vs. output levels, applying smooth, artifact-free compensation so you hear tone changes without being fooled by loudness shifts.
* **Dual Segmented LED Ladders with Idle Blanking**:
  * Features high-resolution 28-segment ladders tracking gain reduction (Ladder A, amber) and output peak level (Ladder B, green-to-cyan). When audio stops or during pauses, both ladders extinguish to zero LEDs, keeping your display quiet and focused.

| Control | Range | Default | Function |
| :--- | :--- | :--- | :--- |
| **Gain** | 0.0 – 18.0 dB | 0.0 dB | Drives signal into limiter threshold |
| **Ceiling** | −12.0 – 0.0 dB | −0.3 dB | True brickwall output ceiling |
| **Release** | 10.0 – 1000.0 ms | 100.0 ms | Smooth limiter recovery time |
| **Enhance** | 0.0 – 100.0 | 0.0 | Multi-stage tape drive and harmonic saturation fader |
| **Auto Gain** | on / off | on | Linked stereo RMS level matching |

---

## Vintage Broadcast VU Metering

The VU meters on the **Opto (LA-2A)** and **FET (1176)** modules are custom-engineered procedural vector instruments designed to match the physics and aesthetic standards of vintage studio hardware:

* **ANSI C16.5 / IEEE 152 Standard Ballistics**:
  * The scale reflects true analog voltmeter physics where needle deflection follows the voltage ratio $V = 10^{\text{dB}/20}$:
  * Marks: `-20`, `-10`, `-7`, `-5`, `-3`, `-2`, `-1`, `0`, `+1`, `+2`, `+3` VU.
  * `-20 to 0 VU`: Rendered in crisp black tracking ink.
  * `0 to +3 VU`: High-visibility red overload saturation sector with red tick marks and numerals.
* **Secondary Modulation Percentage Scale**:
  * Lower concentric arc running from `0` to `100%`, with `100%` aligning precisely beneath `0 VU`.
* **Hardware Gain Reduction Deflection**:
  * When idling or at unity, the needle rests elegantly at `0 VU` (0 dB GR).
  * As gain reduction occurs, the needle swings down to the left, directly indicating the exact decibels of compression (`-3 dB`, `-7 dB`, `-10 dB`, `-20 dB`) matching hardware front panels.
* **Mechanical Needle Geometry**:
  * Knife-edge black needle tapering to a hairline tip.
  * Realistic counter-balance teardrop tail extending behind the pivot.
  * Molded black armature hub with concentric brass/nickel jewel screw and specular highlight.
  * Dynamic translucent drop shadow cast onto the dial face paper.
* **Authentic Dial Face & Illumination**:
  * Warm aged ivory/buff parchment face with incandescent tungsten bulb backlighting glow from above.
  * Recessed inner drop shadows and convex/planar glass reflections for realistic physical depth.
* **Zero Negative Space Waste**:
  * Both modules feature balanced vertical layouts with period-accurate nomenclature plates (`TELETRONIX AUTOMATIC LEVELING AMPLIFIER` and `LIMITING AMPLIFIER MODEL 1176 LN`), providing museum-grade visual polish with zero clipping or border overflow.

---

## Signal Flow and Audio Routing

```
Host Input (Stereo or Mono)
        │
        ▼
┌───────────────────┐   order[] is any permutation of { Console, Opto, FET, Limiter }
│  Slot 0           │   ── whichever module you dragged into this position
│  Slot 1           │
│  Slot 2           │
│  Slot 3           │
└───────────────────┘
        │
        ▼
Master Output Trim (−60 … +6 dB)
        │
        ▼
Host Output (Stereo or Mono)
```

Each module processes audio in-place within the buffer. The rack order is published atomically as a packed 32-bit integer read once per audio block. Dragging modules never causes audio dropouts, clicks, or block tearing.

---

## Latency and Phase Consistency

* **Constant Lookahead Latency**:
  $$\text{lookahead} = \lceil 0.0015 \times \text{sampleRate} \rceil - 1 \quad (\approx 1.375\text{ ms at } 48\text{ kHz})$$
* **Zero Timing Drift**: The limiter's ring buffer continues cycling even when bypassed. Moving plates or bypassing modules never alters reported latency, guaranteeing 100% phase alignment with parallel DAW tracks.
* The EQ, Opto, and FET modules are minimum-phase designs adding zero latency.

---

## Channel Handling & Bus Architecture

* **Strict Bus Negotiation**: Fully supports stereo-in/stereo-out (`2 → 2`) and mono-in/mono-out (`1 → 1`). Asymmetric configurations (`1 → 2` or `2 → 1`) are rejected cleanly at host negotiation.
* **Stereo Linking**: Opto, FET, and Limiter detectors evaluate true unified stereo energy, applying identical scalar gain reduction across channels. Verified with `0.000000` L/R ratio drift to preserve mix width and phantom center stability.

---

## Re-Ordering the Chain

To re-order the signal chain, click and drag the **six-dot chassis grip** above any module. The target modules animate aside, and releasing commits the new chain order immediately. The configuration is serialized into the plugin preset and recalls automatically with your DAW session.

---

## Parameters

40 automated parameters exposed with clean 1-decimal rounding (e.g. `20.0 Hz`, `-0.3 dB`, `100.0 ms`):

| Module | Prefix | Automated Parameters |
| :--- | :--- | :--- |
| **Console** | `con_` | `bypass`, `eq_16k`, `eq_8k`, `eq_4k`, `eq_2k`, `eq_1k`, `eq_500`, `eq_250`, `eq_125`, `eq_63`, `eq_31`, `hpf`, dynamics legacy controls |
| **Opto** | `opto_` | `bypass`, `gain`, `peak`, `mode` (compress/limit) |
| **FET** | `fet_` | `bypass`, `input`, `output`, `attack`, `release`, `ratio` (4:1, 8:1, 12:1, 20:1, ALL) |
| **Limiter** | `lim_` | `bypass`, `gain`, `ceiling`, `release`, `enhance`, `auto_gain` |
| **Master** | `master_` | `level` (−60.0 to +6.0 dB, double-click restores 0.0 dB unity) |

---

## Build and Install

### Prerequisites
- macOS 10.13+ on Apple Silicon (arm64)
- CMake 3.22+ and Ninja (`pip3 install cmake ninja`)
- Xcode Command Line Tools (`xcode-select --install`)
- JUCE 8.0.4 (downloaded and cached automatically via CMake FetchContent)

### Quick Build & Install

```bash
git clone https://git.zyusof.net/zack/rackstrip.git
cd rackstrip

export PATH="$HOME/Library/Python/3.13/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

The compiled Audio Unit bundle installs automatically to:
```
~/Library/Audio/Plug-Ins/Components/zychanstrip.component
```

### Validate with auval
```bash
auval -v aufx Zych ZYUS
```

---

## Development & Verification Tools

### Headless Snapshot & Verification Suite
The built-in verification tool exercises DSP accuracy, stereo phase alignment, state persistence, and GUI resizing without requiring a DAW:

```bash
python3 tools/build_snapshot.py
build/snapshot/snapshot build/snapshot
```

Executes **195 automated tests** covering:
- Proportional-Q EQ band curves and 2× resampling accuracy.
- Enhance Auto Gain RMS loudness compensation matching.
- Master gain ramping, stereo phase synchronization, and preset recall.
- Headless GUI snapshots at `864×608`, `1080×760`, and `1620×1140` ensuring 100% control containment and zero clipping.

---

## Latest Updates

* **Complete Vintage VU Meter Overhaul (1176 & LA-2A)**:
  * Upgraded dial face graphics to authentic **ANSI C16.5 / IEEE 152 broadcast ballistic scales** with sharp black tracking lines, logarithmic voltage deflection, and bold red overload sectors.
  * Added secondary modulation percentage scale (`0 to 100%`) aligned with 0 VU.
  * Implemented realistic tapered knife-edge needles, counter-balance teardrop tails, brass jewel screw hubs, and dynamic drop shadows.
  * Scaled up the LA-2A meter into a massive circular instrument (214 × 214 px) with 4 quadrant bezel screws and stepped brass retaining rim.
  * Scaled up the 1176 meter by 50% into a rectangular Simpson 29T instrument (216 × 154 px) featuring the iconic lower curved black bakelite apron mask and mechanical `ZERO ADJ` calibration screw.
  * Rebalanced panel framing and furnished lower negative space with period-accurate hardware nomenclature sub-plates (`TELETRONIX AUTOMATIC LEVELING AMPLIFIER` and `LIMITING AMPLIFIER MODEL 1176 LN`).
* **API 560 Graphic Equalizer**:
  * Ten full-height proportional-Q vertical faders with discrete 2520 op-amp and 2503 transformer saturation.
* **Enhance Auto Gain**:
  * Real-time stereo-linked RMS input/output compensation inside the mastering limiter stage.
* **Active Idle Meter Blanking**:
  * Segmented LED ladders extinguish to zero during audio pauses for an uncluttered visual display.
