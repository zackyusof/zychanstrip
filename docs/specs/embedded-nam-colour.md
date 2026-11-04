# Embedded colour engines — 1.1.1-beta.2

User scope: one header COLOUR selector (ALGO / NAM / CLEAN), four embedded profiles,
no loader or network during plugin use, all normal host sample rates, new local beta,
updated documentation and source pushed to Gitea. No stable promotion.

## Behaviour

Append-only `colour_mode`: ALGO=0 (legacy/default), NAM=1, CLEAN=2. Saved in APVTS;
old states migrate to ALGO. RESET restores ALGO. The selector is visible on both pages.
Twenty-millisecond crossfades select between time-aligned colour branches. Module bypass
forces the clean branch; the existing limiter bypass retains its look-ahead delay.
CLEAN removes added colour, not EQ, compressor gain reduction/makeup, master gain or limiting.
Enhance's extra colour drive is absent in CLEAN. No new per-module colour controls.

ALGO retains each existing waveshaper and its position. Console colour remains after EQ,
before its legacy dynamics, to avoid changing ALGO's detector topology. This is deliberately
not a literal model of the API 560 circuit. Opto/FET colour follows gain reduction and makeup.
Enhance tape/blend is before the brick-wall limiter and final safety gain. The existing dry/wet
mix and UNITY remain after the rack. One whole-unit capture is used once per module; it is
not duplicated at input and output. 1176 capture is Rev A, not a claim of exact LN modelling.

## Profiles and rights

See `tools/nam/profiles.json` for download URLs, SHA256, creator and licence. The engine is
MIT NAM Core at 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842. Use author-supplied A2-Lite
variants, identical model/state instances per stereo side to avoid unintended channel mismatch.
Small-signal gain calibration is fixed at preparation, not adaptive inter-module levelling.
Steady DC is removed. Existing upstream gain therefore continues to drive the next module.

API 512c gain 3 and IGS ONE LA zero-GR captures are T3K: local software use/evaluation only
until creator redistribution permission is recorded. Model files and generated headers live
in ignored build/; no weights or compiled beta may be pushed. UREI 1176 and Studer captures
are CC BY; attribution in THIRD_PARTY_NOTICES.md is packaged in the AU. No model is asserted
"best" from popularity or training ESR. No human listening evaluation has been completed.

## Rate conversion and latency

Host DSP stays at host rate. Each neural branch uses two prepared, normalized Blackman-sinc
polyphase filters to/from 48 kHz. Filter cutoff is 0.94 times the lower Nyquist; half-length
is ceil(48 * max(1,source/destination)). 1024 fractional phases are linearly interpolated.
At 48 kHz conversion is skipped. A 16-frame neural batching delay exists at every rate.
No file parsing, coefficient generation, profile loading or buffer resizing occurs in frame().
NAM remains running in ALGO/CLEAN to keep its history warm for smooth switching; this beta
therefore does not promise CPU savings in those modes. Filter bandwidth/phase are only applied
to NAM: CLEAN is an exact integer-delayed signal and ALGO is a delayed legacy waveshaper.

Per-module latency = 16 + ceil(forward half-length + (return half-length+2)*hostRate/48000)
when resampling, otherwise 16. Rack latency is all four stages plus existing look-ahead;
reported once in prepareToPlay, invariant across mode/order/bypass. The earlier Lanczos
prototype estimates are superseded; use measured integration results, not those estimates.

Reset clears host/colour delay storage without allocating. Neural histories continue to be
clocked rather than reset/prewarmed on the audio thread; RESET also selects ALGO, so new NAM
selection fades into the continuously running state. Reorder can change sound and transients;
no claim of click-free live drag reordering or hardware-exact time-varying interaction.

## Verification

Production snapshot harness plus colour_checks.h: real models, exact CLEAN impulse delay,
six host rates, linked stereo, bounded mode transitions, session recall and constant latency.
Record final measured results and limitations in docs/dsp/nam-colour.md before installation.
Build/install policy remains commit -> build -> harness -> push -> global installer.

Verification: production harness RESULT 297/297 passed; measured CPU/latency in docs/dsp/nam-colour.md. Global installation/signature and AU validation are recorded in the task delivery.
