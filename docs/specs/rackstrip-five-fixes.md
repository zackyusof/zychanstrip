# SPEC — Five reported defects: mix phase, auto-gain order, editor teardown, README duplication, badges

| Field | Value |
| --- | --- |
| Slug | `rackstrip-five-fixes` |
| Repo | `/Users/zack/projects/rackstrip` |
| Owner | zack |
| Created | 2026-11-01 |
| Classification | bug fix + layout change (pre-existing defects; not a release regression) |
| Version authority | **user** — the agent never picks a version, tag or release |
| Lifecycle | `~/.config/opencode/skills/dev-delegation/references/lifecycle.md` |
| Inner cycle | `~/.config/opencode/skills/dev-delegation/references/dev-cycle.md` |
| Pre-gate | `release-diff-first` — run BEFORE §1 (classification table below) |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | orchestrator | §1 | classification table |
| 2 | Design | done | orchestrator | §2 | DES-1…DES-6 |
| 3 | Build | done | inline (4 modules, one repo, one commit each) | §3 | ninja build |
| 4 | Test | done | inline | §4 | TST-1…TST-7 |
| 5 | Deploy | done | orchestrator | §5 | system-wide install + load proof |
| 6 | Maintain | done | orchestrator | §6 | MNT-1 |

## 1. Requirements

### release-diff-first classification (`v1.0..main`)

The branch differs from the released tag `v1.0` only by the JUCE 9 upgrade (`abc9872..647915c`:
`CMakeLists.txt`, `PluginEditor.cpp` SVG call, docs). **None of the five reports is a regression** —
each is either pre-existing or does not reproduce:

| # | Report | In `v1.0..main` diff? | Live verification | Verdict |
| --- | --- | --- | --- | --- |
| 1 | Dry/wet phase cancellation | no | `processBlock` copies a raw `dryBuffer` and sums it against the wet path with **no** delay | **PRE-EXISTING** — confirmed |
| 2 | Order-dependent auto-gain | no | `runLimiter` integrates `dryEnergy`/`wetEnergy` entirely inside the limiter (post upstream gain) | **PRE-EXISTING** — confirmed |
| 3 | Editor teardown / bad access | no | `masterFader`/`mixFader` are already declared **before** `masterAttachment`/`mixAttachment`; `Knob::slider` before `Knob::att`; dtor stops the timer, cancels animations, clears the LookAndFeel | **PRE-EXISTING, premise partly wrong** — one real latent hazard found (see DES-4) |
| 4 | Duplicated RESET bullets | no | `README.md:130-131` repeats two bullets verbatim after the RESET paragraph | **PRE-EXISTING** — confirmed |
| 5 | Broken shields badges | no | all five `img.shields.io` URLs return **200** with valid SVG and the correct label; none references Gitea | **DOES NOT REPRODUCE** — no change (report is inaccurate) |

### Scope
Fix #1, #2, #3(hazard only), #4. Record #5 as not-reproducing rather than churn the badges.

