<p align="center">
  <img src="docs/screenshots/zychanstrip-rack.png" alt="zychanstrip — analog mastering and tracking rack" width="100%">
</p>

<h1 align="center">zychanstrip</h1>

<p align="center"><strong>Four pieces of classic analog outboard. One rack. No menu diving.</strong></p>

<p align="center">
  <a href="#the-rack"><img src="https://img.shields.io/badge/Version-1.2.0-green" alt="Version 1.2.0"></a>
  <a href="#compatibility"><img src="https://img.shields.io/badge/Format-AUv2-2f6feb" alt="Audio Unit"></a>
  <a href="#compatibility"><img src="https://img.shields.io/badge/macOS-Apple%20Silicon-111111" alt="Apple Silicon"></a>
  <a href="#the-meter-page"><img src="https://img.shields.io/badge/Metering-True%20Peak%20%C2%B7%20LUFS%20%C2%B7%20RTA-8b5cf6" alt="Metering"></a>
</p>

<p align="center">
  <a href="docs/zychanstrip-dsp-flow.html"><strong>See how the signal flows →</strong></a>
</p>

---

## The sound in one sentence

**zychanstrip** puts a classic 10-band graphic EQ, a fast FET compressor, an opto leveler and a
mastering-grade limiter on one photo-real rack — so you can shape, control and finish a mix
without opening four different plugins.

It is built for tracking rooms that want a fast, musical chain and for mastering sessions that
want an honest, repeatable one. Every control is on the front panel, every change is automatable,
and the whole rack recalls exactly as you left it.

---

## Why it feels like hardware

Most channel strips are a list of controls with a picture behind them. zychanstrip is built the
other way around: the faceplates are the interface.

- **One rack, four classics.** Reorder the modules by dragging the faceplates — the signal path
  follows the rack, not a hidden menu.
- **Real colour, not just EQ.** A single COLOUR switch moves the whole rack between algorithmic
  saturation, embedded hardware captures and a clean path, with seamless crossfades and constant
  latency. No clicks, no level jumps.
- **Hear tone, not volume.** UNITY matches the finished loudness to your input while you audition
  changes, so a brighter EQ no longer sounds "better" just because it is louder.
- **A reset you can trust.** One click returns the rack to its documented hardware-neutral
  starting profile — gain, ratios, timing, order and meters included.
- **Metering that tells the truth.** The analyser page is fed by the audio engine, not by a
  cosmetic animation: true peak, RMS, short-term loudness, phase scope, correlation and a
  31-band real-time analyser.

---

## The rack

### Graphic EQ
Ten proportional-Q bands from 31 Hz to 16 kHz, ±12 dB, with the musical interaction of a classic
console EQ. Broad strokes stay broad; surgical moves stay surgical.

### FET compressor
The fast, aggressive one. Input and output gain, attack, release and a five-position ratio bank
including the famous all-buttons mode. Perfect for drums, bass and anything that needs attitude.

### Opto leveler
The smooth one. Program-dependent release and a two-position compress/limit switch make it the
natural choice for vocals and mix bus glue — level first, character second.

### Enhance limiter
A look-ahead brick-wall limiter with tape/console saturation and stereo-linked auto gain, so you
can push a master to competitive loudness without losing the body of the mix. The four controls
live on one shared fader plate, exactly where your hand expects them.

---

## Colour: ALGO · NAM · CLEAN

Colour is the difference between "a plugin that EQ's" and "a rack that sounds like a room".

| Mode | What it does |
| --- | --- |
| **ALGO** | Algorithmic saturation modelled on classic console and transformer behaviour. |
| **NAM** | Embedded captures of real hardware — an API-style preamp, an opto leveler, a FET compressor and a Studer tape machine. |
| **CLEAN** | The full EQ, compression and limiting, with the added colour removed. |

Switching modes crossfades over 20 ms and keeps latency constant, so you can audition colour
while the mix keeps playing. The choice is saved with the session and available to host automation.

---

## The meter page

Flip the **RACK / METER** switch and the same window becomes a mastering analyser:

- per-channel true peak and RMS with max-hold
- short-term loudness in LUFS
- Lissajous phase scope and a correlation bar
- 31-band real-time analyser with averaging
- gain-reduction and output meters for the rack modules

It is a pure observer: it never touches the audio, and it only runs while you are looking at it.

---

## Workflow details that matter

- **Parallel processing** on a master dry/wet control, with the dry path latency-matched so the
  blend stays phase-correct from fully dry to fully wet.
- **Per-module bypass** without losing settings.
- **Persistent rack order** — your layout is part of the session.
- **Mouse wheel** to adjust, **⇧ Shift** for fine steps, **double-click** to return to default.
- **Fully automatable** — every control is host-visible, and sessions from earlier versions
  recall correctly.

---

## Compatibility

- macOS on **Apple Silicon** (arm64)
- **Audio Unit (AUv2)** effect — works in Logic Pro, Ableton Live, Reaper, Studio One, Bitwig and
  any other AU host
- Stereo operation at your session's sample rate
- No iLok, no dongle, no account, no runtime network connection

---

## Install

1. Download **`zychanstrip-1.2.0-arm64.zip`** from the [latest release](../../releases/latest).
2. Unzip and copy `zychanstrip.component` to `~/Library/Audio/Plug-Ins/Components/` — or to
   `/Library/Audio/Plug-Ins/Components/` to make it available to every user on the machine.
3. Restart your host (or rescan plugins) and insert **zychanstrip** on a stereo track.

The release includes a SHA-256 checksum next to the download.

---

## What's new in 1.2.0

- Final-stage 2× oversampling across the rack
- Photo-real bitmap faceplates with fixed lighting, softer contact shadows and refined fader caps
- Smoothed fader transitions and corrected knob scales
- RESET re-tuned to the documented hardware operating references
- Latency reported once to the host: 247 samples (5.15 ms at 48 kHz)

---

## A note on the classics

zychanstrip is an original product inspired by the sound of classic outboard gear. It is not
affiliated with, endorsed by, or licensed by any hardware manufacturer. Product names are used
only to describe the character being modelled, and remain the trademarks of their owners.

Hardware captures and profiles contributed by third parties are credited in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

---

<details>
<summary><strong>Build from source</strong></summary>

Requires macOS, Xcode Command Line Tools, CMake 3.22+ and Ninja.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

The built component appears in `build/zychanstrip_artefacts/Release/AU/`.

</details>

<p align="center"><sub>Made for people who still mix with their ears.</sub></p>
