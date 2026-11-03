PRIOR-ART: zychanstrip DSP optimisation | tier 2 | 2026-11-03 | queries: 11 GitHub API + 7 upstream source reads
Source tree audited: Gitea `zack/zychanstrip` @ a0b1f25 (main), read-only clone.
Status: REPORT ONLY — nothing built, installed, committed or pushed.

Regime: prior-art-research — prior-art researcher active.
Question: which parts of the zychanstrip audio path should be optimised, and what is the
strongest published evidence for each optimisation.

---

## 1. Our side — where the cost actually is

Measured/derived from the source (static analysis; no profiler run yet):

| # | Element | File:line | Why it is expensive |
| --- | --- | --- | --- |
| O1 | **Per-sample libm transcendentals in the compressors and the limiter.** Console, Opto and FET each call `gainToDb` (`std::log10`) and `dbToGain` (`std::pow`) inside the sample loop; the Limiter adds `std::log10` for auto-gain and `dbToGain(compensationDb)` in its loop. | Console `Modules.h:215,217` · Opto `:271,277` · FET `:324,326` · Limiter `:512,515` (and no per-sample `gainToDb` in the Limiter) | `pow`/`log10`/`tanh` are libm calls (~20–100 ns each on arm64). Measured from source: **3 per channel-sample** in each compressor, 2–3 in the limiter. A full chain is therefore **5–6 transcendental calls per channel-sample** — for numbers that only change when the detector state changes. |
| O2 | **`analogMasterSaturation` is a pure `tanh`** and it is *not* level-limited, so every non-zero Enhancement value runs `tanh` on both channels every sample. Default Enhance = 0, so this cost is opt-in. | `Source/Modules.h:91-99` | The `enh <= 0` early-out is the only guard. Note `softClip` (`:44-50`) is **dead code — zero call sites**, so it is not worth optimising. |
| O3 | **EQ560 runs a full 10-band stack twice** (real + stuffed path) with a 4th-order Butterworth up and down per channel, in **double precision**, at **2× the sample rate**. | `Source/Eq560.h:65-81,121-131` | Counted from source: 24 biquad ticks per channel per native sample (2 up + 20 band + 2 down) × 2 channels × 2× oversampling = **96 biquad evaluations per input sample** (0-gain bands are skipped at `:74`, so the real cost is lower). Still the largest arithmetic block in the plugin. |
| O4 | **`ConsoleModule::updateBands()` runs every block and writes 10 `double`s into `target`** even when nothing changed. | `Source/Modules.h:225-230` | Cheap, but it feeds `Eq560::beginSample()`'s per-sample 10-element smoothing loop (`Eq560.h:50-56`) that also runs when static. |
| O5 | **Every parameter is read by *string*** on the audio thread: `get()` builds `prefix + name` (heap `juce::String` concat) and does a hash lookup in the APVTS map. | `Source/Modules.h:156-159` — 20 call sites across `:126,196-200,255-257,307-311,453-456` | 4–5 string constructions + map lookups **per block per module**. Small, but in the audio callback and trivially avoidable. |
| O6 | **`RackMeters::process` walks the block three times** (peak/RMS/correlation/FFT feed, then a second RMS pass, then `scopeWrite`), `scopeWrite` takes a `SpinLock`, `clear()`s and `push_back`s on the audio thread, and **`runFft()` (2048-point FFT + 31-band fold) runs inline in the callback**. | `Source/Meters.h:79-141,207-219,221-252` | Lock + un-reserved `push_back` + an FFT in the callback is the biggest real-time-safety smell in the plugin. |
| O7 | **Every module boundary goes through `getSample`/`setSample` accessors**, whose per-sample index arithmetic blocks LLVM from vectorising the loops. The **dry buffer is also copied every block even at 100 % wet**, although the dry *path* is correctly skipped (`PluginProcessor.cpp:180`). | `Source/Modules.h:211-219,269-279,320-328,467-472,492-525`; `Source/PluginProcessor.cpp:155-159` | The copy is real work that produces a buffer nothing reads when `wet >= 0.9999f` and UNITY is off (UNITY still reads `dryBuffer` at `:242`). |
| O8 | **UNITY does a per-sample `log10`** for the readout (`gainReadout`) even though the target only changes per block. | `Source/PluginProcessor.cpp:269` | Minor. |
| O9 | **`publishAutoGainPair` copies two vectors every block** with `assign(begin,end)`. | `Source/PluginProcessor.cpp:286-287` | Pre-sized in `prepareToPlay` (`:123-124`), so this is a **memmove, not a reallocation**, in the normal path; only the oversized-block path allocates. Low priority. |

