# Rack Strip: AU / VST3 channel-strip rack (JUCE 8)

Four original, hardware-inspired modules in one plugin. Drag the six-dot grip at the top of any module to re-order the signal chain. The order is saved with the session.

| Module | Style | Controls |
|---|---|---|
| Console Strip | HPF, 4-band EQ, bus-style compressor | HPF, LF/LMF/HMF/HF, threshold, ratio, attack, release, out |
| Opto Leveler | Optical leveller, program-dependent release | Gain, Peak Reduction, Compress/Limit, VU needle |
| FET Compressor | Fast FET-style compressor | Input, Output, Attack, Release, ratio buttons 4/8/12/20/30 |
| Master Limiter | Look-ahead brick-wall limiter | Gain, Ceiling, Release, ENHANCE fader, LED ladder |

## Build

Requires CMake 3.22+ and a C++17 compiler (Xcode on macOS, MSVC on Windows, GCC/Clang plus JUCE's Linux dev libraries on Linux). JUCE 8.0.4 is downloaded automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

- macOS: builds VST3, AU and Standalone, and copies them to your plugin folders (`~/Library/Audio/Plug-Ins/`). Run `auval -v aufx Rkst Ycmp` to validate the AU.
- Windows: VST3 and Standalone. Linux: VST3 and Standalone (AU is macOS only).
- Change `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` (exactly 4 characters, at least one uppercase) and `PLUGIN_CODE` in `CMakeLists.txt` before shipping.
- Distributing AU/VST3 on macOS needs code signing and notarization.

## Notes

- Total latency is constant (the limiter's look-ahead, about 1.5 ms) whatever the order or bypass state, so there are no phase shifts when you re-order or bypass.
- This is an original design inspired by classic console, optical, FET and mastering-limiter behaviour. It does not copy any manufacturer's code, circuit models, artwork or trademarks, and it does not use their product names.
- The DSP is a clean first pass. Next steps would be oversampling the saturation stages, sidechain filtering, and avoiding the small allocation when EQ coefficients change.
