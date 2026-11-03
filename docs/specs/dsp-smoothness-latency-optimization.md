# SPEC — DSP smoothness, real-time safety, and latency optimization

| Field | Value |
| --- | --- |
| Slug | `dsp-smoothness-latency-optimization` |
| Repo | `/Users/zack/projects/zychanstrip` |
| Track | dsp-smoothness-latency-optimization |
| Owner | zack |
| Created | 2026-11-03 |
| Classification | DSP optimization / realtime safety / latency study |
| Version authority | **user** (the agent never invents a version, tag or release) |
| Lifecycle | `~/.config/opencode/skills/dev-delegation/references/lifecycle.md` |
| Inner cycle | `~/.config/opencode/skills/dev-delegation/references/dev-cycle.md` |

> Rule: all six phases are accounted for. Work them in order; record `n/a — <reason>` where a phase
> genuinely does not apply. `done` requires evidence in the row. Update this file as the work moves,
> never at the end.

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | Orchestrator | §1 | User request; source audit and external prior art recorded 2026-11-03 |
| 2 | Design | in-progress | Planner | §2 | Measurement-first staged plan; tradeoffs and open choices captured |
| 3 | Build | pending | | §3 | |
| 4 | Test | pending | | §4 | |
| 5 | Deploy | pending | | §5 | |
| 6 | Maintain | pending | | §6 | |

Status values: `pending` · `in-progress` · `done` · `blocked` · `n/a — <reason>`.

## 1. Requirements

### Problem
The request asks for smoother behavior, lower latency, and efficient DSP across the four-module rack. Those goals can conflict: look-ahead improves transient control at the cost of delay; oversampling reduces nonlinear aliasing but costs CPU and can add group delay; smoothing reduces zipper/click artifacts but changes response time. “Lowest latency” and “smooth as butter” need explicit measurements and signal-quality constraints rather than unbounded rewrites.

### Users and outcomes
Musicians using the AU should get click-free parameter/order/bypass transitions, stable processing without audio-thread allocation spikes, predictable reported latency, and no unexplained CPU regressions. Maintain the current tone and automation/session compatibility unless a specific change is separately justified.

### Scope
Profile the complete signal path and each module; fix verified audio-thread hot spots; improve parameter smoothing and state transitions where tests demonstrate discontinuities; evaluate the limiter look-ahead and EQ oversampling options against measured quality/latency/CPU tradeoffs; document results and update DSP measurements.

### Non-goals
No claim that software can make all algorithms simultaneously zero-latency, artifact-free, transparent, and minimum-CPU. No broad tonal redesign, parameter ID/range/default changes, new controls, stable release, or third-party code copying. External repositories are design references only; do not import code without license review and a scoped design decision.

### Requirements (observable)

| ID | Requirement | Acceptance check (observable) | Priority |
| --- | --- | --- | --- |
| REQ-1 | Establish repeatable baseline CPU, allocation, and latency measurements before optimization | Record per-module and full-chain CPU/real-time factor at 44.1/48/96/192 kHz and 32/64/128/256/512 sample blocks, with warm-up and fixed signals; report max/median and methodology | must |
| REQ-2 | Make the audio callback allocation-free for all supported host block sizes | Instrument allocator in harness; exercise oversized blocks, mono/stereo, reset, bypass, and mix changes; zero allocations after prepare, or document a bounded host contract if JUCE forces otherwise | must |
| REQ-3 | Eliminate audible discontinuities on supported real-time control transitions | Automated step/ramp tests for every module's key controls, bypass, rack order, reset, master/mix and Unity; bounded sample discontinuity and settling-time criteria set from baseline/listening protocol before implementation | must |
| REQ-4 | Reduce host-reported latency without silently weakening limiter safety or nonlinear quality | Measure reported and impulse-derived delay for each candidate. Keep current default safety/quality unless a measured alternative preserves agreed peak, alias, and transient targets; expose a separate mode only if user approves new parameter and compatibility design | must |
| REQ-5 | Optimize CPU only where profiling identifies a material hotspot | Each optimization shows repeatable improvement beyond measurement noise and passes exact/reference, stress, and regression tests for affected behavior | must |
| REQ-6 | Preserve session, automation, ordering, bypass and metering contracts | Existing IDs/ranges/defaults/state migration remain compatible; existing harness plus new tests pass; update `docs/dsp/` and measurements with measured evidence | must |