**Already good** — keep it: the sliding-minimum limiter was already rewritten to amortised O(1)
(`Modules.h:342-405`, memory 24877, exact-output tested against the naive ring). Do not touch.

---

## 2. Findings — one row per design element

### F1 · `tanh` and other saturators
- **How the best sources do it.** Airwindows ships its clips/limiters as **pure arithmetic with no
  libm at all** — FinalClip's inner loop is bounded adds, compares and multiplies over a small tap
  buffer (`plugins/MacAU/FinalClip/FinalClip.cpp:185-210`, MIT). It does not use `tanh`; it uses a
  golden-ratio slew-limited clip.
- **JUCE** itself is the counter-example worth citing: its `SpecialFunctions::besselI0` is a
  **rational polynomial minimax approximation** (Abramowitz & Stegun 9.8.1/9.8.2) with only `exp`
  and `sqrt` on the far branch (`modules/juce_dsp/maths/juce_SpecialFunctions.cpp:38-54`, last
  touched 8f1ec7ac45ae, 2026-07-21). That is the JUCE-blessed shape for "we need a special
  function fast": polynomial first, transcendental only in the tail.
- Cost model is unambiguous: `apiConsoleColor`, `optoTubeColor`, `fetColor` and
  `analogMasterSaturation` each contain exactly one `std::tanh` (`Modules.h:63,75,86,97`), and the
  first three sit in a per-sample per-channel path. `softClip` (`:48`) also has a `tanh` but is
  **dead code with zero call sites**, so it is not in scope.
- **Decision: ADAPT.** Replace the inner `std::tanh` calls with a rational approximation evaluated
  in `float` (the standard `x·(27+x²)/(27+9x²)` form, clamped to ±1 outside its valid range, or a
  Pade/minimax fit chosen to hold the existing curve to < 1e-6 over the plugin's actual operating
  window). Keep the **existing shaping equations byte-identical around it** and gate every change
  behind a measured null against the current output — this is a **numeric** change, not a provably
  identical one.
- Expected saving is **2 tanh per sample** at default settings (Console colour + Opto or FET depends
  on position; each compressor colours once per channel) and **4** when Enhance > 0. Earlier draft
  said 5; that counted the dead `softClip`.
- Do **not** adopt Airwindows' clip law: it is a different sound, not an optimisation.
- Caveat: this changes numerics. It must be an *opt-in-free* change validated by the existing
  harness with a bit-exactness tolerance, not silently.
- Licence: none needed (approximation derived in-house); JUCE is already an AGPLv3-or-commercial
  dependency of this plugin, and the A&S coefficients are public-domain mathematics.

### F2 · Per-sample dB conversions (`gainToDb`, `dbToGain`)
- **How the best sources do it.** Both are pure arithmetic in disguise; the industry pattern is to
  move detectors to **linear-domain envelopes** and convert to dB only where a transfer curve needs
  it, or to apply the curve as a gain in linear domain. This is exactly what the JUCE `dsp` module
  avoids: none of `juce::dsp`'s processors (`Compressor`, `Limiter`, `Gain`) call `pow`/`log10`
  per sample — they hold linear envelope followers and multiply.
- **Specific defect in our code** — `ConsoleModule::process` recomputes `dbToGain(-env) * outG`
  **per sample** (`Modules.h:217`) although `env` only changes by a one-pole. Same for Opto
  (`:277`), FET (`:326`) and the limiter's `dbToGain(compensationDb)` (`:515`). The dB→linear
  conversion can be replaced with an exp2 approximation computed once per block plus an
  interpolated ramp, or kept exact at the cost of one `expf` per sample only while the envelope is
  *moving*.
- **Decision: ADAPT.** (a) Hoist the remaining per-block conversions: `dbToGain(get("gain") + enh*12)`
  and `dbToGain(get("ceiling"))` are already hoisted in the limiter, but `dbToGain(compensationDb)`
  at `:515` is **per sample** and must move to a per-block computed gain with a smoothing ramp.
  (b) Test a linear-domain detector. **This is not a group by construction and must be
  null-tested**: `smooth()` (`Modules.h:21-25`) picks its attack/release coefficient from
  `target > state`, so moving the envelope to linear amplitude changes which coefficient is chosen
  on the sample where the curve crosses, plus rounding — output *will* differ slightly. It is a
  candidate, not a free win, and the earlier draft's "zero change by construction" was wrong.

