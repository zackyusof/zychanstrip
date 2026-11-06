# zychanstrip DSP performance optimisation

| Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- |
| 1 | Requirements | done | Orchestrator | §1 — user request 2026-11-03 ("research on github for top projects similar and see how you can optimize the dsp code… make it next beta, build/install") |
| 2 | Design | done | Planner | §2 — prior-art report `docs/research/dsp-optimization-prior-art.md` (Tier 2), verified |
| 3 | Build | done | Engine | §3 — B1–B6 (B6 = neural-branch gate, 2026-11-06) |
| 4 | Test | done for B6 | Validator | §4 — snapshot harness 593/593, linked ColourEngine bench |
| 5 | Deploy | done for B6 | Orchestrator | §5 — `tools/install_global.sh`, `ZYCHANSTRIP_BETA=beta.10` |
| 6 | Maintain | pending | Orchestrator | §6 — soak, CPU table in `docs/dsp/` |

---

## 1. Requirements

### Problem
The audio path calls `libm` transcendentals per sample, runs a 2048-point FFT and takes a spin
lock inside the audio callback, and converts parameters from strings every block. Measured cost
before any change (headless `tools/research/bench`, Apple Silicon, 48 kHz, stereo, 256-sample
blocks, best of 5):

| module | ns / sample | x realtime |
| --- | --- | --- |
| Console (EQ560 + detector) | 43.7 | 480x |
| Opto | 14.3 | 1453x |
| FET | 10.8 | 1922x |
| Limiter | 26.8 | 775x |
| **FULL CHAIN** | **99.5** | **208x** |

### Users and outcomes
Musicians get the same sound with lower CPU and a callback that never allocates, never blocks and
never runs an FFT. No parameter, range, default, rack order, latency or session format changes.

### Scope
`Source/Modules.h`, `Source/Eq560.h`, `Source/Meters.h`, `Source/MeterPanel.*`,
`Source/PluginProcessor.cpp`, `tools/research/*`, `docs/dsp/*`.

### Non-goals
No tonal redesign, no new controls, no parameter ID/range/default change, no latency change, no
look-ahead reduction, no third-party DSP code import, no version bump (the beta label only).

### Requirements (observable)

| ID | Requirement | Acceptance check | Priority |
| --- | --- | --- | --- |
| REQ-1 | Audio callback performs no allocation, no lock, no FFT | Code review + callgraph of `processBlock`; scope path is SPSC | must |
| REQ-2 | No parameter is read by `juce::String` key on the audio thread | grep: no `get (")` / `getRawParameterValue` in `process`/`runLimiter` | must |
| REQ-3 | Full-chain CPU measurably lower than the baseline, module by module | `tools/research/bench` before/after table, same binary flags | must |
| REQ-4 | Output is bit-identical for the plumbing steps; numerically bounded for the maths steps | `--null` comparison: 0.0 diff for F4/F7, ≤ -120 dBFS peak for F2/F1/F3 | must |
| REQ-5 | Rack latency, order behaviour and session state unchanged | snapshot harness green at its full count; `getLatencySamples()` unchanged | must |
| REQ-6 | Everything stays analysable on a non-JUCE host | `tools/research/bench` builds with plain `c++` | must |

### Constraints
Arm64 AU only; `AGENTS.md` rules 1–9 apply (no parameter changes, latency fixed, global install
only, harness green before push). Beta label only — version/tag/promotion stay the user's call.

### Risks
Rounding differences in the EQ and colour stages; a fast-math approximation changing the sound;
moving the FFT off the audio thread introducing a visible analyser delay.

---

## 2. Design

### Prior art
`docs/research/dsp-optimization-prior-art.md` — Tier 2, 11 GitHub queries + 7 pinned upstream
reads. Adopted patterns: cached raw-parameter pointers (JUCE APVTS), one-pass metering and an
off-thread FFT (JUCE device-callback pattern), rational minimax approximation instead of `tanh`
(JUCE `SpecialFunctions::besselI0`, Airwindows pure-arithmetic clipping). Rejected: importing any
peer plugin (all AGPL/GPL), `juce::dsp` module rewrite, hand-written NEON, look-ahead reduction.

### Module map

