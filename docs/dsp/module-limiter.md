# Limiter / Enhance DSP

> **Beta.2 update:** global `colour_mode` selects ALGO, embedded NAM, or CLEAN.
> The routing and latency changes in [NAM colour DSP](nam-colour.md) supersede older
> colour-only/zero-latency statements and historical measurements below. NAM is stateful;
> ALGO retains the documented legacy shaping. CLEAN removes added colour, retaining
> functional EQ/dynamics/limiting and a matched delay. Module order remains user-controlled.


The `LimiterModule` in `Source/Modules.h` is a stereo-linked look-ahead limiter. It derives a per-sample request `req` in `[0,1]`, tracks the minimum request over its `la`-sample window, averages that minimum through `avgRing`, then applies the existing release, saturation, delay, ceiling, and optional auto-gain stages. Reported latency remains `la - 1`, with `la = max(2, ceil(0.0015 * sampleRate))`.

## Sliding minimum

The request ring is initialized to `1.0f`. A fixed-capacity monotonic deque now tracks its minimum in amortized O(1) per sample instead of scanning all `la` entries. The deque stores candidate value, sequence ID, and physical slot in preallocated arrays. Expired candidates leave from the front; candidates no smaller than the incoming request leave from the back. Sequence IDs use `uint64_t`; physical slots are stored explicitly.

This is an internal CPU optimization only: request values, limiter gain law, bypass path, latency, parameters, and rack ordering are unchanged. Exact scan-equivalence checks cover window sizes 1, 2, 3, 67, 72, 144, and 288, reset, and wrap. The tests live in `tools/dsp_checks.h`.

## Beta.3 Enhance slider transitions

Enhance input gain, extra colour drive, colour blend amount, ceiling amplitude and release
coefficient ramp per sample over 20 ms after a parameter target changes. RESET and preparation
snap these states to the current parameters. The four parameter IDs, ranges and defaults are
unchanged. CLEAN still ignores Enhance colour while retaining the smoothed input gain/limiter.
The detector ceiling ramps over 20 ms, while the final linked safety clamp honors a lower
requested ceiling immediately. If the signal already exceeds that new limit, an immediate
amplitude reduction is unavoidable; output safety takes priority over continuity at that instant. Release time remains the requested
10–1000 ms after its coefficient ramp settles.

The limiter's standalone Auto Gain reference now delays the ungained input itself through the
colour delay before the look-ahead ring. This avoids dividing old samples by a newly changing
gain. Processor-supplied chain references, rack order packing, read-only meter tap, and constant
reported latency are unchanged. Forward/reverse slider regression tests cover all three colour
modes at 44.1/48/96 kHz and 32/512-frame blocks. Live listening/DAW automation soak remains
maintainer validation.

## Beta.4 NAM Enhance progression

In NAM, Enhance now varies only the tape capture amount from 0 to 30%; its extra +12 dB
slider-dependent neural drive is removed. The model still receives the module's real input
gain, so upstream gain, EQ/compression and module order continue to affect its response.
At least 70% of aligned dry remains to avoid the deep cancellations/rebounds observed with
the old full crossfade. ALGO and CLEAN behavior, 20 ms ramps, fixed latency, bypass and
ceiling safety are unchanged. Existing NAM session values now use the revised, gentler law.
See [the specification](../specs/embedded-nam-colour.md#beta4-correction--nam-enhance-amount)
for scope and measurement coverage. Beta.4 still requires human listening confirmation.