### F3 · EQ560 double-precision 2× oversampled stack
- **How the source does it.** The upstream is a JSFX (`chkhld/jsfx`, `Equalizer/eq_560.jsfx`,
  pinned `f6d175d8`, MIT — already credited in `THIRD_PARTY_NOTICES.md:5-6`). JSFX is
  single-precision and its author states in the file's `about:` header block that it applies 2×
  oversampling for high-frequency accuracy (header prose, `eq_560.jsfx` lines 7–15 — not line 13's
  code). Note the pinned commit is a *tree* revision whose message is about a different plugin; the
  file content at that revision is what is credited.
- **How JUCE does it.** `juce::dsp::Oversampling` (`juce_Oversampling.h`, 220 lines) offers the
  same 2× with an IIR polyphase allpass option and explicit latency compensation.
- **Our deviation:** `Source/Eq560.h:65-81` runs the whole 10-band cascade in **`double`**, and
  `Peak`/`Butterworth` state is `double` (`:87,109`). The upstream JSFX is `float`.
- **Decision: ADAPT — but the payoff is an estimate, not a measurement.** Port band/Butterworth
  state to `float` (coefficients may stay `double`, cast once per update — update is already
  amortised at 8 samples, `Eq560.h:57-62`). Counter-argument the verifier raised and I accept:
  both the peak SVF and the Butterworth are **dependent-accumulator recurrences**, so the loop is
  latency-bound, not throughput-bound, and double-precision arithmetic on arm64 is only ~3–4
  cycles. The gain is therefore *plausibly* 1.3–2×, **not a clean 2×**, and must be measured before
  being claimed. Benefit to bank regardless: halving the state footprint improves NEON scheduling
  and cache behaviour across a 96-biquad chain.
- **Reject** switching the EQ to `juce::dsp::Oversampling`: it would replace a deterministic,
  licence-clean, already-verified core with a heavier object graph for no measurable gain, and it
  adds its own latency that would change the rack's reported latency (an invariant,
  `AGENTS.md` rule 3).

### F4 · Parameter reads by string on the audio thread
- **How the best sources do it.** JUCE's own `AudioProcessorValueTreeState` documentation and every
  JUCE tutorial resolve parameters **once** into cached `std::atomic<float>*` pointers
  (`getRawParameterValue` in the constructor / `prepareToPlay`) and then only `load()` them.
- **Our code** does exactly the opposite in `Module::get()` (`Modules.h:156-159`); only `Console`'s
  band parameters and the limiter's `auto_gain` are cached (`:176,409`).
- **Decision: ADOPT.** Add a small `std::array<std::atomic<float>*, N>` cache per module, filled
  once in the constructor. Behaviour identical by construction, no allocation, removes all string
  work from the callback — including the single `get("bypass")` concat per `run()`. The saving is
  small (4–5 strings per module per block); the value is that it is **free and provably safe**, not
  that it is large.

### F5 · Metering on the audio thread
- **How the best sources do it.** Metering is normally done on a **background thread** reading an
  audio-thread-written lock-free FIFO, or at minimum lazily: JUCE's own `AudioDeviceManager`
  level meters are driven from the device callback but with no locks and no allocation; the same
  three-pass structure we have is avoidable because peak/RMS/correlation can all be accumulated in
  **one** pass.
- **Decision: ADOPT (partial).**
  - Collapse the two per-sample loops (`Meters.h:79-103` and `:106-113`) into one.
  - Move **`runFft()` off the audio thread** — it is 2048-point FFT + a 31-band fold, run inline in
    the callback (`:139-140,221-252`). Let the audio thread only flag `fftReady` and let the UI
    timer perform the transform on the copied FIFO.
  - Replace the `SpinLock` + `clear()` + `push_back` in `scopeWrite` (`:207-219`) with a
    pre-sized fixed array plus an atomic write index (SPSC), and have the UI copy from it.
  - Note the current `holdCountdown -= 512` is a *per-block* approximation of a sample count
    (`Meters.h:183`) — a correctness smell to fix while in the file.
- Licence: none needed.

### F6 · SIMD / `juce::dsp` blocks
- **What the sources offer.** `juce::dsp` provides SIMD-accelerated `AudioBlock` processors, and
  JUCE 9 enables `JUCE_USE_SIMD` on arm64. Compiler autovectorisation is the practical answer
  here.
- **Decision: ADAPT (cheap variant only).** Do **not** rewrite modules as `juce::dsp` processors —
  that is a sound-affecting rewrite for no guaranteed win. Instead: replace `getSample`/`setSample`
  index arithmetic with raw `float*` pointers hoisted per block (F7). LLVM autovectorises flat
  pointer loops with `-O3` and NEON, not `AudioBuffer` accessor loops.
