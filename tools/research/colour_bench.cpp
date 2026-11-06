// Colour Engine CPU bench — measures the per-host-sample cost of the four rack modules'
// colour engines in each colour mode. Build with tools/research/build_colour_bench.py so it
// links the plugin's own release objects (same code the AU ships).
//
// The call shape mirrors Module::colourFrame(): one frame() call per host sample with
// 2-channel local buffers, four engines (one per module), 256-sample blocks.
//
// History: before 1.1.1-beta.10 the WaveNet ran in every mode even though frame() scales it
// by a weight that is exactly zero in ALGO and CLEAN. ALGO went from 6338 to 150 ns.
#include "ColourEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    constexpr int sr = 48000, block = 256, blocks = 2000; // ~10.6 s of audio
    const char* names[3] { "ALGO", "NAM", "CLEAN" };

    for (int mode = 0; mode < 3; ++mode)
    {
        {
            std::vector<std::unique_ptr<rack::ColourEngine>> engines;
            for (int m = 0; m < 4; ++m)
            {
                auto p = std::make_unique<rack::ColourEngine>();
                p->prepare (sr, m, mode);
                engines.push_back (std::move (p));
            }

            std::vector<float> clean ((size_t) block + 2);
            for (int i = 0; i < block; ++i) clean[(size_t) i] = 0.1f * std::sin (0.01f * (float) i);

            auto runBlock = [&] (int mode_) {
                for (auto& c : engines)
                    for (int i = 0; i < block; ++i)
                    {
                        float raw[2] {}, out[2] {};
                        raw[0] = raw[1] = clean[(size_t) i];
                        c->frame (raw, raw, out, 2, mode_, 1.0f, 1.0f);
                    }
            };

            for (int b = 0; b < 200; ++b) runBlock (mode);

            double best = 1e30;
            for (int r = 0; r < 3; ++r)
            {
                for (auto& e : engines) e->reset();
                const auto t0 = std::chrono::steady_clock::now();
                for (int b = 0; b < blocks; ++b) runBlock (mode);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min (best, std::chrono::duration<double> (t1 - t0).count());
            }

            const double nsPerSample = best * 1e9 / (double) (blocks * block);
            std::printf ("%s (4 modules, stereo): %8.2f ns/host-sample = %7.1fx realtime @48k  (%5.1f%% of one core at 1x)\n",
                         names[mode], nsPerSample, 1e9 / (nsPerSample * sr), nsPerSample * sr / 1e7);
        }
    }
    return 0;
}