### Constraints
Follow `AGENTS.md`: JUCE/macOS arm64 AU is the supported build/install target; latency must remain constant across rack order/bypass; keep parameter compatibility; commit/push Gitea; global install only from a clean latest commit; never use auval/pluginval. Current documented limiter look-ahead is `ceil(0.0015 * sr)` (about 71 samples at 48 kHz), and EQ560 already oversamples 2×. The current snapshot suite is DSP/GUI correctness, not a CPU benchmark.

### Risks
Potential audio signature changes, oversampling/group-delay changes, altered compressor attack/release envelopes, transient overshoot if look-ahead is reduced, CPU increase from higher quality, block-size-specific regressions, low sample-rate edge cases, and parameter automation clicks. A benchmark can be noisy; use repeated runs and report variance. Existing repo docs contain stale/contradictory latency claims that must be reconciled during implementation.

### Open questions

| ID | Question | Owner | Blocks design? |
| --- | --- | --- | --- |
| Q-1 | Should the default limiter retain look-ahead safety, with low-latency as an explicit alternate mode, or should minimum reported latency override current safety/character? | User | yes |
| Q-2 | What maximum limiter overshoot and nonlinear alias level are acceptable when trading latency/CPU? | User + DSP measurements | yes |
| Q-3 | Is low-latency operation an optional profile/control (requires appended automation ID) or a fixed internal redesign? | User | yes |
| Q-4 | What measured CPU budget defines “material” for this target Mac and typical DAW session? | Maintainer; baseline can inform | no |

## 2. Design

### Module map (exactly one owner per file; 0 unclaimed)

| Module | Owned globs | Owner | Notes |
| --- | --- | --- | --- |
| DSP engine | `Source/Modules.h`, `Source/Eq560.h` | Engine | Compressors, limiter, EQ, saturation and module-level state |
| Processor chain | `Source/PluginProcessor.{h,cpp}` | Engine | Host block processing, dry/wet delay, master/Unity smoothing, order and latency reporting |
| Metering | `Source/Meters.h` | Engine | Audio-thread peak/RMS/scope/RTA collection; audit allocation and transfer path |
| Verification | `tools/dsp_checks.h`, `tools/snapshot_main.cpp`, `tools/research/**` | Validator | Correctness, allocation, CPU benchmark, impulse/alias and transition tests |
| DSP documentation | `docs/dsp/**`, `README.md` | Docs | Signal-path contracts, measured latency/CPU/quality, user-facing tradeoffs |
| Plan | `docs/specs/dsp-smoothness-latency-optimization.md` | Planner | Requirements, design choices, evidence, gates |

### Data and state
No new state or automation parameter is presumed. Any optional latency/quality mode must append a new stable parameter ID, add legacy-state migration, preserve old-session behavior, and receive user approval before implementation. Benchmark-only counters stay out of the release audio path or compile out of production.

### Interfaces and seams
Measurement harness should call the same processor/module path as production and isolate timing from rendering/UI. Audio callback owns processing; UI reads published atomics only. `prepareToPlay` owns all buffer/smoother allocation and latency calculation; `processBlock` must not resize/assign vectors. Reported plugin latency remains integer samples and dry/wet alignment must follow the selected algorithmic latency.

### Invariants (must not break)

| ID | Invariant | Where (`path:line`) |
| --- | --- | --- |
| INV-1 | Existing parameter IDs, ranges, defaults, and state compatibility remain unchanged unless approved | `Source/PluginProcessor.cpp` parameter layout and state migration |
| INV-2 | Reported latency is independent of rack order and bypass | `Source/PluginProcessor.cpp` prepare/order processing |
| INV-3 | Dry/wet paths are sample aligned for the reported wet-path latency | `Source/PluginProcessor.cpp` mix/delay path |
| INV-4 | No heap allocation, lock, I/O, or unbounded work in the audio callback | `Source/PluginProcessor.cpp`, `Source/Modules.h`, `Source/Meters.h` |
| INV-5 | Smoothers are sample-rate aware and stable for every supported block size/sample rate | `Source/Eq560.h`, `Source/Modules.h`, `Source/PluginProcessor.cpp` |

