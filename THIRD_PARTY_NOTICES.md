# Third-party notices

## EQ 560 by chokehold

Source: https://github.com/chkhld/jsfx/blob/f6d175d8253375a51832fd52aa86482c191caadc/Equalizer/eq_560.jsfx
Pinned commit: `f6d175d8253375a51832fd52aa86482c191caadc` (upstream EQ 560 v1.8.3).

`Source/Eq560.h` adapts the ten fixed-frequency, gain-dependent-width state-variable peak filters,
the exact `dBToWidth` curve, and fourth-order Butterworth zero-stuffing 2x up/downsampling from this source.
The source credits Andrew Simper's state-variable filter paper and Exstrom Laboratories for the Butterworth design.

Intentional differences: ZychanStrip omits the original random noise floor, idle auto-blanking and fixed
120 Hz/20 Hz/10 kHz character filters, retaining the existing console HPF instead. EQ band automation
is smoothed with a 20 ms one-pole in dB, with coefficients refreshed every eight native sample frames.
There is no separate upstream trim because ZychanStrip already has console output and rack master controls.
This is a deterministic EQ-core port, not a manufacturer-certified hardware model. Resampling uses IIR
filters with frequency-dependent phase, not a fixed sample delay; the console reports no additional fixed latency.

MIT License

Copyright (c) 2019-2025 chokehold

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
