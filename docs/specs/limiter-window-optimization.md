# SPEC — Limiter Window Optimization

| Field | Value |
| --- | --- |
| Slug | limiter-window-optimization |
| Classification | optimization + pre-existing behavior |
| Version authority | user; version stays pending; propose 1.0.2-beta.1 as pending candidate only |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | Orchestrator | §1 | user request + release-diff classification |
| 2 | Design | done | Planner | §2 | fixed-capacity deque design, rollback |
| 3 | Build | done | Engine Builder | §3 | Ninja build, AU install, signed artifact |
| 4 | Test | done | Validator | §4 | 253/253 harness, exact reference checks |
| 5 | Deploy | blocked | Orchestrator | §5 | waiting for maintainer-selected beta version |
| 6 | Maintain | pending | Maintainer | §6 | soak starts after approved release |

## 1. Requirements

| ID | Requirement | Acceptance check | Priority |
| --- | --- | --- | --- |
| REQ-1 | Replace full reqRing scan with fixed-capacity monotonic sliding-minimum | O(1) amortized minimum computation; bit-exact output against naive scan for all valid req in [0,1] at all sample positions and rack orders; existing latency, order, bypass, parameters unchanged | must |
| REQ-2 | Add focused equivalence tests against naive initialized-ones ring | Tests cover window sizes 1, 2, 3, 67, 72, 144, 288; rising/falling/constant/repeated values; wrap and reset; `check()` assertions in `tools/dsp_checks.h` verify optimized `mn` matches naive scan | must |
| REQ-3 | No true-peak or oversampling extensions | Scope is strictly the sliding-minimum optimization; no true-peak detection, multi-sampling, or frequency-domain extensions | must |

## 2. Design

Release-diff classification: PRE-EXISTING. `v1.0.1..HEAD` contains rename/GUI work and no limiter DSP changes; the scan exists at `Source/Modules.h:408-410`.

| ID | Decision | Covers |
| --- | --- | --- |
| DES-1 | Fixed-capacity minimum helper (values ring + monotonic logical-index deque) | REQ-1 |
| DES-2 | Preserve exact output via equivalence migration | REQ-1, REQ-2 |
| DES-3 | No parameter or layout changes | REQ-1 (out of scope for IDs/ranges/defaults) |
| DES-4 | Rollback is a no-op | REQ-1 (revert loop only) |

**DES-1: Fixed-capacity monotonic minimum queue**

The helper owns fixed-capacity candidate value, sequence-ID, and physical-slot arrays, each sized to `la`, allocated in `prepare()`. It also owns a request values ring initialized to `1.0f`, matching `reqRing`. Sequence IDs are monotonically increasing `uint64_t`. The practical overflow horizon is far beyond audio use; if counter renormalization is added later, it must have a dedicated wrap test. Physical slots are stored separately from sequence IDs.

**Reset:** Fill request ring with `1.0f`; clear deque head/count and set sequence `t=0`. Seed `la` candidates with IDs `0 ... la-1`, all values `1.0f`, and explicit physical slots `0 ... la-1`. This exactly represents the old initialized window.

**Push (`req` is finite and in `[0,1]`):**
1. Expire deque-front candidates when `t - id >= la`.
2. Write `reqRing[t % la] = req`.
3. Pop deque-back candidates while their value is `>= req`.
4. Append current sequence ID and physical slot to the deque; store the corresponding request value. When all candidates were popped, preserve the advanced circular head and write at that head.
5. Return the value at the deque front.
6. Increment `t` with unsigned arithmetic. No runtime reset/renormalization path is currently used.

Deque occupancy is maintained as a circular buffer; each candidate is inserted once and removed at most once, giving amortized O(1) operations and O(la) fixed storage. No claim is made to Lemire's worst-case comparison bound.

**DES-2: Fixed-capacity initialization and reset**

- In `prepare()`, allocate fixed-capacity deque storage; `reset()` fills the request ring and initializes all sentinel candidates without allocation.
- In `runLimiter`, replace the request write and full `reqRing` scan at `Source/Modules.h:408-410` with one helper push. Keep `reqRing` storage, `avgRing`, summation, gain reduction, saturation, dry/wet, and latency logic unchanged.

**DES-3: Fixed allocation, no parameter or layout changes**

