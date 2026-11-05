# Enhance core repair — 1.1.1-beta.5

This supersedes beta.3/4's isolated smoothing/blend mitigations. Human feedback found
muffled NAM Enhance and ineffective Auto Gain when moving the control labelled Threshold.
A full-processor probe (other modules bypassed, 0.03-peak 1 kHz input) measured +3/+6/+9 dB
remaining with Auto Gain enabled at +6/+12/+18 dB input gain, in every colour mode. The
standalone module tests passed because they measured uncompensated output; the processor
fed corrected rack output back into the same ratio, mathematically leaving half the dB error.

## Signal contract

- The existing `lim_gain` stays positive 0–18 dB with its original ID/range/default. The GUI
  presents input-referred threshold = ceiling − input gain, with lower physical position
  increasing drive. Popup values are actual negative thresholds. Host automation/session
  values keep their existing meaning; the physical fader direction is corrected.
- All four modules run exactly once in the packed rack order. Enhance still performs colour,
  input gain and limiting in its chosen slot. Auto Gain is a utility after the entire rack,
  before master level and dry/wet, comparing original input against *uncorrected* rack output.
  A 200 ms energy estimate and 100 ms dB ramp are stereo-linked, bounded to ±36 dB. There is
  no previous-block self-feedback. Standalone limiter tests retain a local matcher using
  the same correction routine.
- A lower ceiling is immediately enforced at the module output. When Auto Gain is enabled,
  a final linked ceiling safety also bounds the matched rack output; master/mix remain
  subsequent independent controls. With Auto Gain off, processing after the limiter may
  change peaks, as before. Raising the ceiling is ramped over 20 ms.
- `MATCH <dB>` reports the actual block energy gain applied by Auto Gain, including safety.
  `CEILING <dB>` indicates requested matching is headroom-limited. No matcher can restore
  input RMS exceeding the maximum permitted output amplitude while obeying the ceiling.
- NAM Enhance no longer blends the full pushed tape response into dry. Two identical captures
  receive x and 0.5x at the same native sample positions; the added residual is
  `0.30 * Enhance * postGain * ((F(x)-DC) - 2*(F(0.5x)-DC))`. A common linear transfer cancels
  exactly; level-dependent fundamental/harmonic differences remain. The unfiltered aligned
  dry path stays intact, including frequencies above the model's 24 kHz Nyquist at high host
  rates. This is a nonlinear residual, not a guarantee of harmonic-only output or perfectly
  flat response at every drive level. Real upstream/manual gain continues to drive the models.
- ALGO/CLEAN colour laws, parameter IDs/ranges/defaults, bypass timing, packed order checking,
  read-only final metering tap and fixed reported latency remain intact. The extra matched
  reference model has independent stereo state, is prepared off-thread and remains warm like
  the existing models. CPU rises; no additional SRC or reported delay is introduced.

## Validation

Full-processor threshold down/up matching at 48/96 kHz, 32/512-frame blocks and all three
colour modes; Auto Gain off/manual gain; variable/empty blocks; mono/stereo impossible
headroom; threshold GUI direction/readout; original ceiling, bypass, latency and state tests.
Frequency sweeps at three rates/levels require less than 1 dB deviation for the tested quiet
inputs and bounded progressive response at higher drive. Harmonic tests prove a nonzero,
bounded nonlinear effect. All 24 *active* NAM rack orders compare automated renders against
otherwise identical unautomated instances, and require actual RMS matching with Auto Gain on.
These establish numerical behavior, not listening sign-off. Human audition of beta.5 is pending.
