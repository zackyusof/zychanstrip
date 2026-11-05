# Parallel mix phase repair — 1.1.1-beta.6

Human feedback: 50% mix sounds phased. Previous testing bypassed every module and did
not establish phase coherence with the flat EQ or active NAM captures.

## Reproduced causes

1. EQ560's two fourth-order Butterworth oversampling filters have frequency-dependent
   phase even when every band is zero. At 48 kHz, a 16 kHz sine measures almost 180°
   relative to dry, causing approximately -29.6 dB of cancellation at 50% mix.
2. The pinned API capture inverts polarity and contains about 16 native samples of
   transport delay. The opto and FET captures contain about 6 and 8.5 samples. Existing
   gain/DC calibration did not compensate these offsets. Individual 50% capture/dry
   measurements showed approximately -21.8 dB cancellation at 4 kHz for the opto profile.
3. The dry delay ring advanced only for partial mix, freezing at 0% and 100%. Returning
   from either endpoint read stale history. At 0%, actual delay also disagreed with the
   constant latency reported to the host. RESET did not clear that dry ring.

## Repair

- Keep EQ560's 2x SVF/bandwidth law, replacing the IIR oversampling pair with symmetric
  63-tap Blackman halfband FIRs. Combined group delay is exactly 31 native samples.
  Console bypass carries that same 31-sample delay; both filter/delay histories stay warm.
  The resampler's magnitude/alias rejection changes, intentionally; existing presets keep
  band values, but the oversampling response is no longer the original IIR response.
- Record transport/polarity calibration alongside each pinned profile's SHA256 in
  `tools/nam/profiles.json`; regenerate `EmbeddedNam.h` with `prepare_profiles.py`.
  Normalize API polarity and advance the neural read position by the measured capture
  transport offset within a causal padding budget. CLEAN/ALGO/bypass receive equal padding.
  The fractional 8.5-sample FET offset uses the existing double-precision sinc interpolation,
  including a 50-sample causal support allowance at native 48 kHz. The tape residual stage
  retains its beta.5 law and zero added transport offset.
- Always advance the dry delay, including 0% and 100%. Both endpoints retain the fixed
  host-reported delay. The dry signal is otherwise untouched. Mix targets ramp 20 ms.
  RESET clears wet/dry histories together. Auto Gain and UNITY references use aligned input.
- No parameter IDs, ranges, declared defaults, master/mix order, packed rack permutation,
  or read-only metering contract changes. Latency remains fixed across mode/order/bypass/mix
  and is reported only during preparation; values increase to account for the real delays.

## Limits

This removes incorrect transport/polarity and flat-EQ oversampling phase from the parallel
sum. Intentional EQ band phase, captured analog frequency-dependent phase, nonlinear gain
reduction and harmonic changes remain. A processed signal is not guaranteed to null with
original input at arbitrary settings. The test rejects deep transport-driven combing while
allowing the remaining captured EQ/phase response. Listening confirmation is still pending.

## Validation

Active flat CLEAN EQ through every rack order; input noise and mix endpoint/transition
null tests at six rates and 31/512-frame blocks; native/interpolated NAM capture phase checks
at four rates; active complete NAM 50% blends at 0.01/0.1/0.5 peak and 100/1k/4k/8k/16k Hz;
updated independent Fourier reference for the FIR plus unchanged 2x EQ SVF; all existing
level-matching, ceiling, bypass, state, GUI and packaged-AU tests. 0% tests now require
untouched *delayed* input, correcting the previous incompatible timing assertion.
