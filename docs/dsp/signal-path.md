# Signal path — from host buffer to output

> **Beta.2 update:** global `colour_mode` selects ALGO, embedded NAM, or CLEAN.
> The routing and latency changes in [NAM colour DSP](nam-colour.md) supersede older
> colour-only/zero-latency statements and historical measurements below. NAM is stateful;
> ALGO retains the documented legacy shaping. CLEAN removes added colour, retaining
> functional EQ/dynamics/limiting and a matched delay. Module order remains user-controlled.


This is the frame every other document in `docs/dsp/` lives in: what owns the audio,
what order things happen in, and which properties are invariant under re-ordering.

Source of truth: `Source/PluginProcessor.cpp` (processor) and `Source/Modules.h`
(module contract). Line numbers refer to commit `429cede`; re-check if the file moved.

---

## 1. The buffer contract

`ZychanStripProcessor` is a plain `juce::AudioProcessor` with a fixed four-slot rack
(`Source/PluginProcessor.h:10`). It accepts stereo or mono and requires input and output
channel sets to match (`PluginProcessor.cpp:91-96`). There is no MIDI, no sidechain, no
tail (`PluginProcessor.h:25-30`).

Everything is **in-place**: each module receives the same `juce::AudioBuffer<float>&`
and writes back into it. There is exactly one copy of the dry signal, taken before the
rack runs (`PluginProcessor.cpp:122-126`).

```
host input buffer
   │
   ├─► dryBuffer copy            (untouched reference for the mix)
   │
   └─► slot 0 → slot 1 → slot 2 → slot 3
                                      │
                                      ├─ master gain (smoothed)
                                      ├─ dry/wet blend
                                       ├─ optional UNITY matcher (stereo-linked)
                                       └─ metering tap (read-only)
                                              │
                                              ▼
                                        host output
```

### Why one buffer and not four

A module is a *transform*, not a node with its own storage: `Module::run` takes the whole
block, reads parameters, processes, and returns (`Source/Modules.h:110-114`). That means
the rack is a linear sequence of in-place passes, which is what makes re-ordering a
one-line change in the loop rather than a graph rebuild.

The cost of that choice: a module can only see what the previous module left in the
buffer. Any information a module needs about the *original* input must be captured
before the loop — which is exactly what `dryBuffer` does, and exactly what the limiter's
auto-gain does **not** do (it measures its own input, not the chain input — see
`module-limiter.md` and `interaction.md`).

---

## 2. The module contract

`rack::Module` (`Source/Modules.h:100-132`) defines five things:

| Member | Contract |
| --- | --- |
| `prepare(sr, maxBlock)` | called from `prepareToPlay` only; resets state, sizes ring buffers |
| `getLatencySamples()` | latency this module introduces, in samples; default 0 |
| `run(buf)` | bypass check + `process`; the only entry point the chain uses |
| `grDb` | gain reduction for the UI (dB ≥ 0), published per block |
| `gainCompDb` / `outPeak` | auto-gain correction and output peak (limiter only) |

`run()` is the bypass gate:

```cpp
if (get ("bypass") > 0.5f) { processBypassed (buf); grDb.store (0.0f); outPeak.store (0.0f); }
else                         process (buf);
```
— `Source/Modules.h:112-113`

Two bypass behaviours exist in this codebase and they are **not** the same:

* **Default** `processBypassed` does nothing (`Source/Modules.h:122`) → a true
  pass-through: no colour, no gain, no delay. `ConsoleModule`, `OptoModule` and
  `FetModule` use this.
* **`LimiterModule` overrides it** (`Source/Modules.h:301`) and still runs the whole
  limiter with `active = false`: the delay line is filled and read, the saturation and
  the ceiling clamp are skipped, auto-gain is forced to 0 dB
  (`Source/Modules.h:322-366`). Bypassing the limiter therefore keeps its **71 samples of
  latency**, which is required for `setLatencySamples()` to stay constant.

### Parameter access

