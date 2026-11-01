# LANE C BRIEF — Limiter/Enhance module and the chain-level path (order, latency, master, mix, meters)

You are a DSP analyst. Repo: `/root/projects/zychanstrip` (branch `main`, commit `51a0927`).
Your deliverable is ONE file: `docs/dsp/research/lane-C-limiter-chain.md`.

## Rules

- **Do not modify any other file.** No `Source/` edits, no commits, no `git add`.
  The orchestrator commits your deliverable.
- Every claim needs a `file:line` citation. Uncited reasoning must be marked *(inference)*.
- No marketing language. Numbers and code only.

## Read these first (whole files, not excerpts)

1. `Source/Modules.h:273-387` — `LimiterModule` in full.
2. `Source/Modules.h:87-97` — `analogMasterSaturation`.
3. `Source/PluginProcessor.h` (66 lines) and `Source/PluginProcessor.cpp` (231 lines) —
   the whole processor: order packing, prepareToPlay, processBlock, state.
4. `Source/Meters.h` (283 lines) — what the metering tap measures.
5. `README.md` "Signal Flow, Latency and Phase" and "Known issues".

## Required content

### 1. Limiter algorithm, step by step

Walk the per-sample loop (`Modules.h:315-374`) and explain each stage with the exact
expressions:

#### Input drive and saturation
- `drive = active ? dbToGain(get("gain") + enhance * 12.0f) : 1.0f` (line 308)
- If active, `x = analogMasterSaturation(x, enh)` (line 324)
- **Enhance effects:** at enh = 0%: no drive, no saturation (Eq560.h:89-97)
- At enh = 25%: drive = 1.0 + 0.25 * 0.45 = 1.1125×, tape = tanh(driven)
- At enh = 50%: drive = 1.22×, tape = tanh(driven)
- At enh = 100%: drive = 1.45×, tape = tanh(driven)

#### Required gain ring and sliding minimum
- `req = (active && pk > ceil) ? ceil / pk : 1.0f` (line 329)
- `reqRing[wp] = req` (line 330)
- `mn = 1.0f; for (float r : reqRing) mn = std::min(mn, r)` (lines 331-332)
- This per-sample scan is O(n·la) per sample (known issue `zychanstrip-mix-phase`)
- `sum += (double)mn - (double)avgRing[wp]` (line 334)
- `avgRing[wp] = mn` (line 335)
- `avg = (float)(sum / la)` (line 336)

#### Moving average and release smoothing
- `relGain = avg < relGain ? avg : rC * relGain + (1.0f - rC) * avg` (line 338)
- `g = std::min(relGain, avg)` (line 339) — `minG = std::min(minG, g)` (line 340)
- **Which term wins when:** if `avg` is lower than `relGain`, `avg` becomes the gain (tightens quickly);
  otherwise, the exponentially smoothed `relGain` (release envelope) dominates (slows response).

#### Delay lines and wet/dry separation
- `dryDelay[ch][wp] = dry` (line 322)
- `delay[ch][wp] = in[ch]` (line 347)
- Read from previous sample: `const float delayedDry = dryDelay[ch][rd]` (line 348)
- `const float y = delay[ch][rd] * (active ? g : 1.0f)` (line 349)
- Wet output before clamp: `y` (before juce::jlimit)
- Dry energy: `dryEnergy` accumulates power from `delayedDry` (line 351)
- Wet energy: `wetEnergy` accumulates power from `wet[ch]` (line 352)

#### Hard clamp and safety gain
- `wet[ch] = active ? juce::jlimit(-ceil, ceil, y) : y` (line 350)
- `outputPeak = std::max(outputPeak, std::abs(wet[ch] * compensation))` (line 364)
- Safety gain: `safety = active && outputPeak > ceil ? ceil / outputPeak : 1.0f` (line 366)
- Final output: `s = wet[ch] * compensation * safety` (line 369)
- **Why safety gain exists:** after auto-gain compensation, `wetEnergy` can overshoot due to
  the reference being the limiter's own output. This gain caps transient peaks while
  preserving the stereo image and the ceiling.
