# NAM colour processing — beta.2

## User controls and routing

The header COLOUR selector is global: **ALGO**, **NAM**, **CLEAN**. Its append-only
`colour_mode` automation parameter is saved with the session. Old sessions and RESET use
ALGO. Each mode retains functional EQ, gain controls, gain reduction, limiter and metering.
CLEAN means no extra colour, not full plugin bypass. Enhance's extra colour drive is removed
in CLEAN; its input gain and ceiling remain functional. Module bypass preserves timing.

- Console: 560 EQ -> selected colour -> existing legacy dynamics -> output gain.
- FET: input gain -> detector/gain reduction -> output gain -> selected colour.
- Opto: detector/gain reduction -> makeup gain -> selected colour.
- Enhance: input gain/colour drive -> selected colour/tape blend -> look-ahead limiter ->
  auto-gain correction and linked ceiling safety. The dry energy fallback is colour-delay-aligned.
- Rack: any of 24 module orders -> master trim -> latency-aligned partial dry/wet -> UNITY -> meters.

Console's location preserves the ALGO topology; a preamp capture at this point is an intentional
approximation. A whole-unit NAM cannot separately identify input/output transformers or recreate
a hardware feedback detector. No capture is applied twice. Prefer FET before Opto as a starting
point for fast peak catching followed by levelling, but this is not a universal best order.
Boosting any upstream gain changes subsequent colour and compression; fixed calibration does
not normalize those musical interactions away. The limiter ceiling protects its own output;
placing processing after it can change peaks, as before.

## Embedded models

All four profiles use their author-provided A2-Lite WaveNet. For stereo, both channels use the
same weights and independent state. No loader, network, sign-in, file access or JSON parsing
occurs during plugin processing. At preparation each model is calibrated at its 1 kHz nominal
reference (Console/FET −18 dBFS, Opto/tape −12 dBFS), then prewarmed; its steady DC is
removed. This is fixed gain compensation, not a running loudness follower. Opto and tape
use −6 dB input scaling into the shared −18 dBFS software colour coordinate; physical
capture input calibration remains unknown. The effect at other levels/frequencies follows the capture.

| Module | Capture | Creator | Licence |
|---|---|---|---|
| Console | API 512c, right mic gain 3 | tone3000 | T3K |
| Opto | IGS ONE LA, GR=0 dB | burtbombast | T3K |
| FET | UREI 1176 L, Rev A | yulokchan | CC BY |
| Enhance | Studer A807 L, pushed | yulokchan | CC BY |

Attribution, source links, SHA256 and local download setup are in THIRD_PARTY_NOTICES.md and
`tools/nam/profiles.json`. **Do not redistribute the T3K weights or this beta binary without
creator permission.** Weights are ignored build artifacts. The source/manifest contain no
weights. Profile ranking is a shortlist from provenance/popularity, not a claim of best sound.

## Resampling and latency

NAM runs at 48 kHz; the rest of the DSP stays at host rate. Prepared normalized Blackman-windowed
sinc tables convert each neural stage to/from 48 kHz, with a cutoff adapted to the lower Nyquist.
At native 48 kHz SRC is skipped. A 16-frame block adaptor allows efficient neural processing.
ALGO/CLEAN have matched integer delays and mode changes crossfade for 20 ms. The neural engines
remain warm in all modes. CLEAN bypasses SRC filtering and colour and is an exact delayed signal;
NAM's own frequency-dependent phase is retained. No promise of phase-null against CLEAN.

| Host rate | Colour delays API/opto/FET/tape | Total rack samples incl. limiter | Total ms |
|---|---:|---:|---:|
| 44.1 kHz | 130/121/123/115 | 586 | 13.288 |
| 48 kHz | 32/22/75/16 | 247 | 5.146 |
| 88.2 kHz | 241/223/227/211 | 1065 | 12.075 |
| 96 kHz | 260/240/245/228 | 1147 | 11.948 |
| 176.4 kHz | 479/443/452/420 | 2089 | 11.842 |
| 192 kHz | 520/480/490/456 | 2264 | 11.792 |

These replace the earlier short-Lanczos prototype estimates. FIR lengths are longer for improved
rejection. Delay is reported once in prepareToPlay and remains constant across mode, order and
bypass. The existing 0% wet path still returns untouched input immediately; partial blends use
the compensated dry signal. Existing latency-changing wet/dry behaviour is not redesigned here.

## Validation and limits

`tools/colour_checks.h` uses the production models and converter. It checks exact CLEAN impulse
timing at six rates, NAM stereo output and mode transitions, 30 kHz rejection on 96 kHz input,
full-rack finite output, saved/old-state migration, GUI control and all 24 bypass permutations
in three modes. `tools/build_snapshot.py` also runs the complete pre-existing DSP/GUI harness.
Final results and CPU timings are recorded after the final run below.

This is a local evaluation beta, not a stable release. No human listening comparison, original
hardware null test, broad alias sweep/THD measurement, live DAW automation soak or callback
allocation instrumentation has been completed. NAM histories continue running through RESET;
host delay storage clears without allocation, RESET selects ALGO, and reselecting NAM crossfades.
Native DAW AU validation is recorded separately after install. Fixed headroom calibration and
capture choice require listening feedback, especially pushed tape on a full mix.

## Verification record — 2026-11-04

Production snapshot harness: **RESULT 297/297 passed**. Includes exact CLEAN timing at six
rates, all 24 active/bypassed orders in three modes, old-state migration, parameter recall,
selector visibility on both pages, 96k-to-48k rejection and maximum Enhance ceiling safety.
The 30 kHz / 0.1-peak input test at 96 kHz produced output RMS 0.00000015 through Console NAM.
Both legacy research helper executables compile against the current pinned NAM engine.
GUI snapshots at 864×608, 1080×760 and 1620×1140 were rendered; header selector inspected
on rack/meter views. No DAW listening claim is made.

