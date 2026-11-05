# FET module (`FetModule`) — 1176 style FET compressor

> **Beta.2 update:** global `colour_mode` selects ALGO, embedded NAM, or CLEAN.
> The routing and latency changes in [NAM colour DSP](nam-colour.md) supersede older
> colour-only/zero-latency statements and historical measurements below. NAM is stateful;
> ALGO retains the documented legacy shaping. CLEAN removes added colour, retaining
> functional EQ/dynamics/limiting and a matched delay. Module order remains user-controlled.


Source of truth: `Source/Modules.h:288-335`.
Line numbers refer to commit `8151aa9`; re-check if the file moved.

`FetModule` is a **feed-forward, stereo-peak, fixed-threshold** compressor. The 1176's
defining trait is that the input knob *is* the threshold control — there is no threshold
parameter — and this module reproduces that faithfully.

---

## 1. Signal flow inside the module

Per block (`Source/Modules.h:303-331`):

```cpp
ratios[] = { 4, 8, 12, 20, 30 }                                 // :305
inG    = dbToGain (fet_input)          // 0..+30 dB                :307
outG   = dbToGain (fet_output)         // -20..+20 dB              :308
atkMs  = jmap (fet_attack,  1..7, 0.8 .. 0.02 ms)               // :309   INVERTED
relMs  = jmap (fet_release, 1..7, 1100 .. 50 ms)                // :310   INVERTED
ratio  = ratios[clamp((int) fet_ratio, 0, 4)]                   // :311
```

then, per sample:

```
for each ch: d[i] *= inG;  pk = max(pk, |d[i]|)                  // :318-323
env = smooth (env, gainReductionDb (gainToDb(pk), -10.0f, ratio, 2.0f), aC, rC)   // :324
for each ch: d[i] = fetColor (d[i] * dbToGain(-env) * outG)      // :326-328
```

Order is **input gain → detect → reduce → output gain → colour**. Colour is last, so the FET
nonlinearity always sees the finished level.

### The inverted attack/release knobs

`:309-310` map a *higher* knob number to a *faster* time:

| `fet_attack` knob | 1 | 3 | 5 | 7 |
| --- | --- | --- | --- | --- |
| attack | 0.80 ms | 0.41 ms | 0.23 ms | **0.02 ms** |

| `fet_release` knob | 1 | 3 | 4 | 7 |
| --- | --- | --- | --- | --- |
| release | 1100 ms | 705 ms | 575 ms | **50 ms** |

This matches the hardware's counter-clockwise-is-slower legend, but it means **the parameter
value is not the time**. Anything reading the parameter (automation lanes, presets, the GUI
label) must not assume `fet_release = 7` means 7 ms — it means 50 ms. Check the GUI label
against `:310` before trusting a "release" reading anywhere in the codebase.

---

## 2. Fixed threshold at −10 dB

`gainReductionDb (gainToDb(pk), -10.0f, ratio, 2.0f)` (`:324`) — the threshold is a literal
`-10 dBFS`, never a parameter. The 1176 trick is reproduced exactly: to compress more, turn
`fet_input` up. That raises `pk` into the fixed threshold and the compressor works harder.
`fet_output` then level-matches afterwards.

The **2 dB soft knee** (`:324`) is narrower than Opto's 10 dB, so the FET engages more
abruptly — correct for the FET character, and the main reason FET and Opto sound different at
similar amounts of gain reduction.

---

## 3. Attack is not an attack

`smooth (env, target, aC, rC)` (`:324`) is a one-pole with a **single coefficient chosen by
direction**: `aC` when the target is *above* the current state, `rC` when it is below
(`Source/Modules.h:21-25`).

There is no hold, no look-ahead, and no ratio-dependent program behaviour. The time shown in
the table above is a one-pole time constant, not a 10–90 % rise time, so the audible attack is
roughly 2.2× the quoted number. At `fet_attack = 7` the quoted 0.02 ms is **shorter than one
sample at 44.1 k** (0.0227 ms): the `exp(-1/(0.001·ms·sr))` coefficient underflows to ~0 and
the envelope follows the detector sample-for-sample. That extreme is deliberate (it is the
"all buttons in" region) but means the top of the attack range is effectively instant, not
"0.02 ms".

---

## 4. Colour stage (`fetColor`)

```cpp
a = |x|;
if (a < 0.30) return x * (1 - 0.06*x²);                     // :84
return sign * (0.30 + 0.70*tanh((a-0.30)/0.70));            // :86
```

Below 0.30 the stage is a **pure odd cubic** `x − 0.06x³` — no DC, no asymmetry, so it
generates only 3rd and higher odd harmonics. Above 0.30 it hands over to `tanh`. Both branches
are odd, so unlike `optoTubeColor` there is **no 2nd-harmonic component at all**: this is the
"aggressive odd-harmonic bite" the module is for.

The `0.30` breakpoint is far below the Opto's `0.55`, so the FET colour engages much earlier
in the signal — part of why FET sounds hotter at identical levels.

---

## 5. Ratio switch

`fet_ratio` is an `AudioParameterChoice` with values `{"4", "8", "12", "20", "30"}`
(`Source/PluginProcessor.cpp:55-56`) indexing `ratios[]` (`:305`). The clamp
`jlimit(0, 4, (int) value)` (`:311`) means an out-of-range stored value silently becomes
either 4:1 or 30:1.

The README calls the bank "4:1 / 8:1 / 12:1 / 20:1 / ALL". **"ALL" is `30:1`, not infinite**
(`:305`). Nothing in the DSP special-cases the top position.

---

## 6. Interaction with the rest of the rack

* **Zero latency.** No `getLatencySamples()` override, so this module contributes 0 to the
  reported rack latency. A feed-forward detector with instantaneous (sub-sample) attack means
  the module can *reduce* gain faster than it can see a transient — the first sample of a
  transient is already reduced, which is why FET does not overshoot at low attack settings.
* **No auto-gain** participation: `setChainReference` / `setFinalOutputReference` are not
  overridden; `gainCompDb` stays 0 (`:298`).
* **Upstream gain is the threshold.** Console `con_out` and the module's own `fet_input`
  both move the signal into the fixed −10 dB threshold, so Console's output trim is
  effectively a second input control when Console sits before FET (default order).
* **Downstream (Opto, Limiter) hear the FET colour.** In default order `[Console, Fet, Opto,
  Limiter]` the FET's 3rd-harmonic content is what the Opto's tube stage is fed.
* **RESET** zeroes `env` (`:296`) — the compressor forgets its gain reduction and snaps back
  to unity rather than releasing.

## Beta.7 operating reference

See [UA reference contract](../specs/ua-operating-reference.md): Console/FET use −18 dBFS, Opto/tape −12 dBFS. Fixed colour input/return calibration preserves upstream drive and all 24 rack orders. Digital thresholds/ceiling, Auto Gain, UNITY, parallel mix and constant latency retain their semantics. Opto/FET RESET manual gains are now explicitly 0 dB. Earlier calibration/reset statements on this page are historical. Physical capture calibration remains unverified.