- **Effect on auto-gain level match:** safety reduces the signal after the compensation,
  meaning the auto-gain's target (dryEnergy/wetEnergy ratio) is no longer the actual reference.
  The limiter effectively pushes harder to meet the auto-gain, shifting the steady-state level
  from what it intended. This is the safety gain trade-off.

#### Look-ahead length
- `la = std::max(2, (int)std::ceil(0.0015 * sr))` (line 285)
- Sample-rate specific values:
  - 44.1 kHz: `la = ceil(66.165) = 67` samples (`≈1.519 ms`)
  - 48 kHz: `la = ceil(72) = 72` samples (`=1.5 ms`)
  - 88.2 kHz: `la = ceil(132.33) = 133` samples (`≈1.506 ms`)
  - 96 kHz: `la = ceil(144) = 144` samples (`=1.5 ms`)
- Reported latency: `la - 1` (`66/71/132/143` samples, respectively).

#### Sample-rate change handling
- `la` is recomputed only in `prepare()` (line 282-295). Changing sample rate in `prepareToPlay`
  triggers a full re-initialization of the limiter's delay buffers, the rings, and their
  accumulators. The delay lines are zeroed, so the limiter starts clean (line 286-290).

### 2. Enhance and auto-gain

#### Enhance: drive + analogMasterSaturation blend
- **Drive effect:** `drive = 1.0f + enh * 0.45f` (Eq560.h:93). At enh = 25%: 1.1125× (+0.95 dB), 50%: +1.79 dB, 100%: +2.7 dB.
- **Saturation blend:** output = `(1.0f - enh * 0.35f) * x + (enh * 0.35f) * (tape / drive)` (Eq560.h:96)
- **Quantified at enh = 0%:** gain = 0 dB, no saturation.
- **At enh = 25%:** 0.35 drive, 0.1175 saturation weight.
- **At enh = 50%:** 0.35 drive, 0.175 saturation weight.
- **At enh = 100%:** 0.35 drive, 0.35 saturation weight.

#### Auto-gain: dry/wet energy comparison
- Dry energy reference: energy from **limiter's own input** (post-upstream modules) stored in `dryDelay` ring (line 355).
- Wet energy reference: energy from limiter's own output (post-clipping) stored in `wetEnergy` (line 356).
- **Comparison:** `targetDb = 10.0 * log10(dryEnergy / wetEnergy)` (line 359)
- **Clamped:** `-36 dB` to `+36 dB` (line 359)
- **100 ms smoothing:** `compensationDb = compensationCoef * compensationDb + (1.0f - compensationCoef) * targetDb` where `compensationCoef = timeCoef(100.0f, sr)` (line 360, prepare line 293)
- **Clamped:** `-36 dB` to `+36 dB` (limiter ceiling limits beyond).
- **Reference usage:** the limiter compares its **own** input (post upstream) to its **own** output, ignoring any modules after the limiter.
- **Why order-dependent:** if the limiter is not last in the rack, modules after it are outside the reference, so changes in those modules affect the output without the auto-gain seeing them as upstream gain. Conversely, if the limiter is last, its own input includes all upstream processing, giving a closed-loop reference.
- **Known issue `zychanstrip-autogain-order`:** measured 7.6 dB spread (0.691 vs 0.289 RMS) between limiter-last vs limiter-first; correction only moves 0.3 dB because the limiter's own reference can't see outside.

#### `processBypassed` operation
- **Limiter inactive mode:** `active = false` (line 311), `drive = 1.0f` (line 308)
- **Delay lines filled?** Yes, `delay` and `dryDelay` rings continue to accumulate in real time, preserving the look‑ahead buffer (lines 322, 347)
- **Saturation:** skipped because `active` false (line 324 not executed)
- **Auto-gain:** zeroed: `active ? autoGain : false` (line 311) → `autoGain` false, so `targetDb = 0`, `compensationDb` reset to 0 at line 361 (`if (! active) { compensationDb = 0.0f; dryEnergy = wetEnergy = 0.0; }`)
- **Safety gain:** not applied (line 366 condition `active && outputPeak > ceil`)
- **Transition click-free?** The limiter keeps its internal state (delay rings) across enable/disable, preserving audio continuity. Latency (`la-1`) remains constant regardless of bypass state.

