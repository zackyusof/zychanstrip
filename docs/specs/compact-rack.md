# SPEC — Compact proportional rack and master output

| Field | Value |
| --- | --- |
| Slug | compact-rack |
| Classification | feature + pre-existing bug |
| Version authority | user; keep existing 0.1.0 |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | root | §1 | user request + source reproduction |
| 2 | Design | done | root | §2 | ownership gate: 11 tracked files, 0 unclaimed, 0 overlaps |
| 3 | Build | done | experts | §3 | CMake/Ninja exit 0, installed arm64 AU |
| 4 | Test | done | verifier | §4 | independent harness 66/66, 3 snapshots reviewed, AU validation passed |
| 5 | Deploy | done | root | §5 | installed AU validated; artifact SHA-256 recorded below |
| 6 | Maintain | done | root | §6 | shared memory 24828; regression harness and README updated |

## 1. Requirements

User reports oversized GUI, decorative window symbols and a nonfunctional master slider.

| ID | Requirement | Acceptance check | Priority |
| --- | --- | --- | --- |
| REQ-1 | Remove painted x/minus/circle controls | snapshot has no imitation window buttons | must |
| REQ-2 | Master changes final output and saves/automates | bypass modules; measure gain, UI attachment and state roundtrip | must |
| REQ-3 | Narrower proportionate rack | base design about 1080x760; all controls/labels fit | must |
| REQ-4 | Original hardware family colors | dark console with band colors, light silver opto, black/silver 1176, blue-gray SSL limiter | must |
| REQ-5 | Resize at a constant ratio; controls/reorder stay usable | snapshot at min/default/max, constrainer and scaled drag checks | must |

No parameter IDs/ranges removed; master added at end, unity default for old sessions. No version/tag/release requested. No module DSP redesign. Original faces inferred from existing documented 550B/LA-2A/1176/SSL references.

## 2. Design

