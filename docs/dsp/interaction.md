# Module interaction — what couples, what does not

Source of truth: `Source/Modules.h`, `Source/PluginProcessor.cpp`.
Line numbers refer to commit `8151aa9`; re-check if the files moved.

The four modules each document their own internals (`module-console.md`, `module-opto.md`,
`module-fet.md`, `module-limiter.md`). This page is about the **couplings between them**:
which parameter changes propagate, which stages share detectors, and which of those
couplings survive a re-order.

Throughout: default order is `[Console, Fet, Opto, Limiter]`
(`Source/PluginProcessor.cpp`, `getDefaultOrder()`), and it is a *permutation*, not a fixed
chain (`:165-166`).

---

## 1. The invariant layer — what is order-independent

Three things are deliberately built to be independent of the slot a module occupies. Know
these before chasing any "moving this changed the sound" report:

### 1.1 The master and the mix

```
for (id : getOrder()) modules[id]->run (buffer);     // rack
masterGain.applyGain (buffer);                        // always last
dry/wet blend                                         // always after the master
```

The master follows the **whole rack** and the blend follows the master
(`Source/PluginProcessor.cpp:173-218`). Neither can be dragged into the middle of the rack.
Re-ordering cannot change where master gain or the dry/wet mix sit.

### 1.2 Reported latency

`prepareToPlay` sums every module's `getLatencySamples()` and reports the total once
(`Source/PluginProcessor.cpp:107-113`). Only `LimiterModule` reports non-zero
(`la − 1`, `Source/Modules.h:431`), so the total is the limiter's look-ahead regardless of
order or bypass. The dry path is delayed by exactly that amount before the blend
(`:117-119`, `:196-216`), so partial mixes do not comb.

**Caveat:** Console's oversampling filters add real delay that is *not* reported
(see `module-console.md` §2.3). Latency is order-independent; the **true** latency of the
rack is always a little larger than the reported number.

### 1.3 Enhance Auto Gain

This is the most deliberately engineered coupling-breaker in the codebase. The limiter's
auto-gain divides a **chain input** measurement by a **true chain output** measurement, and
both are captured one block earlier and published as a **pair**
(`Source/PluginProcessor.cpp:316-323`, `publishAutoGainPair`):

```
chain input   — measured before ANY module runs          (:163, :293-296)
chain output  — measured after every module, BEFORE master gain and mix (:171, :304)
```

The limiter then divides these, never its own input/output
(`Source/Modules.h:500-513`). Two consequences:

* The correction is **identical no matter where the limiter sits** — it does not chase its
  own attenuation, and moving Console/FET/Opto gain cannot skew it.
* The pair is published one block late so both buffers describe **one signal**
  (`:297-323`). A one-block lag is inaudible for a 200 ms energy smoother
  (`energyCoef`, `Source/Modules.h:424`) but means auto-gain reacts after, not during, a
  sudden level change.

Correction is clamped to **±36 dB** and only applies when both energies exceed the noise floor
(`Source/Modules.h:510-513`). Below that it holds 0 dB rather than diverging.

---

## 2. The couplings that *do* propagate

Everything below is a real, order-sensitive interaction.

### 2.1 Absolute-level detectors: Console out → Fet → Opto

Both dynamic modules detect **absolute peak** and compare it against a **fixed threshold**:

| Module | Threshold source | Threshold value |
| --- | --- | --- |
| FET | `-10.0f` literal | −10 dBFS, never a parameter (`Modules.h:324`) |
| Opto | `-pr * 40` from `opto_peak` | −40 .. 0 dB (`Modules.h:258`) |

So **any upstream gain change is a threshold change**. Concretely, in default order:

```
con_out (−12..+12 dB)  and  fetal input (0..+30 dB)
          │
          └─► how hard FET compresses
                    │
                    └─► how hard Opto compresses (its detector sees the FET output)
```

Turn Console's output up 6 dB with FET in front of Opto and *both* compress more. Move Opto in
front of FET and the coupling reverses: Opto now sees Console's raw output and FET sees
Opto's makeup. **This is the single biggest reason re-ordering changes the sound.**

### 2.2 The colour stack

All three linear-path modules apply a memoryless nonlinearity, in this order along whatever
path they occupy:

| Module | Colour | Onset | Harmonic character |
| --- | --- | --- | --- |
| Console | `apiConsoleColor` | always on | asymmetric → even + odd (`Modules.h:59-64`) |
| Opto | `optoTubeColor` | \|x\| > 0.55 | asymmetric → 2nd + odd (`:68-76`) |
| FET | `fetColor` | \|x\| < 0.30 cubic | **purely odd** → 3rd (`:80-87`) |
| Limiter | `analogMasterSaturation` | `enh > 0` only | tape blend + exciter (`:91-99`) |