| Module | Owns | Notes |
| --- | --- | --- |
| Fast math | `Source/Modules.h` helper block | `dbToGain`/`gainToDb`/`softTanh` |
| Module params | `Source/Modules.h` class bodies | per-module `std::atomic<float>*` cache |
| EQ core | `Source/Eq560.h` | float state |
| Metering | `Source/Meters.h`, `Source/MeterPanel.*` | one-pass, off-thread FFT, SPSC scope |
| Processor | `Source/PluginProcessor.cpp` | dry-copy skip |
| Verification | `tools/research/bench.cpp`, `tools/research/build_bench.sh` | CPU + null harness |

### Invariants (must not break)

| ID | Invariant | Where |
| --- | --- | --- |
| INV-1 | Parameter IDs/ranges/defaults unchanged | `PluginProcessor.cpp::createLayout` |
| INV-2 | `getLatencySamples()` = `la - 1`, independent of order/bypass | `Modules.h` limiter |
| INV-3 | No allocation/lock/FFT in `processBlock` | `Modules.h`, `Meters.h` |
| INV-4 | Meter readings bit-close to current (peak/RMS/correlation/LUFS) | `Meters.h` |
| INV-5 | Sliding-minimum limiter untouched (exact output) | `Modules.h` limiter |

### Design decisions

| ID | Decision | Why | Rejected |
| --- | --- | --- | --- |
| DES-1 | Fast math is **opt-in-free but bounded**: `dbToGain` uses `exp2` on a float, `softTanh` uses a rational form only outside a linear core | keeps the curve shape, removes the libm call | approximating the whole curve (audible) |
| DES-2 | FFT runs on the message thread from a published FIFO snapshot | callback does a `memcpy` only | background thread (extra lifetime complexity) |
| DES-3 | Scope buffer becomes a fixed SPSC ring with atomic write index | removes `SpinLock` + `push_back` | double buffer with a mutex |
| DES-4 | EQ560 state goes to `float`, coefficients stay `double` | halves state traffic on a 96-biquad chain | full `float` (coefficient precision loss) |
| DES-5 | Measure with the existing headless shim, not Instruments | reproducible on any host, no GUI/flags drift | Instruments only (kept as a follow-up check) |

---

## 3. Build

Work is ordered so each step is independently revertible and independently null-tested:

1. **B1 — parameter pointers** (F4): per-module `std::atomic<float>*` cache filled in the ctor.
2. **B2 — dry-copy skip** (F7): skip `dryBuffer` copy when `wet >= 0.9999f` and UNITY is off.
3. **B3 — metering** (F5): one pass, off-thread FFT, SPSC scope.
4. **B4 — fast math** (F2/F1): `dbToGain` via `exp2`, `gainToDb` via `log2`, `softTanh` for the
   five saturators; `dbToGain(compensationDb)` hoisted out of the limiter's sample loop.
5. **B5 — EQ560 float state** (F3).
6. **B6 — neural-branch gate** (`Source/ColourEngine.cpp`, 2026-11-06): the WaveNet is run
   only while the neural branch actually contributes to the output.

### B6 — neural-branch gate (measured)

`ColourEngine::render()` ran both channel WaveNets on every quantum regardless of colour mode.
`frame()` mixes them with `weights[1]`, which is **exactly zero** in ALGO and CLEAN, so that work
produced nothing. Four engines (one per module), stereo, 48 kHz, 256-sample blocks, best of 3
× 2000 blocks — `ColourEngine` linked from the release objects, per-sample call shape identical
to `Module::colourFrame`:

| colour mode | before (ns/host-sample) | after | share of one core @1x |
| --- | --- | --- | --- |
| ALGO (default) | 6338 | **150** | 30.4 % → **0.7 %** |
| CLEAN | 6268 | **148** | 30.1 % → **0.7 %** |
| NAM | 6270 | 6282 | 30.1 % → 30.2 % (unchanged, as intended) |

Gate: `needNeural = weights[1] > 1e-5f || (fadeRemaining > 0 && steps[1] > 0)`. The second term
keeps the models running for the whole 20 ms crossfade into NAM, whose weight starts at zero, so
the entry is masked. Model state is **retained, not reset** — `nam::DSP::Reset()` may allocate and
must not run on the audio thread (documented in `NAM/dsp.h`).

Rejected: `NAM_ENABLE_A2_FAST` (A2-Lite specialised kernel) — measured 6215 ns vs 6282, ~1 %,
because the embedded model is a `SlimmableContainer` member and the fast path is the
"correct-but-unoptimized" baseline. Rejected: threading (see §3 deviations).

