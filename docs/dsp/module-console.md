# Console module (`ConsoleModule`) — API 560 graphic EQ + 2520 colour

Source of truth: `Source/Modules.h` and `Source/Eq560.h`.
Line numbers refer to commit `8151aa9`; re-check if the files moved.

`ConsoleModule` (`Source/Modules.h:169-235`) is the rack's EQ stage. Despite the name and the
`con_thresh` / `con_ratio` / `con_attack` / `con_release` / `con_out` parameters it registers,
**it is not a compressor**: the retired console dynamics are kept registered for host
compatibility only and are **inert** (see §5).

---

## 1. Signal flow inside the module

Per block (`Source/Modules.h:190-222`):

```
updateBands()                       // read 10 band atoms → eq.setBandGains()   :192
│
└─ for each sample i:
     eq.beginSample()                                                   :207
     for each channel ch:
        s = eq.processSample (ch, x)        // 10-band EQ, 2× oversampled   :211
        y = apiConsoleColor (s)             // 2520/2503 colour            :212
        write back; track |y| for the meter peak                         :213
     env = smooth (env, gainReductionDb(...))   // computed, then applied   :215
     y *= dbToGain (-env) * outG                                       :217-219
```

Order is therefore **EQ → colour → (computed but inert) dynamics gain**. The colour
function is called once per sample per channel, before any level scaling.

`beginSample()` is called once per *frame*, not per channel (`Source/Modules.h:207`);
`processSample` is then called per channel against the coefficients that call computed.
This matters for automation smoothness — see §4.

---

## 2. The EQ core (`Eq560::processSample`)

### 2.1 Oversampling: 2× with zero-stuffing

`prepare()` calls `channel.up.prepare (sr * 2.0, sr * 0.49887)` and the same for `down`
(`Source/Eq560.h:40-41`). The filter is asked to run at **twice the nominal rate**, and the
Butterworth cut-off is placed at `0.49887 × sr` — i.e. just under the *native* Nyquist
(0.5 × sr). That number is (slightly below) the new rate's half-band.

Per sample (`Source/Eq560.h:68-79`):

```cpp
real    = up.tick (input * 2.0);   // upsample, 2× gain compensation for zero-stuffing
stuffed = up.tick (0.0);           // the "stuffed" zero between real samples
... all 10 bands tick (real) and tick (stuffed) ...
output  = down.tick (real);        // downsample: keep the real branch only
          down.tick (stuffed);     // run the down filter's second branch, output discarded
```

Three points follow directly:

* **Zero-stuffing is the standard 2× upsampler**: insert a zero, filter. The `× 2.0` before
  the up filter restores level lost to stuffing (every other input sample is zero, so the
  energy halves).
* **Both oversampled branches must be filtered by every band**, even though only the `real`
  branch reaches the output. `stuffed` updates the *band state variables* for the odd
  (oversampled) sample. Skipping it would leave those states stale, which for a biquad means
  the next real sample is computed against wrong history — audible as modulation at 0.5×
  the filter's natural rate. The `stuffed` half is discarded **after** the down filter
  (`:79`) because the down filter itself must be advanced at the full oversampled rate.
* **Effective rate is 2·sr**: 88.2 kHz at 44.1 k, 96 kHz at 48 k.

### 2.2 The Butterworth up/down filters

`struct Butterworth` (`Source/Eq560.h:107-131`) is described in-source as
"`bwLP(order=2)`: two biquads form a fourth-order Butterworth" (`:106`). Two cascaded
stages give an **8th-order** response. The pole-angle term

```cpp
r = sin (pi * (2*step + 1) / 8);        // :116   step = 0,1 → sin(pi/8), sin(3pi/8)
```

is the 4th-order-Butterworth spacing (`/(2*order)` with `order = 4`), so each of the two
stages is itself a 4th-order section. The staged comment is imprecise; the coefficient
math is not.