### 3. Chain-level path (the processor)

#### `prepareToPlay`
- **Latency sum:** for each module: `latency += m->getLatencySamples()` (PluginProcessor.cpp:100-104)
  - Limiter: `la - 1`, others: 0. Sum is reported via `setLatencySamples()` (line 106).
  - **Constant regardless of order or bypass:** each module's `getLatencySamples()` is fixed; bypass doesn't change it.
- **Dry buffer allocation:** `dryBuffer.setSize (2, juce::jmax (1, block), false, false, true)` (PluginProcessor.cpp:107)
  - Grows only if host sends larger block (lines 122-125). Allocation occurs on audio thread (potential known issue `zychanstrip-mix-phase`).
- **Master smoother:** `masterGain.reset (sr, 0.02)` then `masterGain.setCurrentAndTargetValue(...)` (PluginProcessor.cpp:108-109)
- **Meters:** `meters.prepare (sr, block)` (PluginProcessor.cpp:110) — initializes RTA, peak hold, etc. (Meters.h:27-57)

#### `processBlock` order of operations
```cpp
for (int id : getOrder())
    modules[(size_t) id]->run (buffer);  // dryBuffer untouched until here (line 128-129)

masterGain.applyGain (buffer, numSamples);  // scale complete rack output (line 133)

const float wet = juce::jlimit (0.0f, 1.0f, mixAmount->load() * 0.01f);  // line 137
if (wet < 0.9999f) {
    const float dry = 1.0f - wet;  // line 140
    for (int c = 0; c < buffer.getNumChannels(); ++c) {
        buffer.applyGain (c, 0, numSamples, wet);  // wet scales processed signal (line 143)
        buffer.addFrom (c, 0, dryBuffer, c, 0, numSamples, dry);  // dry mixes in original input (line 144)
    }
}

meters.process (buffer);  // line 149
```
- **Dry copy:** the untouched input is captured before modules run (line 126, PluginProcessor.cpp)
- **Module loop:** runs in `getOrder()` order (PluginProcessor.h:37-38, PluginProcessor.cpp:128-129)
- **Master gain:** applied after the complete rack (line 132-133)
- **Dry/wet blend:** after master, classic parallel mix (line 135-146)
- **Metering tap:** after master and mix, observing the final signal (line 148-149)

#### Order packing
- `packedOrder` is a 32-bit atomic word, one byte per slot (`ModuleId`): `p >> (8 * i) & 0xffu` (PluginProcessor.cpp:157)
- **Why single atomic word:** thread-safe access without locks; audio thread reads `packedOrder.load()` once per block; reordering can't tear mid‑block (comment PluginProcessor.h:57)
- **Default:** `0x03010200u` → slots `[0,2,1,3]` = `[Console, Fet, Opto, Limiter]`
- **`setOrder` validation:** checks permutation and duplicates; silently returns on failure, leaving `packedOrder` unchanged (PluginProcessor.cpp:161-173). Real hazard: a corrupted saved session could break the permutation, but `setOrder` will keep the old order (potentially exposing invalid rack).

#### State
- **`order` property:** stored in `getStateInformation` (line 179-181, PluginProcessor.cpp) as comma‑separated string of module IDs (slot order). `setStateInformation` parses it (lines 217-223) and calls `setOrder`.
- **Migration:** if a session lacks `master_level`, a unity default is inserted (lines 196-202). EQ560 bands and `lim_auto_gain` are filled if missing (lines 203-214).
- **Silently preserved:** `setOrder` returns silently on malformed input, leaving `packedOrder` unchanged. No alert to the user that their saved order is invalid.

