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