### Non-goals
- No version bump, no tag, no release (user's decision).
- No AUDIO change for 100 % wet sessions (the default) — the mix fix must be a no-op there.
- No new parameters; parameter IDs, ranges and defaults are untouched (AGENTS.md rule 1).
- No validation (user directive 2026-11-01).
- No fix for #5 — the badges are healthy; changing them would be churn against evidence.

### Requirements (observable)

| ID | Requirement | Acceptance check | Priority |
| --- | --- | --- | --- |
| REQ-1 | Dry path delayed by exactly the reported latency | At 50 % mix with a steady tone, output level equals the fully-wet level within 0.5 dB (was ~-24 dB) | must |
| REQ-1b | 100 % wet is bit-identical to before | mixed result == fully wet result at `master_mix` = 100 | must |
| REQ-2 | Auto-gain is order-independent | The same settings give the same output RMS for every one of the 24 rack orders (spread < 0.5 dB; was 7.6 dB) | must |
| REQ-3 | Editor teardown cannot leave dangling callbacks | `MeterPanel` callbacks cleared before the panel is released; harness runs the full construct/destroy cycle | must |
| REQ-4 | RESET bullets appear exactly once | Each bullet string appears once in `README.md` | must |
| REQ-5 | No regression | `tools/build_snapshot.py` → all checks pass (was 230/230) | must |
| REQ-6 | Limiter uses a 2x2 fader grid with full-height meters beside it | Harness asserts the grid geometry at all three sizes | must |

### Constraints
- Latency stays constant across every order/bypass combination (AGENTS.md rule 3).
- `processBlock` must stay allocation-free; the delay uses pre-sized buffers.
- The metering tap stays after master gain and mix (AGENTS.md rule 4).

### Risks
- Delay lines change the dry path's alignment with the *reported* latency; a mismatch would add comb
  filtering instead of removing it. Mitigation: REQ-1 measures the null directly.
- Order-independent auto-gain needs a reference from outside the limiter; a stale or wrong reference
  regresses loudness. Mitigation: REQ-2 measures across all 24 orders.

### Open questions

| ID | Question | Owner | Blocks design? |
| --- | --- | --- | --- |
| Q-1 | Should the version move for these fixes? | user | no |

## 2. Design

### Module map (exactly one owner per file; 0 unclaimed)

| Module | Owned globs | Owner | Notes |
| --- | --- | --- | --- |
| processor | `Source/PluginProcessor.{h,cpp}` | orchestrator | delay line + reference plumbing |
| dsp | `Source/Modules.h` | orchestrator | limiter reference input |
| editor | `Source/PluginEditor.{h,cpp}` | orchestrator | teardown hazard |
| harness | `tools/snapshot_main.cpp`, `tools/dsp_checks.h` | orchestrator | new checks |
| docs | `README.md`, `docs/specs/rackstrip-five-fixes.md` | orchestrator | |

One repo, four modules, one commit per fix — the work is sequential and the modules are coupled through
the DSP contract, so a fleet would add coordination cost without parallelism.

### Decisions

| ID | Decision | Rationale |
| --- | --- | --- |
| DES-1 | Delay the dry path with a **pre-sized stereo ring buffer** (`dryDelay[2]` + write index) written in chunks, not per sample | Keeps `processBlock` allocation-free and branch-light; `juce::dsp::DelayLine` would also work but adds a dependency and per-sample calls |
| DES-2 | Compensate only when `latency > 0`; at 100 % wet the dry path is **not even summed** (existing `wet < 0.9999f` branch) | REQ-1b: 100 % wet stays bit-identical |
| DES-3 | Auto-gain reference = **chain input energy** captured at `processBlock` entry, handed to the limiter as a linear-domain input, and completed with the limiter's measurement of the **final** output (post mix) | Order-independent by construction: input is order-invariant and output is the real output |
| DES-4 | Clear `meterPanel->onBack`/`onReset` in `~RackStripEditor` before the panel is released | The callbacks capture `this`; the panel is a member that is destroyed *after* the destructor body, so a timer/tick reaching it would call into a partially destroyed editor |
| DES-5 | No change for badges | Evidence: HTTP 200 + valid SVG |
| DES-6 | New harness checks + reset of module state between order permutations | REQ-1, REQ-1b, REQ-2 must be measured, not argued |

### Data and state
- New members: `dryDelay[2]`, `dryWp`, `dryDelayLen` (processor); `refInEnergy`, `refOutEnergy`,
  `refCoef`, `refValid` (limiter). No parameter, no state-schema change: nothing new is serialised.

### Invariants (must not break)

| ID | Invariant | Where |
| --- | --- | --- |
| INV-1 | Parameter IDs/ranges/defaults unchanged | `PluginProcessor.cpp` `createLayout()` — untouched |
| INV-2 | Rack order packing + permutation check unchanged | `PluginProcessor.cpp:162-182` |
| INV-3 | Latency reported once from `prepareToPlay()` | `PluginProcessor.cpp:98-111` |
| INV-4 | Metering tap last | `PluginProcessor.cpp` |
| DES-7 | Limiter control area split into a 2x2 grid, metering area kept full-height on the right | Matches the requested layout while keeping both ladders legible |
| DES-8 | Editor destructor clears every `this`-capturing callback before member release | Closes REQ-3: the meter page is destroyed after the destructor body, so a stale `onBack`/`onReset` would call into a half-destroyed editor |
| DES-9 | RESET copy de-duplicated to one bullet list | Closes REQ-4 |
| DES-10 | Harness stays the single regression gate; one new check set per fix | Closes REQ-5 |
| DES-11 | Grid geometry asserted by the harness at 864x608, 1080x760, 1620x1140 | Closes REQ-6: layout regressions are caught headlessly, not by eye |

### Verification strategy
Each REQ gets a harness check (TST-n) that fails on the old code and passes on the new.

### Rollback
Revert the fix commits; `v1.0` still builds.

## 3. Build

Configure with CMake 4.4.5 + Ninja 1.13.2 (downloaded to task scratch — this host has neither) against
JUCE 9.0.4. Build `zychanstrip_AU`.

## 4. Test

| REQ | TST | Test | Result |
| --- | --- | --- | --- |
| REQ-1 | TST-1 | 50 % mix vs wet, bypassed rack | PASS — max err **3e-8** (exact) |
| REQ-1b | TST-2 | 0 % mix returns untouched input; 100 % unchanged | PASS |
| REQ-2 | TST-3 | All 24 rack permutations, identical settings | PASS — spread **0.000 dB** (was 7.6 dB) |
| REQ-2b | TST-4 | Auto Gain correction vs master level | PASS — identical compensation |
| REQ-3 | TST-5 | Full construct/destroy of editor in harness | PASS |
| REQ-4 | TST-6 | RESET bullets appear once | PASS — 1 occurrence each |
| REQ-5 | TST-7 | Full harness | **PASS — 246/246** (230 original, all unchanged) |
| REQ-6 | TST-8 | Limiter 2x2 grid geometry at 3 sizes | PASS |

### Bugs found and fixed during the work (not in the original report)

1. **Off-by-one in the dry ring.** A ring read at `rd = wp + 1` already supplies one sample of delay, so
   sizing it to `latency` shifted the dry path by `latency - 1`, leaving a ~1 % residual. Sized to
   `latency + 1` → exact.
2. **Auto-gain feedback loop.** Measuring the final output *after* master gain made the correction chase
   its own attenuation (a −6 dB master pulled +2.9 dB of compensation). The reference is now the rack output
   *before* master gain, so it normalises the rack only.
3. **Pair timing.** The limiter runs before the block's output exists; input and output energies are now
   published together as the **previous** block's pair, so both sides describe one signal.

## 5. Deploy

| Item | Value |
| --- | --- |
| DEP-1 | Installed system-wide at `/Library/Audio/Plug-Ins/Components/zychanstrip.component`; no user copy |

System-wide only (`/Library/Audio/Plug-Ins/Components`), user copy removed. Validation: **user**.

## 6. Maintain

MNT-1: harness stays the regression gate (246 checks); README is the issue register; next review on the next bump.

## Follow-up (reported, not reproduced)

**#5 badges** — all five `img.shields.io` URLs return HTTP 200 with valid SVG and the correct label, and
none of them references Gitea. No change made: editing healthy badges would be churn against evidence. If a
badge still renders broken client-side, the cause is caching/proxy, not the URL.
