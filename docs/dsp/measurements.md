# Measured behaviour — re-ordering, mix, auto-gain, ceiling

All numbers below come from `tools/research/permute.cpp`, which **includes the real
`Source/Modules.h`** through a JUCE shim and reproduces the chain loop of
`Source/PluginProcessor.cpp:113-150`. The full generated tables are in
[`measurements-raw.md`](measurements-raw.md); this page is the interpretation.

Run conditions: 48 kHz, block 256, 3.0 s per run, first 1.0 s discarded (settling).
Stereo, identical L/R (mono programme). Signals:

| id | content |
| --- | --- |
| multitone | 100 Hz @ 0.30 + 1 kHz @ 0.30 + 8 kHz @ 0.20 |
| transients | 4 Hz kick-like decaying 55→115 Hz sweep + noise hats + 1 % noise floor |

Presets:

| preset | what it is |
| --- | --- |
| unity | every band 0 dB, no gain reduction requested, limiter at 0 dB drive, ceiling −0.3, auto-gain off |
| mastering, AG off | EQ +3 dB @1k / +2 dB @63 / −2 dB @16k, Opto 35 % peak + 25 % gain, FET input +8 dB / output −2 dB, limiter +6 dB drive, ceiling −1.0, Enhance 30 %, auto-gain **off** |
| mastering, AG on | same, auto-gain **on** |

---

## 1. How order-dependent is the chain, really?

| preset | signal | RMS spread over 24 orders | peak spread | quietest order | loudest order |
| --- | --- | --- | --- | --- | --- |
| unity | multitone | **0.69 dB** | 0.21 dB | FET > EQ > OPTO > LIM | OPTO > EQ > FET > LIM |
| unity | transients | **0.71 dB** | 0.12 dB | FET > EQ > OPTO > LIM | OPTO > EQ > FET > LIM |
| mastering, AG off | multitone | **6.96 dB** | 7.08 dB | EQ > OPTO > LIM > FET | OPTO > EQ > FET > LIM |
| mastering, AG off | transients | **6.12 dB** | 6.40 dB | EQ > OPTO > LIM > FET | EQ > FET > LIM > OPTO |
| mastering, AG on | multitone | **4.25 dB** | 5.76 dB | EQ > OPTO > FET > LIM | LIM > FET > EQ > OPTO |
| mastering, AG on | transients | **5.46 dB** | 5.76 dB | EQ > OPTO > FET > LIM | LIM > FET > EQ > OPTO |

Three conclusions, all measured:

1. **The "unity" rack is not order-neutral.** It spreads 0.69 dB RMS even with every
   control at its neutral position, because `FetModule` compresses against a **fixed
   −10 dBFS threshold** (`Source/Modules.h:260`) driven by its input knob — at input 0 dB
   the multitone already produces 4.0–5.4 dB of gain reduction (raw table, section 2).
   A compressor that is never "off" makes the whole rack order-sensitive by construction.
2. **A hot mastering preset spreads ~7 dB RMS and ~7 dB peak across the 24 orders.**
   That is the practical size of the "why does dragging a module change my level" effect.
   It is dominated by detector drive (who gets the loud signal) and by the limiter's
   position, not by the colour stages.
3. **Auto-gain narrows the spread but does not remove it** (6.96 → 4.25 dB RMS). It can
   only correct what happens *upstream of itself* (see §4).

---

## 2. Dry/wet comb filtering (`zychanstrip-mix-phase`)

Chain latency is **71 samples** at 48 kHz (1.479 ms), all of it from the limiter.
The dry copy is not delayed before the blend (`Source/PluginProcessor.cpp:138-146`), so
the mix is a comb filter with the wet path.

| mix | dry delay | RMS dBFS | 1 kHz bin | vs 100 % wet |
| --- | --- | --- | --- | --- |
| 100 % (reference) | 0 | −12.16 | 34.6 | +0.00 dB |
| 50 %, shipped | 0 | −25.34 | 21.4 | **−13.20 dB** |
| 50 %, dry delayed 71 | 71 | −10.49 | 36.3 | +1.68 dB |
| 25 %, shipped | 0 | −13.82 | 33.0 | −1.66 dB |
| 25 %, dry delayed 71 | 71 | −9.73 | 37.1 | +2.43 dB |

* First comb notch: `48000 / (2 · 71) = ` **338.0 Hz**, repeating at every odd multiple
  (1014 Hz, 1690 Hz, …). This is why a 50 % mix thins the low-mid instead of halving it.
* Delaying the dry copy by the reported latency restores the expected `wet + dry` sum
  (+1.68 dB rather than −13.20 dB at 1 kHz) — the arithmetic fix proposed for
  `zychanstrip-mix-phase`.
* Note the *level* consequence too: at 25 % the shipped path is 1.66 dB **below** the wet
  signal, i.e. the mix control currently behaves as a low-mid scoop, not as a blend.

---

## 3. Limiter ceiling vs module order

Ceiling −1.0 dBFS. Two of the 24 orders break it:

| order | peak dBFS | verdict |
| --- | --- | --- |
| EQ > FET > LIM > OPTO | −0.80 | **exceeds ceiling by 0.20 dB** |
| FET > EQ > LIM > OPTO | −0.63 | **exceeds ceiling by 0.37 dB** |
| all other 22 orders | ≤ −1.00 | ceiling respected |

Mechanism: `OptoModule` runs after `LimiterModule` in those two orders and its
`optoTubeColor` (`Source/Modules.h:66-74`) adds 2nd-order asymmetry *after* the ceiling
clamp, so the sample peak grows again. The same applies to any module placed after the
limiter — the ceiling is only a true output ceiling when the limiter is last.

Practical rule: **the limiter is the output stage; putting anything after it re-opens the
ceiling.**

---

## 4. Auto-gain reference (`zychanstrip-autogain-order`)

| preset | order | RMS dBFS | limiter auto-gain correction |
| --- | --- | --- | --- |
| auto-gain off | EQ > FET > OPTO > LIM | −6.83 | +0.00 dB |
| auto-gain off | LIM > EQ > FET > OPTO | −8.29 | +0.00 dB |
| auto-gain on | EQ > FET > OPTO > LIM | −8.72 | −1.89 dB |
| auto-gain on | LIM > EQ > FET > OPTO | −8.63 | −1.52 dB |

The correction is computed from the limiter's **own** input vs its **own** output
(`Source/Modules.h:344-360`). Consequences:

* Upstream gain (EQ, both compressors) is inside the reference, so auto-gain does track it
  — which is why it *reduces* the spread across orders.
* Anything **after** the limiter is outside the reference, so the correction cannot know
  about it; with the limiter last the same settings land 1.9 dB lower than with
  auto-gain off, because the correction is measured before the output stage exists.
* The correction is not a null test: turning auto-gain on changes the absolute level by
  ~1.9 dB for the same programme. It matches the module's input, not the rack's input.

The README's `zychanstrip-autogain-order` row describes the same defect from the user's
side (0.691 vs 0.289 RMS across orders). The measured spread above (6.96 dB with AG off)
is consistent with it; the fix remains "measure chain input vs final output in
`processBlock` and share one reference with the mix".

---

## 5. Detector coupling

Mastering preset, multitone, auto-gain off. `gr` is each module's own `grDb`.

| order | Opto gr | FET gr | Lim gr | RMS dBFS |
| --- | --- | --- | --- | --- |
| EQ > FET > OPTO > LIM | 2.2 | 11.2 | 9.9 | −6.83 |
| EQ > OPTO > FET > LIM | 4.9 | 12.8 | 9.6 | −6.29 |
| FET > EQ > OPTO > LIM | 2.5 | 11.4 | 9.8 | −7.44 |
| LIM > EQ > FET > OPTO | 2.3 | 11.6 | 7.7 | −8.29 |

* Whichever compressor runs **first** does the most work: FET 11.2 dB before Opto vs
  12.8 dB after Opto (the opto removes 2.7 dB of drive from the FET detector when it goes
  first, and the FET's fixed threshold then bites harder/softer accordingly).
* The limiter's own gain reduction falls from 9.9 dB (last) to 7.7 dB (first) — when it is
  first it sees an uncompressed programme; when it is last it sees the result of ~15 dB of
  upstream compression.
* Level follows the detector chain: 1.5 dB difference between "FET first" and "FET last"
  at identical settings.

---

## 6. Colour-stage stacking

Limiter last in all four rows; only EQ / FET / Opto order changes.

| order | 100 Hz bin | 1 kHz bin | 8 kHz bin | RMS dBFS |
| --- | --- | --- | --- | --- |
| EQ > FET > OPTO > LIM | 36.0 | 37.1 | 28.8 | −6.83 |
| FET > OPTO > EQ > LIM | 36.1 | 37.2 | 28.9 | −6.75 |
| OPTO > FET > EQ > LIM | 36.2 | 37.2 | 29.0 | −6.70 |
| FET > EQ > OPTO > LIM | 35.4 | 36.5 | 28.3 | −7.44 |

Colour stacking is a **second-order** effect (< 0.3 dB in the band bins) compared with
detector drive (1.5 dB). The 0.7 dB drop in the last row is the FET's detector being
driven 3 dB harder (EQ boost in front of it), not the colour functions.

---

## 7. Invariants

Measured for every order:

* reported chain latency **71 samples** (EQ 0, Opto 0, FET 0, Limiter 71) — the sum is
  computed once in `prepareToPlay` (`Source/PluginProcessor.cpp:100-106`).
* master gain, dry/wet blend and the metering tap are applied **after** the module loop,
  so they are identical for all 24 orders.
* parameter values, bypass state and the state blob are order-independent (only the
  `order` property changes).

---

## Reproducing

```bash
tools/research/build.sh
./build/permute > docs/dsp/measurements-raw.md
```

The harness has no JUCE dependency, so it runs on Linux as well as macOS. If module DSP
changes, re-run and re-read this page — a stale table is worse than none.
