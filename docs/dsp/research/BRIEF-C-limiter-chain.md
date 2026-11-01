# LANE C BRIEF — Limiter/Enhance module and the chain-level path (order, latency, master, mix, meters)

You are a DSP analyst. Repo: `/root/projects/rackstrip` (branch `main`, commit `51a0927`).
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
- input drive: `gain + enhance*12` dB, then `analogMasterSaturation` when active;
- required-gain ring `reqRing` (`ceil/pk`), sliding minimum over `la` samples;
- moving average over the same ring (`sum`/`avgRing`) and the one-pole release
  `relGain`; explain `g = min(relGain, avg)` — which term wins when;
- the delay lines: `delay` (wet, look-ahead) and `dryDelay` (dry, for the auto-gain
  energy comparison) and the read index `(wp+1)%la`;
- the hard clamp to `±ceil` at the output;
- the **safety gain** (`outputPeak > ceil` → `ceil/outputPeak`) applied per sample after
  the auto-gain compensation — explain why it exists and what it does to the auto-gain's
  intended level match.
- Look-ahead length: `la = ceil(0.0015*sr)`, reported latency `la-1`. Give the exact
  numbers for 44.1/48/88.2/96 kHz in samples and ms.
- Why `la` is recomputed only in `prepare()` and what a sample-rate change does.

### 2. Enhance and auto-gain
- `lim_enhance` has **two** effects (drive + `analogMasterSaturation` blend) — quantify
  both at enh = 0/25/50/100 %.
- Auto-gain: dry/wet **energy** comparison with a 200 ms exponential (`energyCoef`), a
  100 ms smoothing on the dB correction, clamp ±36 dB, then the safety limiter. Explain
  exactly what reference it uses — and why that makes it **order-dependent** (this is
  known issue `rackstrip-autogain-order`). State the mechanism precisely: it compares
  the limiter's own input (post-upstream-modules) to its own output, so upstream gain
  changes are inside the reference, but any module *after* the limiter is outside it.
- `processBypassed` still runs the limiter in "inactive" mode: say exactly what still
  happens (delay lines filled? saturation skipped? auto-gain zeroed?) and what that means
  for switching bypass live — is the transition click-free? Is the latency preserved?

### 3. Chain-level path (the processor)
- `prepareToPlay`: latency sum, `dryBuffer` allocation, master smoother, meters.
  Explain `setLatencySamples(latency)` being a sum over modules — and what breaks if a
  module ever reports latency that depends on a parameter or on order.
- `processBlock` order of operations, exactly as written: dry copy → module loop in rack
  order → master gain → dry/wet blend → metering tap. Quote the lines.
- **Order packing**: `packedOrder` `uint32_t`, one byte per slot, `getOrder()`/`setOrder()`
  and the permutation check; default `0x03010200` = `[0,2,1,3]`. Explain why a single
  atomic word is used instead of an array, and what the audio thread can and cannot tear.
- **State**: `order` property round-trip, the legacy migration path, and what happens if
  a saved session has a malformed/duplicate order (`setOrder` returns silently → order
  stays whatever it was). Is that a real hazard? Say plainly.
- **Dry/wet**: applied after master, wet<0.9999 branch, and the **missing latency
  compensation** on `dryBuffer` (known issue `rackstrip-mix-phase`). Quantify the
  comb-filter notch: with 71 samples at 48 kHz, give the first notch frequency and the
  expected cancellation at 1 kHz for a 50 % mix. Show the arithmetic.
- **Metering tap** (`Source/Meters.h`): list each measurement, the window/smoothing
  constant and whether it is pre/post which stage. State that it never alters audio and
  cite the atomic publication path.

### 4. Re-order interaction — from the chain's point of view
- What is **order-invariant** by construction: latency, master, mix, metering, parameter
  values. Prove each with a citation.
- What is **order-dependent**: every detector (each module's), the limiter's auto-gain
  reference, colour stacking order, and the limiter's ceiling position relative to
  upstream colour. Give a table: "stage moved → what changes → why".
- The two extremes: limiter **last** (`[0,2,1,3]`, default) vs limiter **first**
  (`[3,0,2,1]` etc.) — describe what each configuration is for in practice and what
  breaks in the other (e.g. colour after the ceiling can exceed 0 dBFS; auto-gain
  compensates a different reference). This is the heart of the document: be precise.

### 5. Hazards / defects you can see in the code
Ranked with citations: missing dry-path latency compensation; auto-gain reference
inside the module; `sum`/`avgRing` initialisation; `minG` reporting `-gainToDb(minG)`
sign; per-sample `std::min` scan over `reqRing` (O(n·la) per sample → CPU at high
sample rates); `blockOutPeak` only updated when active; `dryBuffer` growth path
allocating on the audio thread if the host sends a bigger block; `setOrder` silent
failure; `getStateInformation` writing `order` into the APVTS copy each save.

## Output format

Markdown, ~250-380 lines, `##` sections matching the headings above, tables for the
sample-rate/latency numbers and the order matrix. End with `## Sources checked` listing
files+line ranges you actually read.
