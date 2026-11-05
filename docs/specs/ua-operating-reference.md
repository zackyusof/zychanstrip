# UA operating reference, beta.7

Apply UA's documented nominal-level convention without reducing the rack to a
hardware-only control set. Source: [UA Realtime Rack v8.6, page 136](https://media.uaudio.com/support/downloads/Realtime_Rack_Manual_v86.pdf).
Most titles use −18 dBFS; the LA-2A Collection and Studer A800 use −12 dBFS.

## Implementation contract

- Console/FET nominal reference: −18 dBFS. Opto/tape: −12 dBFS.
- Shared software colour coordinate: −18 dBFS; opto/tape receive −6 dB input
  scaling. ALGO receives the inverse return trim; NAM receives fixed AC output
  normalization measured at its module reference, replacing the −40 dBFS-peak
  calibration and unrelated −15/−9 dB input trims. Calibration sine RMS is
  relative to full-scale amplitude 1 (peak is √2 times the RMS reference).
- These trims enclose colour only. Detector thresholds, manual gain, digital
  lookahead/ceiling, mix, Auto Gain and UNITY retain their control semantics.
  No running compensation is added between modules: earlier gain/EQ/compression
  still changes the drive reaching later modules in every order.
- Preserve NAM tape's bounded residual, slider smoothing, latency compensation,
  stereo linking, ALGO/NAM/CLEAN, ALL ratio, compress/limit, bypass, all 24
  orders, master, mix, Auto Gain and UNITY. No host parameter change.
- RESET opts for explicit software unity manual gains: Opto Gain 0, FET input
  0 dB/output 0 dB. Existing declared host defaults and saved values remain.
  This replaces the unsupported Opto Gain 50 (+15 dB) unity claim. UA's 1176
  knob mark 24 is a position, not a dB value; do not translate it to +24 dB.
  Opto's caption becomes OUTPUT GAIN, consistent with post-compression makeup.

## Limits

This adopts a UA operating convention, not UAD algorithms or measured hardware
transfer functions. Our capture metadata supplies no physical converter input
reference. In particular API 512c is not API 560, IGS One LA is not a UA LA-2A,
1176 Rev A is not LN, and A807 is not A800. The common circuit coordinate is an
explicit software convention; it does not recover those missing physical
calibrations. Compression curves and ALL remain approximations; UA's 1176
attack/release endpoints were already implemented (20–800 µs/50–1100 ms).

## Verification

Measure rendered nominal gain and rising level sweeps at 44.1/48/96 kHz for
all four NAM stages. Full captures should be within 0.12 dB at nominal;
tape residual within 1 dB, retaining intentional nonlinear effect. Run all
24 orders across three modes, mix 0/50/100 and UNITY off/on with Auto Gain,
EQ, compression, gain and Enhance engaged and four input levels. This extra
integration check verifies finite stereo-linked output and constant latency;
the existing gain-match, phase, tone-progress and ceiling checks provide the
stronger feature-specific assertions. Run the complete regression harness and
actual packaged AU factory/render probe before installing globally.