Measured full stereo NAM rack at 128-frame host blocks, 30% Enhance, Auto Gain off. Wall time
includes test-tone generation; short 0.25-second runs on this machine, not a universal CPU promise.
Approximate one-core utilization is runtime milliseconds per audio second divided by ten.

| Host rate | Rack latency samples | Latency ms | Runtime ms / audio second |
|---|---:|---:|---:|
| 44100 | 526 | 11.927 | 177.5 |
| 48000 | 135 | 2.812 | 151.0 |
| 88200 | 920 | 10.431 | 312.4 |
| 96000 | 991 | 10.323 | 306.6 |
| 176400 | 1772 | 10.045 | 593.6 |
| 192000 | 1919 | 9.995 | 599.6 |

At 192 kHz, mirrored rings and Accelerate SIMD dot products reduced the earlier scalar result
from 756.4 to approximately 599.6 ms per audio second. This remains CPU-heavy at high rates;
ALGO/CLEAN keep NAM warm and therefore do not provide a CPU-saving mode in beta.2.

Herdr FreeLLMAPI review was attempted in an OpenCode sibling pane. It returned "Invalid API key";
local provisioning confirmed missing unified key and missing DB bootstrap credential. No model
review result is counted as evidence. Validation above is deterministic local code execution.

### AU package validation follow-up

The first installed beta opened its AU factory but Initialize returned -1. Investigation found
that static-archive linking discarded NAM's registration-only WaveNet object, although the
headless harness linked it directly. The embedded engine now calls the fixed WaveNet config
factory explicitly, making its linkage mandatory. Direct installed-bundle factory initialization
is required in addition to the headless harness. System auval discovery also failed after a
registrar restart; do not count that as a validation pass.

Direct built-bundle regression now passes at 44.1, 48, 96 and 192 kHz: factory open=0,
initialize=0, COLOUR parameter found, and finite nonzero audio rendered in ALGO/NAM/CLEAN.
Reported latencies match the table. Repeat with `tools/check_au_bundle.sh [AU executable path]`;
without an argument it tests the global installed bundle. This catches the factory-link defect
that direct-object headless linking did not. System auval discovery remains a separate open
machine/registrar validation issue; direct registration in the test is not a claim that discovery works.

## Beta.3 Enhance transitions

Enhance's input gain, extra drive and NAM blend amount now ramp per sample over 20 ms, as
do ceiling and release coefficient. CLEAN ignores the colour amount/drive; ALGO and NAM
receive the smoothed values. The directly delayed ungained Auto Gain reference avoids mixing
delayed samples with a newly changing gain. Colour-engine/SRC delay and mode crossfades are
unchanged. See [Limiter / Enhance](module-limiter.md#beta3-enhance-slider-transitions).

## Beta.4 NAM Enhance progression

In NAM, Enhance now varies only the tape capture amount from 0 to 30%; its extra +12 dB
slider-dependent neural drive is removed. The model still receives the module's real input
gain, so upstream gain, EQ/compression and module order continue to affect its response.
At least 70% of aligned dry remains to avoid the deep cancellations/rebounds observed with
the old full crossfade. ALGO and CLEAN behavior, 20 ms ramps, fixed latency, bypass and
ceiling safety are unchanged. Existing NAM session values now use the revised, gentler law.
See [the specification](../specs/embedded-nam-colour.md#beta4-correction--nam-enhance-amount)
for scope and measurement coverage. Beta.4 still requires human listening confirmation.

## Beta.5 core repair — current behavior

This supersedes the beta.3/4 routing and blend descriptions above. Auto Gain now measures
uncorrected output after **every** rack module and applies stereo-linked matching there,
before master/mix. It no longer feeds corrected output back into its own energy ratio.
The limiter's colour/drive/gain reduction still runs in its selected slot; downstream
modules receive that uncorrected output. With Auto Gain on, final rack safety obeys the
ceiling and reports headroom limitation. With it off, downstream processing can change peaks.
The Threshold fader now moves down for increased input drive and displays ceiling minus gain,
while the existing positive `lim_gain` automation values stay compatible.

NAM Enhance adds 0–30% of a matched-capture nonlinear residual, rather than blending the
full tape EQ/phase response. Identical models run at x and 0.5x; subtracting twice the latter
cancels their common linear response. True input gain still affects level-dependent colour.
An extra stereo model increases CPU; SRC, bypass timing and fixed reported delay are unchanged.
See [Enhance core repair](../specs/enhance-core-repair.md) for formula, ceiling semantics,
compatibility changes and processor-level regression coverage. Beta.5 listening sign-off is pending.

### Beta.5 adapter performance correction

The host adapter is now max(16, ceil(16 × hostRate/48000)) frames, bounded to 128 for
the supported 8–384 kHz range. This retains approximately 16 native model samples per
process call above 48 kHz, avoiding the tiny-batch overhead that exceeded a real-time
budget at 192 kHz with the extra reference model. Sinc design, double precision and
48 kHz model rate are unchanged. The table above reflects the new per-rate delays;
48/44.1 kHz delay is unchanged, while 96/192 kHz add 64/192 total rack samples. Latency
is still reported once at preparation and identical across every mode/order/bypass.

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

## Beta.7 operating reference

See [UA reference contract](../specs/ua-operating-reference.md): Console/FET use −18 dBFS, Opto/tape −12 dBFS. Fixed colour input/return calibration preserves upstream drive and all 24 rack orders. Digital thresholds/ceiling, Auto Gain, UNITY, parallel mix and constant latency retain their semantics. Opto/FET RESET manual gains are now explicitly 0 dB. Earlier calibration/reset statements on this page are historical. Physical capture calibration remains unverified.