- **Rejected:** hand-written NEON intrinsics. Four modules × two channels is not enough parallelism
  to justify intrinsics over the compiler, and it would break the "arm64 only" build being
  maintainable.

### F7 · Block-copy and dry-path plumbing
- **Decision: ADOPT.** (a) Skip the `dryBuffer` copy when `wet >= 0.9999f` **and UNITY is off** —
  the copy currently runs unconditionally (`PluginProcessor.cpp:158-159`), but nothing reads it at
  100 % wet (`:180` gates the blend; `:242` is the only other reader). (b) The dry-delay ring uses
  `%` per sample (`:206`) — replace with a compare-and-wrap. (c) Low priority:
  `predefinedPluginRun` copies are memmoves over pre-sized storage, so leave them unless a profile
  flags them.

### F8 · Latency / look-ahead trade
- **What the sources say.** JUCE's oversampling doc is explicit that FIR is linear-phase and higher
  latency while IIR is lower latency with phase compromise (`juce_Oversampling.h`). For look-ahead
  limiters, the published position (and our own `docs/specs/dsp-smoothness-latency-optimization.md`
  §Q-1) is that cutting look-ahead trades transient safety for latency.
- **Decision: REJECT for now.** Not a DSP-optimisation (CPU) question, it is a sound/latency
  product decision owned by the user. Out of scope of this report; the spec's Q-1–Q-3 remain open.

---

## 3. Comparison table

| Candidate | Source (pinned) | Licence | Effort | Expected CPU win | Output change | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| F1 tanh → rational approx | Airwindows FinalClip (pure-arithmetic pattern); JUCE `besselI0` (polynomial pattern) | MIT / AGPL-or-commercial | M | High (2 tanh/ch-sample at defaults, 4 with Enhance > 0) | numeric change, must be null-tested | ADAPT |
| F2 per-sample `dbToGain` / `log10` | JUCE `dsp::Compressor` pattern | AGPL-or-commercial (already a dep) | S | High | candidate, **not** provably identical — null-test | ADAPT |
| F3 EQ560 double → float | chokehold eq_560.jsfx `f6d175d8` | MIT (already credited) | M | Medium (latency-bound recurrence; *unmeasured*) | measurable null, must be recorded | ADAPT |
| F4 cache raw parameter pointers | JUCE APVTS tutorial | — | S | Low | none | ADOPT |
| F5 one-pass meters, FFT off the audio thread, lock-free scope | JUCE device-callback pattern | — | M | **High** (FFT out of callback) | none | ADOPT |
| F6 `juce::dsp` rewrite | juce_dsp | — | L | unknown | changes sound | REJECT |
| F6b hand-written NEON | — | — | M | Low | none | REJECT |
| F7 pointer loops, skip dry copy, ring wrap | — | — | S | Medium | none | ADOPT |
| F8 reduce look-ahead | — | — | S | none (latency, not CPU) | yes | REJECT (user decision) |
| F9 switch EQ to `juce::dsp::Oversampling` | juce_Oversampling.h | — | M | none | adds latency | REJECT |

---

## 4. Recommended order (each step independently revertible, each with a null test)

1. **F4 + F7 + F5** — plumbing with no numerical effect (parameter pointers, skip the unused dry
   copy, one-pass meters, FFT moved off the audio thread, lock-free scope). Ship as one commit;
   this is also the biggest *real-time safety* win.
2. **F2 (hoist/ramp the per-sample `dbToGain`, then a linear-domain detector candidate)** — the
   hoisting is safe; the detector change must be null-tested, because `smooth()` is asymmetric.
3. **F3 (EQ560 to float)** — measure before and after; the recurrence is latency-bound so the win
   is real but probably smaller than the operation count suggests.
4. **F1 (tanh approximation)** — highest risk of an audible delta; only after 1–3 are proven, and
   only with a measured THD/null comparison.

Measurement gate (from `AGENTS.md` §6 and the existing spec): a full harness run must stay green at
the current count, and each step needs a before/after CPU table in `docs/dsp/measurements.md`.
No step should touch the sliding-minimum limiter, the rack-order packing, the reported latency, or
any parameter ID/range/default.

---

## 5. Unverified — training knowledge only

- The numeric form of the tanh rational approximation and its valid range (must be re-derived and
  error-checked against the current curve before use).