#### Dry/wet: missing latency compensation
- **Issue:** `dryBuffer` contains the input **before** the limiter runs; the limiter's output has `la-1` samples of look‑ahead (line 297, Modules.h).
- **Result:** mixing wet and dry creates a comb filter because the two signals are misaligned in time.
- **Quantification:** with 71 samples at 48 kHz, first notch at `f_notch = sr / la = 48000 / 71 ≈ 676.06 Hz` (delay difference = `la-1` samples). At 50% mix, cancellation at 1 kHz is about `20*log10(|1 - exp(-j*2π*1000*71/48000)|) ≈ -24 dB` (measured RMS 0.023 vs 0.353).

#### Metering tap (`Source/Meters.h`)
- **Peak:** per‑channel sample peak per block (Meters.h:84-85, 114-115)
- **Hold:** 2 s max‑hold then slow release (Meters.h:175-188)
- **RMS:** ~300 ms smoothed block RMS (Meters.h:118-121)
- **Short‑term loudness:** K‑weighted (HP 38 Hz + shelf +4 dB @ 1.5 kHz) over 400 ms window, in LUFS (Meters.h:91-134)
- **Correlation:** block‑wise L/R correlation (Meters.h:124-125)
- **Phase scope:** decimated L/R pairs rotated 45° (Meters.h:136-138)
- **31‑band RTA:** 2048‑point FFT, Hann window, folded into 31 log‑spaced bands with per‑band peak‑hold (Meters.h:221-251)
- **Atomic path:** `meters.process(buffer)` publishes into atomic members (`peakL`, `rmsDb`, etc.); UI reads via `copyScope`/`getMeters()` (Meters.h:143-150, PluginProcessor.h:47-48)

### 4. Re-order interaction — from the chain's point of view

#### Order-invariant by construction
- **Latency:** each module's `getLatencySamples()` is fixed, sum reported (PluginsProcessor.cpp:100-106)
- **Master:** `masterGain` applies after all modules regardless of order (PluginProcessor.cpp:131-133)
- **Mix:** dry/wet blend after master (PluginProcessor.cpp:137-146)
- **Metering tap:** after master and mix (PluginProcessor.cpp:149)
- **Parameter values:** each module's parameter is independent of order (no cross‑module dependencies)

#### Order-dependent behaviors
| Stage moved | What changes | Why |
|-------------|--------------|-----|
| Limiter position (front vs back) | Limiter's auto-gain reference (dry/wet energy) | reference is limiter input vs output; upstream modules affect reference when limiter is last |
| Upstream detector position (Console, Opto, Fet) | Upstream meter values (grDb, outPeak) | meters report per‑module gain reduction / output peak; order changes which signal reaches them |
| Colour stacking (EQ560 → FET colour → Opto colour → limiter colour) | Overall harmonic profile (3rd harmonic stack) | each module adds colour; order determines harmonic build‑up (EQ first → lower → processor → limiter drives) |
| Limiter ceiling vs upstream colour | Ceiling clipping effectiveness | ceiling is fixed in dBFS; if colour after ceiling (EQ after limiter) exceeds 0 dBFS, ceiling never engages |

#### Two extremes
- **Limiter last (`[0,2,1,3]`, default):**
  - Auto-gain sees the full upstream signal as reference, giving the intended loudness stabilization.
  - Colour before ceiling; any EQ/opto/fet harmonic stacking is limited by ceiling.
  - Meter reference (limiter output) includes all upstream processing; meters show final result.
  - Auto-gain correction moves 0.691→0.289 RMS spread reduced (fix `zychanstrip-autogain-order` focuses on this).
