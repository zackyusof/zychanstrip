# SPEC — EQ560 model and Enhance Auto Gain

| Field | Value |
| --- | --- |
| Slug | api560-autogain |
| Classification | feature |
| Version authority | user; unchanged 0.1.0, no tag or release |

## Phase status

| # | Phase | Status | Owner | Artifact | Evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Requirements | done | root | §1 | screenshot + user scope reply |
| 2 | Design | done | root | §2 | module gate12files,0unclaimed,0overlaps |
| 3 | Build | done | audio/gui | §3 | CMake/Ninja exit0, installedAU, MITresourcepresent |
| 4 | Test | done | verifier | §4 | independent195/195, AUvalidation40params, visualreview |
| 5 | Deploy | done | root | §5 | installedhash+signature+validationrecordedbelow |
| 6 | Maintain | done | root | §6 | regressionharness,README,sharedmemoryupdated |

## 1. Requirements

| ID | Requirement | Acceptance check | Priority |
| --- | --- | --- | --- |
| REQ-1 | Replace EQ with ten-band EQ560 using a well-rated GitHub implementation | source attribution, frequency response agrees with pinned upstream algorithm | must |
| REQ-2 | GUI follows attached560 face | dark green/charcoal, cyan grid, horizontal white faders with red500,16K..31, +/-12dB | must |
| REQ-3 | Enhance-module-only Auto Gain | changing Enhance/gain/ceiling/release matches module input/output RMS after settling, limited by ceiling | must |
| REQ-4 | Keep compact resizing, master, module order, host IDs | existing regressions pass, old IDs/indexes unchanged, new state roundtrip | must |

User explicitly selected Enhance module only. EQ core source: chkhld/jsfx Equalizer/eq_560.jsfx, MIT,156stars observed2026-10-31. No claim of manufacturer-certified hardware equivalence. Preserve compressor section within console panel. Existing four-band parameters remain registered for automation compatibility but become legacy inactive EQ controls; new graphic-band values start neutral. Legacy sessions missing Auto Gain restore off; new instances on. No version bump requested.

## 2. Design

