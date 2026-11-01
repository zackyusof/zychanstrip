# tools/research — headless DSP research harness

`permute.cpp` measures how the rack behaves under every one of the 24 module orders.
It **includes the real `Source/Modules.h`** through a minimal JUCE shim
(`shim/juce_audio_processors/juce_audio_processors.h`, `shim/juce_dsp/juce_dsp.h`) and
mirrors the chain loop from `Source/PluginProcessor.cpp:128-146`, so the numbers come
from the shipped algorithms, not from a re-implementation.

It needs only a C++17 compiler — no JUCE, no Xcode, no plugin build. That is why it runs
on Linux CI/hosts where the AU itself cannot be built.

```bash
tools/research/build.sh                    # -> build/permute
./build/permute > /tmp/permute.md          # markdown tables, ~3 s of audio per run
```

What it measures:

| Section | Question |
| --- | --- |
| 1 | How far apart are the 24 orders (RMS / peak spread) for a neutral and a hot preset |
| 2 | Full per-permutation table: level, 1 kHz bin, each module's gain reduction, auto-gain |
| 3 | Dry/wet comb filtering with and without the 71-sample dry-path compensation |
| 4 | Limiter auto-gain value and level for limiter-last vs limiter-first |
| 5 | Whether any order lets the output exceed the limiter ceiling |
| 6 | How each detector's gain reduction changes with position |
| 7 | Colour-stage stacking: band levels for four orders with the limiter last |
| 8 | Invariants (latency) |

Results are transcribed into `docs/dsp/measurements.md`. Re-run and update that file
whenever module DSP changes — a stale table is worse than no table.

## Shim limits

The shim implements only what `Source/Modules.h` touches: `juce::String`,
`juce::AudioBuffer<float>`, `juce::AudioProcessorValueTreeState::getRawParameterValue`,
`juce::jlimit`, `juce::jmap`. Anything added to the modules that needs more JUCE surface
will fail to compile here — extend the shim rather than forking the DSP.