Each stage is level-dependent, so the colour stack **compounds**: an over-driven rack sounds
thicker than any single stage at the same amount of gain reduction, and the character depends
on which stages are driven before which. This is amplitude-order-dependent (nonlinear), so
`permute.cpp` cannot make it order-independent — see `reordering.md` for what was measurable.

Note the asymmetry: FET's colour starts *below* its own threshold-ish region (0.30) while
Opto's starts far above (0.55). FET therefore colours quiet material that Opto leaves clean.

### 2.3 The EQ before or after dynamics

Console's 10 bands sit *inside* its module, before its colour (`module-console.md` §1). So
Console's position relative to FET/Opto determines whether the EQ is applied **before**
detection (changing what the compressors see — real, audible) or **after** it (shaping the
final tone without changing the amount of compression).

A boost at 1 kHz before FET can measurably increase gain reduction; the same boost after
compression is purely tonal. Neither is "wrong", but presets are only transferable if the
order is the same.

### 2.4 Redundant dynamics

FET and Opto are both compressors. In default order they are **cascaded** (FET then Opto),
which is the classic "one for control, one for tone" arrangement. Either can be bypassed
independently (`Source/Modules.h:124-128`), and bypass zeroes that module's meter
(`:126`) but **does not reset its internal state** — a module re-enabled after a bypass
resumes with its envelope where it left off. That is a small but real pop hazard.

---

## 3. The final stage: UNITY

UNITY is the only thing that sits **after** the dry/wet mix, and the only thing that measures
the rack's *finished* output against its *raw input* at sample level
(`Source/PluginProcessor.cpp:220-273`):

```
inPower  = untouched dryBuffer (pre-rack)              :242
outPower = post-master, post-mix buffer                :243
         │
         └─ 0.25 s exponential energy (unityEnergyCoef, :130)
                │
                └─ targetDb = 10·log10(in/out), clamped ±18 dB   :258-260
                        │
                        └─ 50 ms smoother (juce::SmoothedValue) → per-sample gain :265-267
```

Points that matter for interaction:

* **It is not in the module chain.** UNITY cannot be re-ordered, and modules cannot see it.
* **It includes the master and the mix** by design — it levels the *output you'd hear*.
  Lowering `master_level` therefore does not defeat it; UNITY simply restores the level.
* **It holds through silence**: when either energy falls to the floor it targets 1.0
  (`:231-232`, `:262-263`) and the readout is forced to 0 (`:272-273`), so a quiet passage
  never gets boosted by a stale correction.
* **Meters are after UNITY** (`:275-276`), so the meter page shows what actually leaves.
* **It interacts with everything.** Because it closes the loop on total loudness, a sweep of
  FET input or Console output will be *audibly flattened* when UNITY is on — you hear tone
  change, not level change. Any level-matching test must switch UNITY off first.

---

## 4. RESET — the cross-cutting control

`resetRequested` is exchanged once per block; every module's `reset()` then runs **on the audio
thread**, followed by snapping the master smoother and clearing UNITY state
(`Source/PluginProcessor.cpp:140-148`).

Consequences across modules:

* All envelopes snap to zero reduction — no release tail.
* All limiter rings refill: delay lines to silence, request/avg rings to 1.0, `relGain` to 1
  (`Source/Modules.h:411-429`). The limiter's next `la` samples are therefore outputting
  what is now silence — a **short dropout immediately after RESET** is expected, not a bug.
* EQ integrators reset per band; colour stages are stateless and unaffected.
* Parameters are **not** touched by `reset()` — the "hardware neutral starting profile" is a
  separate UI action that writes APVTS values (`Module::reset` doc comment,
  `Source/Modules.h:117-120`).

---

## 5. Order-sensitivity summary

| Coupling | Sensitive to order? | Why |
| --- | --- | --- |
| Master gain / dry-wet mix | No | Always after the rack (`PluginProcessor.cpp:173-218`) |
| Reported latency | No | Summed once in `prepareToPlay` (`:107-113`) |
| Enhance auto-gain correction | No | Chain-input ÷ chain-output pair (`Modules.h:500-513`) |
| UNITY | No | Post-everything (`:220-273`) |
| Amount of FET / Opto compression | **Yes** | Absolute-peak detectors vs fixed thresholds |
| Harmonic character | **Yes** | Non-commuting nonlinearities |
| EQ-before vs EQ-after dynamics | **Yes** | Console's EQ is inside its module |
| Meter readings | Derived | Meters measure the true output, so they move with any coupling above |