- Whether float SVF state is audibly transparent for a 10-band cascade — plausible, **not measured**.
- Every "expected CPU win" figure is qualitative (operation counts). **No profiler has been run.**
  The first implementation step must include an actual Instruments `Time Profiler` capture.
- GitHub search returned very few comparable projects (see §6); the survey is therefore weighted
  toward JUCE upstream and Airwindows, not toward a population of peers.

## 5b. Independent verification of this report

Every claim above was re-derived from source by an independent verifier (not the author). Result:
3 fully verified, 2 partially correct, 6 initially wrong — all six corrected in this revision:

| Was wrong | Correction now in the text |
| --- | --- |
| Limiter attributed a per-sample `gainToDb` | Limiter has none; only `:512` `log10` (auto-gain) and `:515` `dbToGain` |
| "5 tanh/sample removed" | `softClip` is dead code; 2 tanh/ch-sample at defaults, 4 with Enhance > 0 |
| "≈112 biquad evaluations/input sample" | **96** (24/ch × 2 ch × 2×; 0-gain bands skipped) |
| "double → float is a ~2× win" | latency-bound recurrence; unmeasured, plausibly 1.3–2× |
| "14 strings/block/module" | 4–5 per module per block; 20 call sites total |
| "linear-domain detector = zero output change" | **False** — `smooth()` is asymmetric; must be null-tested |
| "`assign` may reallocate every block" | pre-sized in `prepareToPlay`; memmove only |
| Metering section missed `runFft()` inline | added: the FFT is the biggest metering cost |
| "dry path copied at 100 % wet" | the *path* is gated correctly; only the unused **copy** runs |

Also surfaced by the verifier and recorded here: `docs/dsp/measurements.md` has **stale line
citations** against the current code (e.g. it points at `Modules.h:260` for the FET threshold, which
is now `:324`; `PluginProcessor.cpp:138-146` for the undelayed dry copy, which is now `:180-217`).
That is a repo documentation defect, worth a separate cleanup, not a DSP optimisation.

---

## 6. Sources and search provenance

Search method: GitHub REST `search/repositories` (7 queries, ~30 results screened) before the
unauthenticated rate limit; topics `audio-plugin`, `juce`, `dsp`, `compressor`, `limiter`,
`channel-strip`, `oversampling`. Registry step n/a (this plugin vendors no DSP package).
**No peer project had a usable licence or a per-sample cost pattern worth adopting** — that is a
finding in itself: this niche is dominated by closed-source and copyleft implementations.

| Source | Kind | URL | Revision/date | Licence |
| --- | --- | --- | --- | --- |
| JUCE `SpecialFunctions::besselI0` | upstream source | https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/maths/juce_SpecialFunctions.cpp | 8f1ec7ac45ae, 2026-07-21 | AGPLv3 / commercial |
| JUCE `dsp::Oversampling` | upstream source | https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/processors/juce_Oversampling.h | master, 2026-11 | AGPLv3 / commercial |
| JUCE framework | repo | https://github.com/juce-framework/JUCE | 9.0.4 pinned in our `CMakeLists.txt:17` | AGPLv3 / commercial |
| Airwindows FinalClip | upstream source | https://github.com/airwindows/airwindows/blob/master/plugins/MacAU/FinalClip/FinalClip.cpp | master, 214 lines | MIT |
| airwindows (catalogue) | repo | https://github.com/airwindows/airwindows | active, 556 plugins | MIT |
| chokehold eq_560.jsfx | upstream source (our EQ origin) | https://github.com/chkhld/jsfx/blob/f6d175d8/Equalizer/eq_560.jsfx | f6d175d8 (v1.8.3) | MIT |
| blablack/nine-strip | repo | https://github.com/blablack/nine-strip | 2026-09-19 | **AGPL-3.0 — unusable** |
| FilTer87/VST-AnalogChannel | repo | https://github.com/FilTer87/VST-AnalogChannel | 2025-12-13 | **GPL-3.0 — unusable** |
| fsecada01/bus_channel_strip | repo | https://github.com/fsecada01/bus_channel_strip | 2026-09-15 | BSD-3 — Rust, no C++ reuse |
| gmoican/PunkOTT(-MB) | repo | https://github.com/gmoican/PunkOTT | 2026-03-07 | MIT — OTT-style upward compressor, different topology |

---

## 7. What I did not do

- Did not build, install, commit, push, tag or release anything.
- Did not change any source file in the repo (read-only audit of a temp clone).
- Did not run a profiler — the CPU ordering above is derived from operation counts and must be
  confirmed with Instruments before implementation.