| Module | Owner | Files |
| --- | --- | --- |
| audio | audio | Source/Modules.h, Source/PluginProcessor.*, Source/Eq560.h, THIRD_PARTY_NOTICES.md |
| gui | gui | Source/PluginEditor.* |
| tests | verify | tools/** |
| integration | root | README.md, docs/**, CMakeLists.txt, .gitignore |

| ID | Decision | Covers |
| --- | --- | --- |
| DES-1 | Port upstream gain-dependent SVF peak filters and2x oversampling; include full MIT notice, pinned commit and adaptation notes | REQ-1 |
| DES-2 | Ten horizontal slider rows descending16k..31Hz, +/-12dB, red500Hz cap; keep existing dynamics compact below | REQ-2 |
| DES-3 | Stereo-linked RMS auto level correction around limiter/Enhance only, latency-aligned dry reference, bounded/smoothed compensation and silence gate; final ceiling respected | REQ-3 |
| DES-4 | Append10band parameters con_eq_16k/con_eq_8k/con_eq_4k/con_eq_2k/con_eq_1k/con_eq_500/con_eq_250/con_eq_125/con_eq_63/con_eq_31 and lim_auto_gain after existing29. Preserve master and ratios; default new gains0 | REQ-4 |

Seams: GUI binds above IDs; audio owns processor migration and Eq560 core. Auto Gain has visible toggle named Auto Gain, default on for new instances; state migration inserts off for old sessions. Match is an RMS volume aid, not LUFS normalization; matching input louder than ceiling is bounded by peak safety. Do not compensate upstream EQ/compressors or manual master trim. No extra auto-gain latency. Eq oversampling uses documented filter implementation; bypass preserves consistent reported timing.

Rollback: baseline90c4ff3 plus backed-up installed AU under task scratch. No released tags after fetch. Invariants: getOrder/setOrder remain4modules; master output after rack; no realtime heap allocations in new DSP paths. Verify source/pinned coefficient math, measured response, silence, extreme settings, changing gains, stereo linking and bypass.

## 3. Build

JUCE8.0.4 C++17 arm64; configure/build CMake/Ninja in task scratch. Install local AU via existing copy-after-build. No public publication. Build snapshot harness from exact shared objects.

## 4. Test

| ID | Test | Covers | Type | Command | Result |
| --- | --- | --- | --- | --- | --- |
| TST-1 | Ten EQ response and stability checks against pinned formula | REQ-1 | DSP | snapshot harness | pass10bands+/-12at48k,31/16kedges44.1/96k,automatedextremes32/44.1/96/192k |
| TST-2 | Render all three editor sizes, verify band slider bindings | REQ-2 | UI | snapshot harness | pass3sizes,10fadersbothdirections,AutoGaintogglebinding |
| TST-3 | Auto Gain matching, silence, changes, bypass, ceiling and stereo | REQ-3 | DSP | snapshot harness | pass10settings/channelcaseswithin0.002dBsettled;silence/recovery/ceiling/manualoff/linking/bypass |
| TST-4 | Existing master/reorder/resize/state regression checks | REQ-4 | integration | harness + auval | previous66checksretained,195/195total,AUvalidationpasses40params |

Independent source review and real numeric checks required; listening and native DAW gestures remain user evaluation.

## 5. Deploy

DEP-1: unchanged version authority user. Build/install local AU, record SHA256, codesign and auval verification. Commit and push only private Gitea after checking origin; keep baseline rollback through Git. Screenshot export to Codex visualization artifacts before scratch cleanup.

## 6. Maintain

MNT-1: regression harness and README maintained by root; user feedback in this chat. Record source/revision/license and exact auto-gain semantics. Persist verified discoveries in shared memory; cleanup task scratch after install+verification. No service monitoring for local AU.

## Traceability

| REQ | Design | Test | Deploy | Maintain |
| --- | --- | --- | --- | --- |
| REQ-1 | DES-1 | TST-1 | DEP-1 | MNT-1 |
| REQ-2 | DES-2 | TST-2 | DEP-1 | MNT-1 |
| REQ-3 | DES-3 | TST-3 | DEP-1 | MNT-1 |
| REQ-4 | DES-4 | TST-4 | DEP-1 | MNT-1 |

Upstream pin: `f6d175d8253375a51832fd52aa86482c191caadc`, read full JSFX and MIT license. Port deliberately omits random noise, idle blanking and fixed character bumps; retains exact EQ band topology, Q curve and 2x resampling. AU ships the MIT notice as a bundle resource.

### Implementation evidence

- Exact pinned source and MIT license read in full (467+21 lines). GitHub project had156stars at live check.
- Implemented stack-only ten-band SVF coefficients,20ms control smoothing with coefficient refresh every8frames; same upstream4th-order Butterworth2xresampler.
- Enhance Auto Gain uses±36dB bounds,200ms linked stereo power estimates and100ms correction smoothing with -120dB RMS gate; reference dry signal uses existing lookahead ring position. Final linked peak safety respects ceiling.
- Existing29parameter IDs and order preserved;11newcontrols append;40hostparameters total. Six retired four-band EQ parameters remain inactive but registered. HPF and console dynamics stay active.
- New instances enable Auto Gain. Legacy states missing it restore off; missing new EQ bands resetflat, including when loading into reusedinstances.
- CMake resource includes complete upstreamMIT notice in installedAU, verified present.

### Final verification and deployment evidence

- `cmake --build <task>/build --parallel 8` exit0; installed to `~/Library/Audio/Plug-Ins/Components/Rack Strip.component`.
- `python3 tools/build_snapshot.py --build-dir <task>/build`; resulting harness independently rerun: **195/195 passed**. Final targeted matrix supersedes an earlier243-case draft; the48removed checks were redundant full-band runs atadditionalrates, replacedwithlow/highedgechecks.
- Ten measured EQ bands at48k plus31Hz/16kHz at44.1k/96k match independently derived pinned-upstream transfer function within0.025dB. The 28 boost/cut response comparisons have maximum error 0.0002 dB.
- Ten EnhanceAutoGain settings/channelcases match linked input/output RMS within0.002dB aftersettling. Separate stereo-link test has0steadyerror; initial0.000127transitiondifference was queuednonlinearaudio from priorEnhancesetting, so fixture nowchecks settledwindow. NoDSPchange was needed for thatfalsefailure.
- `auval -v aufx Rkst Ycmp` exit0: **AU VALIDATION SUCCEEDED**,40parameters. `codesign --verify --deep --strict` exit0.
- Installed binary SHA256: `f26752499705b35b18cdf61a56e9d6d1b444d2bdd6a2866bc7c413ccd268b8ce`.
- All29existinghostparameter IDs/order independently confirmedunchanged. Originalsixshelf/peakparametersareinactivebydesign; newEQstartsflat.
- FullMITnotice verified in installedAU `Contents/Resources/THIRD_PARTY_NOTICES.md`.
- GUI min/default/max screenshots reviewed: allcontrolscontained,tenfaders/cyan/red500present,AutoGainclear,nooverlap.
- Not verified: listening or nativeDAWmousegestures. Handler-leveldragging and nativeAUvalidationpassed.
- Docs updated with actualDSPURLs,behavior,migration,limitations; usercommit/push/reportingdirective saved asmemory24829. Verified session learning persisted as shared memory entry 24830.
- Rollback remains baseline90c4ff3 inprivateGit. Taskbuild/temporarybackup removedafterverifiedinstall; screenshots/logs retainedasCodexreviewartifacts.

### Task evidence

TASK: source-basedEQ560GUI/DSP andEnhance-onlyAutoGain. MODULES: audio,GUI,tests,integration.
FILES: Source/Eq560.h,Modules.h,PluginProcessor.cpp/.h,PluginEditor.cpp;tools/dsp_checks.h,snapshot_main.cpp;CMakeLists.txt;README.md;THIRD_PARTY_NOTICES.md;thisspec.
VERIFIED:195/195harness,40parameterAUvalidation,signature,noticepackaging,sourcecompatibilityandrendering. NOTVERIFIED:listening/nativeDAWinteraction.
REFERENCE: [pinnedEQ560DSP](https://github.com/chkhld/jsfx/blob/f6d175d8253375a51832fd52aa86482c191caadc/Equalizer/eq_560.jsfx); intentionalportdifferences listedinTHIRD_PARTY_NOTICES.md.
COMMIT: logicalfeaturecommit inprivateGiteahistory; no version/tagchange. HANDOFF:userassesses listening/nativefeel.