### Alternatives considered
1. Cut limiter look-ahead immediately: rejected as a blind change; it trades away transient safety and makes downstream dry/wet alignment change.
2. Raise oversampling everywhere: rejected without alias and CPU measurements; more oversampling costs CPU and can add delay.
3. Optimize by code inspection alone: rejected; first establish per-module CPU and allocation baselines.
4. Adopt JUCE primitives where they fit and verify their exact behavior/latency: preferred over importing third-party algorithms wholesale.

### Verification strategy (how each requirement proves itself)
Create reproducible baseline and optimized builds using identical compiler, architecture, sample data and runtime; repeat runs and report median/p95/max plus variance. Add callback allocation instrumentation and oversized-block tests. Compare output against pre-change references for unchanged modes; run impulse/null alignment, full-scale transient ceiling, alias-spectrum, parameter step/ramp/click tests, 24 rack orders and bypass permutations. Re-run existing 259-check snapshot harness. Any candidate that reduces delay must report both DAW-reported samples and impulse alignment and document phase/group-delay behavior.

### Rollback
Each stage is a separate commit and can be reverted independently. Keep current limiter look-ahead and 2× EQ oversampling as the fallback until an alternative passes quality gates. Do not add or migrate parameters until its mode decision is accepted.

### Deploy topology
Build arm64 AU on macOS; Gitea `main`; install globally via `tools/install_global.sh` only after clean pushed HEAD. Host validation remains `validation: user` per repo rules.

### Observability
Record commit, compiler/toolchain, device/CPU model, sample rate, block size, channel count, buffer/allocation counts, reported/impulse latency, CPU distribution and signal metrics for every run. Do not use GUI timer/meter CPU as DSP callback timing.

### Design decisions

| ID | Decision | Covers | Rejected alternative |
| --- | --- | --- | --- |
| DES-1 | Measurement-first staged optimization; profile audio callback, modules and allocations before changing algorithms | REQ-1, REQ-2, REQ-5 | Guess-driven micro-optimization |
| DES-2 | Separate latency from smoothness/quality; evaluate limiter look-ahead and EQ oversampling as explicit tradeoff candidates | REQ-3, REQ-4 | Treat “lowest latency” as unconditional removal of look-ahead |
| DES-3 | Preserve existing sound/defaults and session compatibility as acceptance constraints | REQ-3, REQ-6 | Unreviewed parameter or tone changes |

## 3. Build

### Build plan
Not started. After questions Q-1–Q-3 resolve and design lint passes: baseline profiler/allocation harness; prioritize proven callback allocation risks (`dryBuffer.setSize` fallback in `processBlock`, dry delay sizing guard in mix path) and measured CPU hotspots; then implement small independently reversible changes.

### Files and globs
Expected only after design approval: `Source/{Modules.h,Eq560.h,PluginProcessor.cpp,Meters.h}`, `tools/{dsp_checks.h,snapshot_main.cpp,research/**}`, `docs/dsp/**`, `README.md`, this spec. Existing implementation behavior remains unchanged in this planning update.

### Dependency / version pins

### Build commands

```sh
# pending design/requirements gates; macOS arm64 only
```

### Deviations from design

## 4. Test

### Test plan (requirement -> test)

| ID | Test | Covers | Type | Command | Result |
| --- | --- | --- | --- | --- | --- |
| TST-1 | Reproducible per-module/full-chain baseline and post-change benchmark | REQ-1, REQ-5 | performance | planned | pending |
| TST-2 | Audio callback allocation counter incl. oversized blocks and all paths | REQ-2 | realtime-safety | planned | pending |
| TST-3 | Transition, null/impulse alignment, ceiling, alias and automation suite | REQ-3, REQ-4, REQ-6 | DSP regression | planned | pending |
| TST-4 | Existing GUI/DSP snapshot harness | REQ-6 | regression | `python3 tools/build_snapshot.py` then snapshot | pending |