### Build commands

```sh
# headless CPU + null harness (any host with a C++17 compiler)
./tools/research/build_bench.sh
./build/bench                    # CPU table
./build/bench --null out.f32     # deterministic output capture

# the real AU (macOS arm64)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DZYCHANSTRIP_BETA=beta.10
ninja -C build
tools/build_snapshot.py && ./build/snapshot/snapshot "~/.config/opencode/temp/shots"
sudo tools/install_global.sh
```

### Deviations from design
_Recorded as they happen._

**2026-11-06 — the user asked for multi-threading; measurement rejected it.** Request: "this
plugin is very cpu heavy and its single threaded, cant you make it multi-threaded?"

* The four modules are **serial**: audio flows Console → Opto → FET → Limiter in a user-defined
  order, each stage consuming the previous stage's output. No two stages can run concurrently
  without changing the result, so there is nothing to parallelise across modules.
* Across instances the plugin is already parallel: a DAW renders tracks on its own thread pool.
* Inside one instance only the two channel WaveNets are independent (~7.5 % of a core each).
  Splitting them needs a persistent worker thread per engine — four extra spinning/waiting
  threads per instance, an RT-safety and oversubscription risk — to halve a branch that, after
  B6, is not even running unless the user selects NAM. Rejected as a bad trade.
* The measured cause of "very CPU heavy" was not thread count but B6: the neural engine ran in
  ALGO and CLEAN where its contribution is multiplied by zero. Fixing the gate removed 30 % of
  a core for the default patch without touching threading, latency, or the audio result.

---

## 4. Test

| ID | Test | Covers | Command | Result |
| --- | --- | --- | --- | --- |
| TST-1 | CPU baseline and after, per module and full chain | REQ-3 | `./build/bench` | baseline recorded in §1 |
| TST-2 | Null: plumbing steps bit-identical | REQ-4 | `./build/bench --null` + compare | pending |
| TST-3 | Null: maths steps within -120 dBFS | REQ-4 | same | pending |
| TST-4 | GUI/DSP snapshot harness green | REQ-5 | `tools/build_snapshot.py` + snapshot | **PASS — `RESULT 593/593 passed`** (2026-11-06, beta.10 build) |
| TST-5 | Real-time safety: no allocation/lock/FFT in the callback | REQ-1 | code review of `processBlock` callgraph | **PASS for B6** — the gate only reads two `Impl` floats; `Reset()` deliberately not called (it may allocate) |
| TST-6 | Colour-mode CPU, before/after | REQ-3 | linked `ColourEngine` bench | **PASS** — see §3 B6 table (ALGO 6338 → 150 ns/host-sample) |

### Known gaps / not verified
The headless bench measures DSP in isolation. A DAW-level CPU reading (Idle/Play in a real
session) is still a manual follow-up; `auval` is never used (user-owned validation).

---

## 5. Deploy

Beta label only: `ZYCHANSTRIP_BETA=beta.2` on the current `1.1.1` line. No tag, no Gitea release
and no version bump without the user's instruction. Install is `sudo tools/install_global.sh`
after a clean pushed `HEAD`.

| ID | Step | Command | Evidence |
| --- | --- | --- | --- |
| DEP-1 | build + install | `sudo tools/install_global.sh` | **done 2026-11-06** — beta.10, clean `main`, installed component verified |
| DEP-2 | CPU figures published | `docs/dsp/` | §6 — table added to `docs/dsp/nam-colour.md` |

---

## 6. Maintain

CPU table and null figures get published in `docs/dsp/measurements.md` beside the existing
reorder tables, so the next optimisation wave starts from a measured baseline instead of a guess.

---

## Traceability

| REQ | Design | Test | Deploy |
| --- | --- | --- | --- |
| REQ-1 | DES-2, DES-3 | TST-5 | DEP-1 |
| REQ-2 | B1 | TST-5 | DEP-1 |
| REQ-3 | DES-5 | TST-1 | DEP-1 |
| REQ-4 | DES-1, DES-4 | TST-2, TST-3 | DEP-1 |
| REQ-5 | DES-4 | TST-4 | DEP-1 |
| REQ-6 | DES-5 | TST-1 | DEP-1 |