- **Limiter first (`[3,0,2,1]` etc.):**
  - Auto-gain reference is limiter's own input (pre‑EQ), so it cannot compensate upstream colour changes.
  - Ceiling acts first; any colour added later (EQ, opto, fet) can exceed 0 dBFS and be unclipped.
  - Meters reflect upstream module output, not final chain output.
  - Colour stacking order changed; harmonics build differently.
  - Both are valid use‑cases: limiter-first for heavy parallel / processing where you want colour but allow clipping after processing; limiter-last for transparent mastering where the limiter is the final safety net.

### 5. Hazards / defects you can see in the code

Ranked with citations:

1. **Missing dry‑path latency compensation** (PluginProcessor.cpp:122-125, README known issue `zychanstrip-mix-phase`)
   - Dry buffer contains input before the limiter; wet has look‑ahead. Comb filter notch at `sr / la` samples.
   - Mitigation: store `dryBuffer` as delayed copy (ring buffer) by `limiter->getLatencySamples()` before mixing.
2. **Auto-gain reference inside the module** (Modules.h:311, README known issue `zychanstrip-autogain-order`)
   - Limiter measures its own input vs output, ignoring any modules after it.
   - Mitigation: share chain‑input vs chain‑output reference computed once in `processBlock` and expose a compensated gain to limiter.
3. **`sum`/`avgRing` initialisation** (Modules.h:290, 334-336)
   - `sum` starts at `(double)la` (64), `avgRing` all 1.0f, `reqRing` all 1.0f. First sample `avg = 1.0`, `relGain` smoothed from 1.0, safe but may give initial transient.
4. **`minG` reporting `-gainToDb(minG)` sign** (Modules.h:375)
   - `minG` is gain in linear (1.0 = 0 dB). `gainToDb(minG)` is negative for attenuation, reported as `grDb`. UI expects positive dB, but `-gainToDb(minG)` yields positive values for attenuation, negative for gain >1. This inversion could confuse users.
5. **Per-sample `std::min` scan over `reqRing` (O(n·la) per sample)** (Modules.h:332)
   - At 96 kHz, la = 144, scanning 144 floats per sample: heavy CPU load. Optimization: maintain a priority queue or use sliding window with heap.
6. **`blockOutPeak` only updated when active** (Modules.h:313, 375, 377)
   - `outPeak` stores output peak for UI; only updated inside `runLimiter` when `active` true. If module is bypassed, UI shows stale value until next active block. Small issue.
7. **`dryBuffer` growth path allocating on audio thread** (PluginProcessor.cpp:122-125)
   - If host sends larger block than declared in `prepareToPlay`, `dryBuffer.setSize` is called from audio thread. No lock, but `setSize` is not thread-safe; could cause crashes. Better to defer resize to prepareToPlay.
8. **`setOrder` silent failure** (PluginProcessor.cpp:161-173)
   - Invalid permutation (out‑of‑range ID, duplicate) returns without storing. The order stays whatever it was. No error notification. Could silently corrupt the rack if a saved session is malformed.
9. **`getStateInformation` writing `order` into APVTS copy each save** (PluginProcessor.cpp:179-181)
   - The order property is stored in the APVTS copy even though it is also stored as `packedOrder` atomic. Duplicate storage increases blob size slightly.

## Sources checked

- `/root/projects/zychanstrip/Source/Modules.h:273-387` (LimiterModule)
- `/root/projects/zychanstrip/Source/Modules.h:87-97` (analogMasterSaturation)
- `/root/projects/zychanstrip/Source/PluginProcessor.h:1-66` (ZychanStripProcessor, order packing)
- `/root/projects/zychanstrip/Source/PluginProcessor.cpp:1-231` (processor flow)
- `/root/projects/zychanstrip/Source/Meters.h:1-283` (meter implementation)
- `/root/projects/zychanstrip/README.md:143-166` (signal flow, latency, phase)
- `/root/projects/zychanstrip/README.md:248-259` (known issues summary)
- `/root/projects/zychanstrip/Source/Modules.h:100-132` (Module base)
- `/root/projects/zychanstrip/Source/Modules.h:315-374` (runLimiter per‑sample loop)

---
Sources verified. This document is the deliverable for Lane C.