Modules do not hold parameter pointers except where they cache one for the audio thread
(`ConsoleModule`'s ten band atomics, `LimiterModule`'s `auto_gain`). Everything else goes
through `get(name)` → `apvts.getRawParameterValue(prefix + name)->load()`
(`Source/Modules.h:124-127`). Prefixes are `con_`, `opto_`, `fet_`, `lim_`
(`PluginProcessor.cpp:83-86`).

Parameters are **read once per block**, not per sample, except the ten EQ band values
which are read once per block in `updateBands()` (`Source/Modules.h:174-179`) and then
smoothed internally at 20 ms (`Eq560.h:33`, `Eq560.h:47-63`). Consequence: automation is
block-rate for gain/threshold controls and effectively sample-rate-smooth only inside the
EQ.

---

## 3. Order packing

The rack order is stored as **one 32-bit word**, one byte per slot:

```cpp
enum ModuleId { Console = 0, Opto = 1, Fet = 2, Limiter = 3, NumModules = 4 };
std::atomic<uint32_t> packedOrder { 0x03010200u };   // slot0..slot3, little-end first
```
— `Source/PluginProcessor.h:10`, `PluginProcessor.h:58`

`getOrder()` unpacks a fresh `std::array<int,4>` per call, reading the atomic once
(`PluginProcessor.cpp:153-159`). `setOrder()` validates a permutation — every id in
`[0,4)`, no duplicates — and only then stores (`PluginProcessor.cpp:161-173`).

Properties this buys:

* **No tearing.** The audio thread reads one aligned 32-bit atomic, so it can never see
  half of an old order and half of a new one. A `std::array<int,4>` written by the
  message thread would allow exactly that.
* **No allocation, no locks, no re-prepare.** Changing the order does not touch module
  state, latency, or the parameter tree.
* **Silent rejection.** A non-permutation is ignored (`PluginProcessor.cpp:168`), so a
  corrupt saved session leaves the *current* order in place rather than scrambling the
  rack. That is safe but invisible — worth knowing when debugging a "wrong order" report.

Default order `0x03010200` = `[0,2,1,3]` = **EQ560 → FET 1176 → LA-2A → Enhance**
(`PluginProcessor.h:56-58`).

---

## 4. Latency accounting

```cpp
int latency = 0;
for (auto& m : modules) { m->prepare (sr, block); latency += m->getLatencySamples(); }
setLatencySamples (latency);   // constant regardless of order or bypass
```
— `PluginProcessor.cpp:100-106`

* The sum is computed **once**, in `prepareToPlay`, over all four modules.
* It is independent of order (addition commutes) and of bypass (the limiter reports its
  latency even when bypassed, because its `processBypassed` still delays).
* Measured: **71 samples** at 48 kHz for every one of the 24 orders
  (`measurements.md` §7); EQ, Opto and FET report 0.
* The limiter's `la = ceil(0.0015 · sr)` (`Source/Modules.h:285`) is recomputed only in
  `prepare()`, so a sample-rate change re-runs `prepareToPlay` and updates both the ring
  sizes and the reported latency consistently.

**Consequence for the rest of the chain:** any module that reports 0 latency while
actually delaying the signal desynchronises the reported latency from reality. The EQ's
2× oversampling filters (`Eq560.h:40-41`) are such a case — they introduce a small group
delay that is *not* reported. It is inaudible next to 71 samples but it is a real
inconsistency; see `module-console.md` §6.

---

## 5. processBlock, line by line

```cpp
// 1. clear any output channels the host added
for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, numSamples);

// 2. snapshot the untouched input (grow only if the host sends a bigger block than declared)
dryBuffer.setSize(...) if needed;  dryBuffer.copyFrom(...)

// 3. run the rack in the packed order
for (int id : getOrder()) modules[(size_t) id]->run (buffer);

// 4. master gain (SmoothedValue, 20 ms)
masterGain.setTargetValue (dB(master_level));  masterGain.applyGain (buffer, numSamples);

// 5. dry/wet blend
if (wet < 0.9999f) { buffer *= wet; buffer += dryBuffer * (1-wet); }

// 6. optional final UNITY match, referencing original input and completed output
// 7. metering tap
meters.process (buffer);
```
— `PluginProcessor.cpp:113-150`

Points worth knowing:

* **The dry snapshot is taken before any module**, so the mix always blends against the
  *host input*, never against a partially processed signal.
* **Master gain and mix are after the rack**, so they cannot be re-ordered. Moving a
  module never changes where the master fader acts.
* **The mix uses continuously delayed dry**, matching fixed rack timing at every value.
  Active flat EQ uses linear-phase oversampling, and capture transport/polarity is normalized.
* **UNITY, when enabled, is last before metering**: its stereo-linked energy follower reads
  the untouched host input and the completed post-master/post-mix output, applies a smoothed
  correction limited to ±18 dB, and holds through input silence. It is a level match, not
  waveform alignment or a true-peak limiter.
* **The metering tap is after UNITY and read-only** (`Meters.h:14-16`, `:71`), so the meters show
  what actually leaves the plugin.

### Block-size growth

`dryBuffer` is allocated in `prepareToPlay` for the declared block size
(`PluginProcessor.cpp:107`) and grown inside `processBlock` if the host exceeds it
(`PluginProcessor.cpp:122-124`). That growth allocates on the audio thread — rare, but a
real-time hazard if a host is inconsistent about block size.

---

## 6. State and migration

`getStateInformation` serialises the APVTS plus an `order` property
(`"0,2,1,3"`) — `PluginProcessor.cpp:176-185`. `setStateInformation`:

1. loads the tree, and if `master_level` is absent inserts it at 0 dB
   (`PluginProcessor.cpp:196-202`);
2. inserts any missing `con_eq_*` band and `lim_auto_gain` parameter
   (`PluginProcessor.cpp:203-214`);
3. replaces the APVTS state, then parses `order` and applies it if it has exactly four
   tokens (`PluginProcessor.cpp:217-223`).

Note that `addMissingParameter("lim_auto_gain", 0.0f)` restores **off** for old sessions
while new instances default to on (`PluginProcessor.cpp:71`) — a deliberate
"do not change the sound of an existing session" decision, documented in
`docs/specs/api560-autogain.md`.

The order property is written into a *copy* of the APVTS state at save time, so it never
pollutes the live parameter tree.

---

## 7. What re-ordering can and cannot change

| Property | Invariant? | Why |
| --- | --- | --- |
| reported latency | yes | summed once, order-independent (`PluginProcessor.cpp:100-106`) |
| master gain, dry/wet, meters | yes | applied after the module loop (`PluginProcessor.cpp:132-149`) |
| parameter values, bypass flags | yes | live in the APVTS, not in the chain |
| saved state blob | yes | same parameters; only `order` differs |
| per-module gain reduction | **no** | each detector sees whatever the previous module produced |
| output level | **no** | measured 0.69 dB spread neutral, 6.96 dB hot (`measurements.md` §1) |
| output peak vs ceiling | **no** | colour after the limiter re-opens the ceiling (`measurements.md` §3) |
| harmonic content | **no** | four different non-linear colour stages, not commutative (`interaction.md`) |
| auto-gain correction value | **no** | reference is the limiter's own input (`measurements.md` §4) |

The practical summary: **re-ordering never breaks the plumbing, always changes the
sound** — because three of the four modules are non-linear or dynamics processors and the
fourth is a colour stage.

## Beta.6 parallel mix timing — current behavior

This supersedes earlier dry-path/oversampling/capture timing descriptions. EQ560 still runs
its SVF at 2x, with a symmetric 63-tap FIR pair adding 31 native samples and matching bypass
padding. The pinned API capture is polarity-normalized; API/opto/FET native transport
offsets of 16/6/8.5 samples are advanced within fixed causal padding in every mode. The FET
fractional offset uses the existing double-precision sinc. Tape residual behavior is unchanged.

The dry ring advances continuously at 0/100% as well as partial mix. Even fully dry now
returns untouched input at the fixed reported latency; RESET clears both wet/dry histories.
Mix targets ramp 20 ms, and Auto Gain/UNITY compare aligned input time intervals. Phase from
intentional EQ bands/captured analog response remains; this is not arbitrary phase removal.
See [parallel mix repair](../specs/parallel-mix-phase-repair.md) for causes, tradeoffs and tests.