| Module | Files | Owner |
| --- | --- | --- |
| gui | Source/PluginEditor.* | gui-expert |
| audio | Source/PluginProcessor.* | audio-expert |
| integration | tools/**, docs/**, README.md, CMakeLists.txt, .gitignore, Source/Modules.h | root |

| ID | Decision | Covers |
| --- | --- | --- |
| DES-1 | Delete decorative glyph drawing | REQ-1 |
| DES-2 | Append master_level parameter (-60 to +6 dB, unity default); final smoothed gain after module chain; slider attachment | REQ-2 |
| DES-3 | Narrow logical design and uniform component transform for resize; fixed aspect constrainer | REQ-3, REQ-5 |
| DES-4 | Restore hardware face/knob palette; keep current neutral labels | REQ-4 |

Invariant: Source/PluginProcessor.cpp createLayout existing IDs stay stable; processBlock order unchanged. Editor grip and animation coordinate system remain logical. Reference: JUCE 8.0.4 AudioProcessorEditor and ComponentBoundsConstrainer implementation, APVTS SliderAttachment and dsp Gain. Read exact upstream files from configured JUCE source before implementation.
Rollback: committed baseline plus task scratch backup and previous installed AU. Verification: built AU, numeric audio test, rendered snapshots, independent verifier. No tags exist after git fetch --tags origin, so no released-tag diff available; baseline 09eaf98.

## 3. Build

Configure JUCE 8.0.4 using CMake/Ninja in task scratch. Build AU and snapshot harness; install using existing copy-after-build. GUI and processor owners edit only assigned files; integration owns harness/docs. Keep version unchanged.

## 4. Test

| ID | Test | Covers | Type | Command | Result |
| --- | --- | --- | --- | --- | --- |
| TST-1 | Inspect three sizes of rendered editor | REQ-1, REQ-3, REQ-4, REQ-5 | UI | snapshot harness + visual review | all 3 sizes render without clipped controls; captions corrected after independent review |
| TST-2 | Master gain, state, smoothing, UI attachment, mono/stereo | REQ-2 | audio/UI | snapshot harness | 16 gain cases, stereo ramp, active-chain comparison, current/legacy state, attachment and reset pass |
| TST-3 | Aspect constraints and scaled reordering | REQ-5 | UI | snapshot harness | min/default/max, off-ratio correction, 3 scaled grip reorders pass |

Red reproduction: master slider has only setRange/setValue and no attachment; processBlock ends after module loop. No master parameter. Editor lacks setResizable and hardcoded controls do not scale. Independent verifier reran the real harness: 66/66 passed. Final caption spacing was also rebuilt and visually checked. The four initial false failures used exact float equality or disallowed 1px transformed-bounds rounding; corrected with 1e-5 dB and 1px tolerances after confirming JUCE normalization and transforms. No functional code change was needed for those checks. DAW listening and host-specific mouse behavior require user evaluation.

## 5. Deploy

DEP-1: Build/install local AU, ad-hoc signature, record artifact hash and validation. Version authority: user; no change. Existing AU backed up under task scratch until verification succeeds. Gitea private origin confirmed; no public publication. README updated for working master and resize limits.

## 6. Maintain

MNT-1: root maintains regression harness and documentation; user reports through this chat. Run numeric and layout checks on subsequent GUI changes. Persist verified result in shared memory. No external monitoring needed for local AU. Retain known host-validation limits in report.

## Traceability

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1 | DEP-1 | MNT-1 |
| REQ-2 | DES-2 | TST-2 | DEP-1 | MNT-1 |
| REQ-3 | DES-3 | TST-1 | DEP-1 | MNT-1 |
| REQ-4 | DES-4 | TST-1 | DEP-1 | MNT-1 |
| REQ-5 | DES-3 | TST-3 | DEP-1 | MNT-1 |

## Evidence log

- 2026-10-31: startup Redis and MariaDB available; shared session-context key empty.
- Fetch tags found no release tags; no released-tag comparison possible. Existing baseline preserved in Git.
- Read complete dev-delegation skill, dev-cycle, lifecycle, verification and execution-contract: 425 lines.
- Reference implementations: JUCE 8.0.4 `juce_AudioProcessorEditor.cpp`, `juce_ComponentBoundsConstrainer.cpp`, `juce_Component.cpp`, `juce_ParameterAttachments.cpp`, `juce_AudioProcessorValueTreeState.cpp`, `juce_SmoothedValue.h`; adapted child transforms, parameter attachments and channel-linked gain smoothing.
- Build uses CMake and Ninja in task scratch with JUCE pinned at 8.0.4. Existing compiler warnings are not errors.
- Independent installed-artifact check: `codesign --verify --deep --strict` exit 0; `auval -v aufx Rkst Ycmp` exit 0, 29 parameters, custom Cocoa UI and rendering validation pass. Previous spec's registrar limitation did not reproduce on this host.
- Actual listening and interactive DAW use not verified. Automated mouse events exercise real panel handlers; host-specific gestures remain user evaluation.

- Final installed artifact SHA-256: `314ca8c5c73d5ddb66710dd34771a7d06f2f3b10da315e3b012eea227b6e79d8` (`Rack Strip.component/Contents/MacOS/Rack Strip`).
- Verified learnings persisted in shared memory entry 24828. GUI snapshots retained as Codex visualization artifacts; temporary build and backup removed after verification.

### Task evidence

TASK: compact proportional GUI and functional master. MODULES: editor, processor, integration.
FILES TOUCHED: Source/PluginEditor.cpp/.h; Source/PluginProcessor.cpp/.h; tools/build_snapshot.py; tools/snapshot_main.cpp; README.md; this spec.
COMMANDS: CMake/Ninja build exit 0; snapshot harness 66/66; auval exit 0; codesign exit 0; spec strict lint passes.
REFERENCES: exact JUCE 8.0.4 implementation files listed above (adapted); existing hardware family palette retained/restored.
VERIFIED: numeric audio/master/state/binding, rendered layouts and handler-level dragging, AU validation/signature. NOT VERIFIED: listening and OS-level DAW interaction.
COMMIT: logical fix commit in private Gitea history (no version/tag change). HAND-OFF: user evaluates sound/native host feel.

- Independent review caught snapshot PNG append-on-rerun behavior in the old helper. Harness now resets position and truncates before writing; regenerated screenshots contain only the current image.
