# LANE A BRIEF — Console / API 560 graphic EQ module

You are a DSP analyst. Repo: `/root/projects/rackstrip` (branch `main`, commit `51a0927`).
Your deliverable is ONE file: `docs/dsp/research/lane-A-console.md`.

## Rules

- **Do not modify any other file.** No `Source/` edits, no commits, no `git add`.
  The orchestrator commits your deliverable.
- Read the actual code. Every claim needs a `file:line` citation, e.g.
  `Source/Modules.h:167`. If you cannot cite it, mark it *(inference)*.
- No marketing language, no hardware-manual claims, no "sounds like". Numbers and code.
- If something is dead/legacy code, say so explicitly and name the parameters that are
  registered but inert.

## Read these first (whole files, not excerpts)

1. `Source/Eq560.h` (145 lines) — the EQ core.
2. `Source/Modules.h:100-183` — `Module` base class + `ConsoleModule`.
3. `Source/Modules.h:41-62` — `softClip` and `apiConsoleColor`.
4. `Source/PluginProcessor.cpp:24-38` and `:67-71` — console parameter registration.
5. `README.md` sections "The Four Modules" and "Parameters".

## Required content

### 1. Signal flow inside the module
Sample-by-sample path from `ConsoleModule::process` (`Source/Modules.h:155`) through
`Eq560::processSample` to the colour function. State exact call order and per-sample vs
per-block work. Include `beginSample()`'s smoothing/coefficient-update cadence.

### 2. The EQ core
- Topology: why 2× oversampling (`prepare (sr*2.0, ...)`) and what the Butterworth
  up/down filters are for. Explain the "stuffed sample" (zero-stuffing) trick at
  `Eq560.h:69,76` — what problem does it solve, what is the effective oversampled rate,
  and what is the resulting group delay / phase behaviour at 44.1k and 48k.
- Per-band filter: identify the SVF form in `struct Peak`, give the transfer function
  implied by `a1,a2,a3,m1`, and explain the `A = 10^(db*0.025)` gain mapping.
- **Proportional-Q law**: `bandWidth()` — tabulate the Q for |gain| = 0, 3, 6, 9, 12 dB.
  Say what that means musically (narrow at extremes, broad near flat) and what happens
  to the Q at exactly 0 dB (band bypassed at `Eq560.h:74`).
- Coefficient update policy: 8-sample counter, 20 ms smoothing coefficient, dirty flag.
  What is the worst-case zipper/step behaviour when automating a band at 48 kHz?
- Numerical type: double throughout — say where it matters (coefficient stability at
  31 Hz, integrator state).

### 3. Colour stage
`apiConsoleColor` — give the polynomial, the `tanh` and the `0.966` make-up. State the
harmonic character (2nd vs 3rd) implied by the even/odd terms, the amplitude where the
`tanh` starts to matter, and whether the stage is unity-gain at low level (prove it: the
`1.035` and `0.966` factors).

### 4. Parameter mapping
Table: parameter id → range → what it does in code → default. Include the ten `con_eq_*`
bands and the legacy `con_hpf/lf/lmf/hmf/hf/thresh/ratio/attack/release/out` parameters,
stating precisely which are read by DSP (cite where) and which are inert.

### 5. Interaction with the rest of the rack
- Does the EQ change level? (colour stage gain, band gains, any make-up) — quantify the
  worst case at ±12 dB on all ten bands *(inference is fine if labelled, but state the
  mechanism)*.
- How does it feed a following compressor: does EQ gain before/after a detector change
  the compression? Cite the detector in the other module and explain direction.
- Boosted low end + limiter: what happens to limiter gain reduction and to the auto-gain
  correction (`Source/Modules.h:357-360`)? Describe the mechanism, not a vibe.
- Does the module have latency of its own? (`getLatencySamples()` not overridden →
  answer with the citation.) Then explain why the reported host latency does not include
  the EQ's oversampling filters — is that a correctness issue? Say plainly yes/no and why.
- Aliasing: at 2× oversampling with a 4th-order Butterworth at 0.49887·sr, what folds
  back, and how loud is it likely to be for a +12 dB 16 kHz boost? *(inference, labelled)*

### 6. Re-order sensitivity
What changes if Console moves from slot 0 to a later slot: list every coupling that
breaks or appears (detector drive, limiter auto-gain reference, colour stacking order,
EQ-after-compression vs before). One short table: "slot position → consequence".

### 7. Hazards / defects you can see in the code
Anything that looks wrong: stale state when a band returns to 0 dB, the `reset()` policy,
channel-count assumptions (`std::min(2, ...)`), the missing EQ latency report, the
2× oversampling being fixed rather than sample-rate aware, etc. Rank by severity with
the citation.

## Output format

Markdown, ~200-320 lines, `##` sections matching the numbered headings above, tables
where tabulated, code blocks for formulas. End with a `## Sources checked` list of the
files+line ranges you actually read.