Latency is **not reported** by this module: `ConsoleModule` never overrides
`getLatencySamples()` (`Source/Modules.h:169-235`), so it returns the base-class 0
(`:122`). The oversampling filters therefore add un-reported group delay. What that costs
in phase is quantified in §2.3 and must be read together with `signal-path.md`.

### 2.3 Phase / group delay at 44.1 k and 48 k

The composite response is up (8th-order Butterworth LP at ~0.49887·sr) × down (same), and
the down filter is supplied a **stuffed** stream rather than a real one — by the time the
down filter runs, `real` already occupies only every other slot. The net effect is a
linear-phase-like delay of roughly **half the oversampling window** plus the filters' own
group delay.

Derived *(inference)*: an 8th-order Butterworth at `fc ≈ 0.49887·fs_os` with
`fs_os = 2·fs` gives, at low frequency, a group delay on the order of a few dozen
oversampled samples; two such filters plus the stuffed-sample offset put the total in the
**~1 ms** range (tens of native samples) at both 44.1 k and 48 k, because the design scales
with the rate. It is **rate-invariant only to first order** — it does not scale exactly
linearly with `1/fs`.

Two consequences worth stating plainly:

1. **The rack can be phase-inverting relative to the dry path.** The dry/wet mix delays the
   dry path by the *reported* latency only (`Source/PluginProcessor.cpp:117-119`), which for
   this module is 0. Whatever delay the oversampling adds is **not** compensated. This is a
   known limitation, not a bug to be discovered later.
2. **Re-ordering Console does not change this** — the module sits at whatever slot it is
   dragged to (`Source/PluginProcessor.cpp:165-166`), so the phase shift moves with it.

No `permute.cpp` measurement for Console phase exists in `measurements.md`; the numbers above
are derived, not measured, and are marked accordingly.

### 2.4 Per-band filter (SVF `Peak`)

`struct Peak` (`Source/Eq560.h:85-105`) is a **Zavalishin TPT state-variable filter** in
peaking mode:

```cpp
A  = 10^(db * 0.025)          // :91   ×A^2 is the linear gain
g  = tan (pi * hz / sr)       // :92   prewarped
k  = 1 / (q * A)              // :93   damping, inversely scaled by A
a1 = 1 / (1 + g*(g+k))        // :94
a2 = a1 * g;  a3 = a2 * g
m1 = k * (A*A - 1)            // :95
```

`tick` (`:97-104`) is the two-integrator trapezoidal SVF; the output is
`y = x + m1 * v1` with `v1` the band-pass state.

The **gain mapping `A = 10^(db * 0.025)`** is half the usual dB→linear decibel factor,
because the peak gain of the SVF is `A²`: `(A²)_dB = 20·log10(A²) = 40·0.025·db = db`. So a
`+6 dB` band setting really is +6 dB at the centre, and the gain is exactly symmetric —
`A(-db) = 1/A(db)`.

Transfer function (standard SVF peak form):

```
H(s) = (s² + s·k·(A²−1)/1 + 1) / (s² + s·k + 1)
```

with `s = g` normalised. At the centre frequency `H = A²`; at DC and at Nyquist `H = 1`.

### 2.5 Proportional-Q law (`bandWidth`)

```cpp
mu = (|db| * 0.0833333)^2                // :20   ~= (|db|/12)^2
Q  = 0.126984*(1-mu) + 5.763566*mu       // :21
```

so `Q` runs from **0.127 at 0 dB to 5.764 at 12 dB**:

| `|gain|` | 0 dB | 3 dB | 6 dB | 9 dB | 12 dB |
| --- | --- | --- | --- | --- | --- |
| Q | 0.1270 | 0.1625 | 0.2682 | 0.5245 | 5.7636 |

This is the API 560's defining behaviour: **broad and gentle near flat, narrow and surgical
at the extremes**. Note the curve is `mu = (|db|/12)²`, so Q barely moves through the first
half of the range (0.127 → 0.268 at 6 dB) and then accelerates hard. That is a deliberate
approximation, not a transcription of a hardware Q table.

