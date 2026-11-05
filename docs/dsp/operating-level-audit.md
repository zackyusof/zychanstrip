# Operating-level audit — installed 1.1.1-beta.6

Audit date: 2026-11-05. This is a calibration gap, not a claim of hardware accuracy.
No DSP or installed binary was changed during this audit.

## Existing research recovered

- Project memory record **24907**, dated 2026-11-03, contains the earlier official-UA-manual
  research for hardware-neutral controls. It says 1176 input raises compression and output
  sets level; the cited input/output mark around 24 is an example, not a universal dB gain.
  LA-2A Peak Reduction minimum requests no compression, and Gain's 0–100 markings are
  arbitrary: there is no universal physical unity setting. API graphic EQ flat is 0 dB bands.
- [Hardware-neutral reset specification](../specs/unity-real-defaults-knobs.md) requires
  researched starting positions, explicitly distinguishing them from declared host defaults
  and avoiding an invented universal hardware gain setting.
- [Embedded NAM specification](../specs/embedded-nam-colour.md) implements fixed small-signal
  gain calibration. That controls output amplitude at one test condition; it does not supply
  a physical dBFS-to-dBu or tape-flux reference for these captures.

## Implementation comparison

| Module | Current implementation | Operating-level status |
| --- | --- | --- |
| API/Console | Flat band RESET is 0 dB. NAM input scaling is 1.0. | Flat control position applied. Physical nominal input/headroom mapping is unverified. |
| 1176/FET | RESET input +12 dB, output 0 dB; fixed detector threshold −10 dBFS. NAM input scaling is 1.0 after compressor input/gain reduction/output. | Suggested starting position is implemented. The resulting input-referred threshold is approximately −22 dBFS at RESET, before other processing; this is not established as a hardware-calibrated operating point or capture input reference. |
| LA-2A/Opto | RESET Peak Reduction 0 and Gain 50; makeup law is Gain × 0.30 dB, so Gain 50 applies +15 dB. NAM input scaling is −15 dB after makeup, followed by separate gain normalization. | Minimum Peak Reduction agrees with the research. Gain 50 is not demonstrated unity. The −15 dB model-input scaling does not make the whole module unity because output calibration is separate. |
| Enhance/tape | RESET drive 0 dB, Enhance 0, ceiling −0.3 dBFS. NAM tape input scaling is −9 dB; Enhance adds a bounded nonlinear residual. | Digital limiter baseline is explicit. Tape nominal drive/flux and capture physical input reference are unverified. |

Sources: `Source/PluginProcessor.cpp` (`resetToDefaults`), `Source/Modules.h`
(`OptoModule::process`, `FetModule::process`, `LimiterModule::runLimiter`),
`Source/ColourEngine.cpp` (`prepare`, `render`), `tools/nam/profiles.json`.

All profiles are gain-matched using a **0.01-peak / −40 dBFS-peak, 1 kHz sine** during
preparation. The opto/tape model input attenuations are −15/−9 dB; the corresponding output
normalization makes small-signal gain near unity at that test point. These scalings were
conservative choices, not a documented physical operating-level mapping recovered from the
hardware research. Capture transport/polarity calibration is separate and does not establish
nominal level. Whole-rack Auto Gain and UNITY can hide output level errors while the nonlinear
stages are still being driven at an inappropriate level.

## What remains unverified

1. The intended digital reference and physical analog reference for every modeled circuit,
   including the capture creator's recording-chain calibration. No universal −18 dBFS or
   other mapping should be asserted without supporting evidence.
2. Separate model input-drive calibration, output trim and control-position mapping. A
   small-signal output normalization cannot establish the correct nonlinear operating point.
3. Level/transfer, compression-onset and harmonic sweeps below/at/above each researched
   reference, with Auto Gain and UNITY disabled, followed by reordered-chain level checks.

The existing 559-check suite verifies implemented arithmetic, gain matching, timing,
parameter/state compatibility and sampled signal behavior. It does **not** validate
hardware operating levels. Accurate per-module operating-level calibration remains open.