- No new parameters, no ID reassignment, no range or default changes. `reqRing` and `avgRing` remain; the new fixed-capacity candidate deque is allocated in `prepare()`.
- State migration in `setStateInformation` is unaffected.

## 3. Build

Build and test commands from README:

```bash
# Build the AU
export PATH="$HOME/Library/Python/3.13/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# Headless snapshot harness
python3 tools/build_snapshot.py
./build/snapshot/snapshot "$HOME/.config/opencode/temp/zychanstrip-dsp-beta/shots"
# Expect every existing and newly added check to pass; record the actual count.
# Remove the task temp directory when the work ends.
```

## 4. Test

**TST-1: Equivalence tests (new, in `tools/dsp_checks.h`)**

- Run `python3 tools/build_snapshot.py` and confirm all checks pass, including the new limiter equivalence tests.
- The test harness in `tools/dsp_checks.h` will contain `check()` assertions comparing the optimized `mn` against the naive scan result across all test cases:
  - Window sizes: 1, 2, 3, 67, 72, 144, 288
  - Patterns: rising, falling, constant, repeated values, wrap and reset
- The exact new total is base count plus added checks, not 246 unchanged.
- Compare the optimized helper to a literal initialized-ones ring scan on every pushed sample; include each window size, wraparound, and reset/reinitialization.

## 5. Deploy

BLOCKED: version, tag, and release identity require explicit maintainer selection.

**DEP-1: Deploy**

- Build and install the AU; verify ownership, `codesign -v`, plist identity/version, and AU factory loadability per README. `validation: user`; do not run auval/pluginval.
- Version: `1.0.2-beta.1` is proposed as a **pending candidate only** — never apply, commit, tag, or publish without explicit approval. The tag/version remains the maintainer's decision.

## 6. Maintain

**MNT-1: Maintain**

- Owner monitors subsequent DSP changes for regressor in the minimum-computation path.
- If the monotonic queue is modified, re-run the equivalence test suite from TST-1.
- Persist verified result in shared memory; no external monitoring needed for local AU.

## Traceability

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1 | DEP-1 | MNT-1 |
| REQ-2 | DES-1, DES-2 | TST-1 | — | — |
| REQ-3 | DES-1 (out of scope) | TST-1 (out of scope) | — | — |

## Evidence log

- Lemire 2006, arXiv cs/0610046: "Streaming Maximum-Minimum Filter Using No More than Three Comparisons per Element", as related prior art only, not proof of this deque bound.
- Prior art consulted: JUCE current limiter docs, Lemire streaming min-max filter
- Release classification: PRE-EXISTING; latest published release verified as `v1.0.1`, DSP scan absent from post-tag diff.
- No parameter IDs, ranges, or defaults changed
- Findings for v1.0.1..HEAD classified PRE-EXISTING; version authority pending
- `1.0.2-beta.1` pending candidate only

## End RECEIPT: PLANNER | input verified | output | GATES | NOT VERIFIED | HAND-OFF

- **input verified**: Req/Des/Test sections trace to Source/Modules.h and tools/dsp_checks.h; Source/Modules.h = Engine DSP, tools/dsp_checks.h = QA, docs/dsp/module-limiter.md + index = Docs, spec file = Planner.
- **output**: docs/specs/limiter-window-optimization.md created with phases 1-6 explicit; REQ→DES→TST observable; build/harness commands from README, DS5 window updates, temp directory documentation, evidence classification, lint command cleared.
- **GATES**: dev_spec lint verified 29/29; baseline CMake/Ninja build and snapshot harness verified 246/246; post-change snapshot harness verified 253/253; window-equivalence checks pass for sizes 1, 2, 3, 67, 72, 144, and 288 across wrap and reset; independent randomized Python equivalence verified for 100,000 samples at each requested window size; bundle `codesign -v`, metadata, AU factory Open/Close verified.
- **NOT VERIFIED**: Beta version metadata, system-wide install, commit/push, and Gitea release remain pending; 1.0.2-beta.1 is a pending candidate without approval. Maintain/soak is not yet started. Counter-renormalization is not tested or claimed.
- **HAND-OFF**: Core implementation and validation complete; deployment awaits explicit version selection. No untracked DSP_RESEARCH_ANALYSIS.md changes. Central library probed absent.
