# Opto module (`OptoModule`) — LA-2A style optical leveller

Source of truth: `Source/Modules.h:239-286`.
Line numbers refer to commit `8151aa9`; re-check if the file moved.

`OptoModule` is a **feed-forward, stereo-peak-detecting** leveler with a two-stage
(program-dependent) release, a soft knee, a compress/limit switch, and a triode colour
stage. It is a model of the LA-2A's *behaviour*, not of its T4 cell.

---

## 1. Signal flow inside the module

Per block (`Source/Modules.h:252-282`):

```cpp
pr     = opto_peak * 0.01f                       // :255   0..1
makeup = dbToGain (opto_gain * 0.01f * 30.0f)    // :256   0..+30 dB
limit  = opto_mode > 0.5f                        // :257
thresh = -pr * 40.0f                             // :258   -40 .. 0 dB
ratio  = limit ? 12.0f : 3.0f                    // :259
```

then, per sample:

```
pk      = max over channels |x|                                :269
gr      = gainReductionDb (gainToDb(pk), thresh, ratio, 10.0f) :271   soft knee 10 dB
fast    = smooth (fast, gr,       aC, fC)                      :272   attack 10 ms, release 60 ms
slow    = smooth (slow, gr*0.6f,  aC, sC)                      :273   attack 10 ms, release 1800 ms
applied = max (fast, slow)                                     :274
y       = optoTubeColor (x * dbToGain(-applied) * makeup)      :277-279
```

The detector is **feed-forward**: it measures the input at the same sample it processes, not
the output. The gain is computed **before** the colour stage, so the tube runs on the already
gain-reduced signal — the tube nonlinearity therefore gets driven harder at low gain
reduction and barely at all when squashing, which is the opposite of a real LA-2A (where the
T4's cell sits in the gain path itself).

---

## 2. The peak-reduction law

`thresh = -40 · (opto_peak/100)` gives the LA-2A's inverted control: **more peak reduction =
lower threshold**, 0 → 0 dB (nothing) and 100 → −40 dB.

The ratio is a two-state switch, not a continuous control: `12:1` in limit mode, `3:1` in
compress (`:259`). Hardware LA-2A is roughly 3:1 with a knee; the 12:1 "limit" position is a
modelling decision, and the 10 dB soft knee (`kneeDb = 10`) spans `±5 dB` around the
threshold (`gainReductionDb`, `:35-39`), so the two modes differ by a gentle slope change, not
by a hard limiter.

**Critically, the `gr` value in `:271` is computed but the level `-10 dB` is a fixed
threshold in the FET module — not here.** Opto's threshold really is `-pr * 40`.

---

## 3. Program-dependent release (the two-stage model)

This is the module's defining behaviour and the part most worth understanding:

| Stage | Target | Attack | Release | Effect |
| --- | --- | --- | --- | --- |
| `fast` | `gr` (full) | 10 ms (`aC`) | 60 ms (`fC`) | tracks the instant demand |
| `slow` | `gr * 0.6` | 10 ms (`aC`) | 1800 ms (`sC`) | lags, holds a floor |

`applied = max(fast, slow)` (`:274`).

Three consequences fall out of that `max`:

1. **Attack is a lie for the slow stage, and harmless for the fast one.** Both stages share
   the 10 ms attack coefficient. When the signal gets louder, both rise together, so `fast`
   dominates (it targets the full `gr` while `slow` targets only 60 % of it).
2. **Release is genuinely two-speed.** `fast` drops quickly toward the (already low) demand;
   `slow` decays over 1.8 s but only needs to fall to 0.6·`gr`. The applied reduction
   therefore falls fast to ~60 % of peak reduction and then **crawls** the rest of the way.
   That plateau-then-crawl shape is the audible "opto memory".
3. **Sustained loud passages ratchet up.** Because `slow` only ever targets 0.6·`gr`, a long
   loud section converges to a *lower* steady-state reduction than the instantaneous demand.
   This is a model, and it is the single place where this module diverges most from the
   hardware's T4 cell.

Note the `max` runs per sample on a smoothed state, not on a held envelope; there is no
hold/release-delay stage.

---

## 4. Makeup gain mapping

`makeup = dbToGain (opto_gain * 0.01f * 30.0f)` (`:256`) — a linear 0..100 → 0..+30 dB map
applied **after** the gain reduction and **before** the colour stage.

Because it is pre-colour, makeup directly controls tube drive: turning the gain knob up on an
opto module that is also compressing changes the *character*, not just the level.

---

## 5. Stereo linking

The detector is `max` over channels (`:269`) and the gain `g` is applied identically to every
channel (`:277-279`). The module is therefore **fully stereo-linked**, with no balance
control and no per-channel detector. Mono input behaves identically.

Channel count is `buf.getNumChannels()` (`:254`), *not* clamped to 2 as in the other modules —
the DSP is per-sample scalar so this is safe, but it means a >2-channel host layout would be
levelled with a single shared gain across all channels.

---

## 6. Colour stage (`optoTubeColor`)

```cpp
tube = x + 0.075*(x² - |x|·x) - 0.025·x³      // :71
a = |tube|; if (a < 0.55) return tube          // :73
return sign·(0.55 + 0.45·tanh((a-0.55)/0.45))  // :75
```

The `0.075·(x² − |x|·x)` term is zero for positive `x` and `2·0.075·x²` for negative `x` —
i.e. **it breaks the waveform's symmetry in one direction only**, producing 2nd-harmonic
content. That is the "tube warmth". Everything below |0.55| passes untouched, so the stage is
transparent at normal levels and only bites on peaks — a deliberately late-onset soft clip.

`tanh` is bounded to `±1.0`, so the module output can never exceed full scale by itself.

---

## 7. Interaction with the rest of the rack

* **Zero reported latency.** `OptoModule` does not override `getLatencySamples()`, so it
  contributes 0 to the rack latency (`Source/Modules.h:122`, summed at
  `Source/PluginProcessor.cpp:111`). It is a feed-forward detector, so there is nothing to
  look ahead by.
* **No auto-gain.** `OptoModule` does not participate in the Enhance auto-gain contract
  (`setChainReference` / `setFinalOutputReference` are not overridden), so `gainCompDb` stays
  at whatever `reset()` wrote — 0 (`:247`).
* **Upstream gain drives it.** Peak reduction depends on the absolute peak at the module's
  input, so moving Console or FET's make-up ahead of Opto changes how hard it works. This is
  precisely why re-ordering matters — see `interaction.md`.
* **Downstream modules hear its colour.** The tube stage is inside the module, so anything
  after Opto (FET, Limiter) sees already-coloured signal. In the default order
  `[Console, Fet, Opto, Limiter]` the limiter is the only stage after it.
* **RESET** zeroes `fast` and `slow` (`:245`), so the leveller snaps back to no reduction
  instantly rather than gliding down — a deliberate consequence of
  `Source/PluginProcessor.cpp:142-148`.