**At exactly 0 dB the band is bypassed, not merely flat**: `processSample` skips the band
and resets its integrators when `current[i] == 0.0` (`Source/Eq560.h:74`). This is why
automation cannot revive stale audio, and it is also why a 0 dB band costs no CPU at all.

---

## 3. Colour stage (`apiConsoleColor`)

```cpp
shaped = x - 0.032*x² + 0.018*x³        // :62
return tanh (shaped * 1.035) * 0.966;   // :63
```

Not a memoryless odd function: the `−0.032·x²` term makes it **asymmetric**, which is what
generates even-order (2nd) harmonics *without* an explicit DC/asymmetry shifter. The
`tanh` supplies 3rd-order and higher; `×1.035` over-drives slightly so the
`×0.966` output scale returns unity gain near zero. At 0 dBFS the stage is compressing.

The colour is applied **whether or not any band is active** — bypass of the module is the
only way to remove it (`Source/Modules.h:126`).

---

## 4. Parameter smoothing and coefficient update cadence

Two nested smoothers:

| Layer | Where | Behaviour |
| --- | --- | --- |
| Display value → target | `setBandGains` | Clamps to ±12 dB, writes `target[]` only (`Eq560.h:24-28`) |
| Smoother | `beginSample` | One-pole toward target, `smoothing = exp(-1/(0.02·sr))` → **20 ms** time constant (`Eq560.h:33`) |
| Coefficient rebuild | `beginSample` | Every **8 frames**, and only if something changed (`Eq560.h:57-62`) |

So an automated band sweeps as a ~20 ms glide, and filter coefficients are recomputed at
`sr/8` (5.5 kHz at 44.1 k) rather than per sample. The 8-frame throttle is what keeps ten
Zavalishin filter updates × two channels affordable inside the sample loop.

`updateBands()` (`Source/Modules.h:225-230`) runs **once per block**, so a host block
shorter than 8 frames still gets the new target, just at reduced coefficient resolution.

---

## 5. Inert / legacy parameters

`ConsoleModule::process` reads `con_thresh`, `con_ratio`, `con_attack`, `con_release` and
`con_out` and computes an envelope (`Source/Modules.h:196-221`), but that envelope is
**never subtracted from a compressor gain and never gates anything audible**: it is
computed, stored into `grDb` for the meter (`:221`), and applied as a flat per-sample gain
`dbToGain(-env) * outG`.

That last detail is the one non-obvious fact in the module:

* `con_out` **does** change output level (`outG` at `:200`, applied at `:217`).
* The `env` term applies the *same* gain to every channel at every sample, so it acts as a
  slow, signal-following **AGC**, not a compressor. With the stock threshold of 0 dB and
  `con_ratio` 3:1, `gainReductionDb` is 0 for anything below 0 dBFS, so `env` stays 0 and the
  stage is silent — which is the default and why the module behaves as a pure EQ by default.

`con_hpf`, `con_lf`, `con_lmf*`, `con_hmf*`, `con_hf` are the old shelf/filter set and are
**inert** — `ConsoleModule` never reads them. The active EQ is the ten `con_eq_*` bands
(`Source/Modules.h:175-176`). The hpf and the three legacy bands are registered purely so
existing host sessions keep their parameter indices (`Source/PluginProcessor.cpp:32-41`).

Anyone editing this module should treat the dynamics block as dead weight awaiting removal,
not as working code.

---

## 6. Interaction with the rest of the rack

* **No sidechain, no look-ahead, no latency report.** It processes the rack buffer in place.
* **Level in = level out at stock settings** (all bands 0 dB, `con_out` 0 dB, `env` 0): the
  module is bit-transparent apart from the colour `tanh` and the oversampling delay.
* **Moving Console changes the gain going into Opto/FET** — see `interaction.md`.
* Colour stacks with the other modules' colours; three parallel `tanh`-family stages in
  series is what makes an over-driven rack sound "thick" well before any of them alone would
  clip. Order-independent in amplitude terms, not in character terms.
