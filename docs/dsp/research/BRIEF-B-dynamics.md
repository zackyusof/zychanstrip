# LANE B BRIEF — Opto (LA-2A style) and FET (1176 style) dynamics modules

You are a DSP analyst. Repo: `/root/projects/zychanstrip` (branch `main`, commit `51a0927`).
Your deliverable is ONE file: `docs/dsp/research/lane-B-dynamics.md`.

## Rules

- **Do not modify any other file.** No `Source/` edits, no commits, no `git add`.
  The orchestrator commits your deliverable.
- Read the actual code. Every claim needs a `file:line` citation. Uncited reasoning must
  be marked *(inference)*.
- No marketing language. Numbers and code only.

## Read these first (whole files, not excerpts)

1. `Source/Modules.h` — all of it (389 lines), especially `:14-48` (helpers),
   `:186-228` (`OptoModule`), `:230-271` (`FetModule`).
2. `Source/PluginProcessor.cpp:39-52` — opto and FET parameter registration.
3. `README.md` "The Four Modules", "Signal Flow, Latency and Phase".

## Required content

### 1. Opto module — exact algorithm
- Parameter → internal value mapping: `peak` → threshold (`-pr*40`), `gain` → makeup
  (`gain*0.01*30` dB), `mode` → ratio (3 vs 12). Give the table of resulting numbers at
  several knob positions (0/25/50/75/100 % peak reduction; gain 0/50/100).
- Detector: peak-of-channels, **feed-forward, no look-ahead**. Say what that means for
  transient overshoot and cite the loop.
- Two-stage release: `fast` (attack 10 ms / release 60 ms) and `slow` (attack 10 ms /
  release 1800 ms), then `max(fast, slow)` at `Modules.h:216`. Explain what
  "program-dependent release" means here, and why `slow` targets `gr*0.6`.
- Static curve: `gainReductionDb()` with a 10 dB soft knee — give the knee region and the
  slope for both ratios. State whether the knee is symmetric and how it is computed.
- Gain application is smoothed *after* the detector (`smooth()` uses attack when rising,
  release when falling) — describe the resulting effective attack/release times, and the
  consequence of the detector itself being instantaneous.
- `optoTubeColor`: give the polynomial, the piecewise `tanh` above 0.55, and the
  harmonic content it implies (2nd-order asymmetry). State the gain at small signal.

### 2. FET module — exact algorithm
- Input/output gain applied per sample; **fixed -10 dB threshold** (`Modules.h:260`),
  ratio bank 4/8/12/20/30, attack mapped `jmap(1..7 → 0.8..0.02 ms)`, release
  `jmap(1..7 → 1100..50 ms)`. Give the full mapping table for attack and release steps
  1..7 (compute the actual ms values).
- Detector: peak of channels **after** input gain — explain why that makes the input knob
  a threshold control, and what the 0.8 ms vs 0.02 ms attack difference does to transients
  *(inference, labelled)*.
- Single `env` smoother with attack/release coefficients — state the exponential
  coefficient form (`timeCoef`) and the time-constant convention it implies (63 % vs
  10-90 %).
- `fetColor`: piecewise polynomial/tanh at 0.30, harmonic content (3rd dominant), gain at
  small signal.
- No makeup beyond `fet_output`; state that gain reduction is not auto-compensated.

### 3. Comparative table
One table: property × Opto × FET — detector position, threshold source, ratio law,
knee, attack range, release range/structure, colour stage, makeup, latency, meter value
(`grDb` semantics: max-hold per block vs instantaneous — cite both stores).

### 4. Interaction with the rest of the rack
- Opto → FET and FET → Opto: what each does to the other's detector, with numbers where
  the mapping allows (e.g. "FET at input +10 dB drives the opto detector 10 dB harder →
  threshold effectively moves from -X to -X+10 relative to programme").
- Both → limiter: how their gain reduction and their colour stages reach the limiter's
  look-ahead ceiling and the auto-gain energy comparison (`Modules.h:344-360`).
- Colour stacking: `optoTubeColor` then `fetColor` vs the reverse — are they commutative?
  Answer by argument from the two functions (asymmetric polynomials → generally not) and
  say what the audible difference is *(inference, labelled)*.
- Do either of these modules report latency? Cite.
- What does bypass do exactly (`Module::run` → `processBypassed`)? For these two, is
  bypass a clean pass-through (no colour, no gain, no state update)? State the exact
  consequence of bypassing mid-programme (envelope state retained?).

### 5. Re-order sensitivity
For each of the two modules: what changes when it is moved before vs after the console
EQ, before vs after the other compressor, and before vs after the limiter. Table with
"position → mechanism → expected effect".

### 6. Hazards / defects you can see in the code
Ranked list with citations: e.g. per-sample `std::max` over channels with `getSample`
(performance), `grDb` reporting only the block max, no attack/release on the detector
itself, `fast`/`slow` initialisation, `timeCoef` for sub-millisecond attacks at 44.1 kHz
accuracy, colour stages applied to already-limited signals, etc. Say which are real bugs
and which are design choices.

## Output format

Markdown, ~220-340 lines, `##` sections matching the headings above, tables for mappings.
End with `## Sources checked` listing files+line ranges you actually read.