### Red-first evidence (bugs only)

### Independent verification

### Known gaps / not verified
No profiling, audio-thread allocation instrumentation, alias analysis, listening review, or latency redesign has been performed yet. Current latency/signal docs contain contradictions; reconcile from implementation before claiming improvements.

## 5. Deploy

### Version and authority
No version change proposed. User controls version, tag and release.

### Release diff (patch/beta only)

### Packaging, signing, notarization

### Staging / install
Pending implementation. Follow clean commit → build → global install; do not install dirty-tree experimental artifacts.

### Smoke on the real artifact

### Rollout and rollback

### Notes and changelog

### Distribution identity / anonymity gate (public targets)

### Deployment record

| ID | Step | Command | Evidence |
| --- | --- | --- | --- |
| DEP-1 | | | |

## 6. Maintain

### Monitoring and alerts
After each accepted optimization, retain before/after benchmark artifacts and monitor callback deadline headroom on the target Mac in representative sessions.

### Support intake

### Known issues

### Deprecation

### Docs and skill updates
Update `docs/dsp/measurements.md`, affected module pages, signal-path latency description, README and this spec with actual measured values and limitations.

### Feedback loop (incident -> new requirement)

### Maintenance record

| ID | Item | Owner | Cadence | Last verified |
| --- | --- | --- | --- | --- |
| MNT-1 | | | | |

## Traceability

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1 | DEP-1 | MNT-1 |
| REQ-2 | DES-1 | TST-2 | DEP-1 | MNT-1 |
| REQ-3 | DES-2 | TST-3 | DEP-1 | MNT-1 |
| REQ-4 | DES-2 | TST-3 | DEP-1 | MNT-1 |
| REQ-5 | DES-1 | TST-1 | DEP-1 | MNT-1 |
| REQ-6 | DES-3 | TST-3, TST-4 | DEP-1 | MNT-1 |

## Evidence log

| Date | Phase | Command / check | Result |
| --- | --- | --- | --- |
| 2026-11-03 | Requirements/Design | Repository source/docs audit via current Herdr workspace | Verified current reported limiter latency is look-ahead-based; EQ560 documents 2× oversampling; identified callback buffer growth/size guards for profiling. No implementation changed. |
| 2026-11-03 | Prior art | JUCE GitHub and JUCE Oversampling API | JUCE repository has 9.1k stars; official API documents IIR oversampling as lower latency with phase compromise, FIR as linear-phase with greater latency. This confirms the quality/latency tradeoff; not a blanket recommendation. |
| 2026-11-03 | Prior art | ZeroLimit GitHub README | 19 stars; useful low-latency limiter comparison with zero host latency and explicit empirical null/latency verification. Lower social proof than JUCE, so treated as a design example only, not authoritative evidence. |
| 2026-11-03 | Prior art | Airwindows GitHub org | Large open-source DSP catalogue (main repo shown with 1.3k forks); useful algorithm survey, but no third-party code adoption is planned. |

### Prior-art links

- [JUCE framework repository](https://github.com/juce-framework/JUCE) — approximately 9.1k stars; primary reference for this JUCE codebase.
- [JUCE `dsp::Oversampling` source/API](https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/processors/juce_Oversampling.h) — explicitly describes FIR linear-phase / higher latency versus IIR lower latency / phase compromise, and oversampling's anti-aliasing purpose.
- [ZeroLimit](https://github.com/Jun-Murakami/ZeroLimit) — 19 stars; a lower-rated but relevant zero-host-latency limiter design example. Its README recommends empirical DAW latency/null checks.
- [Airwindows source catalogue](https://github.com/airwindows/airwindows) — broad open-source DSP reference; inspect licensing per algorithm before any reuse.

Social stars are a discovery signal only, not proof of correctness or sound quality. The plan favors JUCE's primary implementation/API evidence and project-local measurement; it does not propose copying a third-party implementation.
