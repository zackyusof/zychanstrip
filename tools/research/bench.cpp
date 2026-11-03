// Headless DSP benchmark for zychanstrip.
//
// Includes the REAL module DSP (Source/Modules.h + Source/Eq560.h) through the
// tools/research/shim JUCE stand-in, so every number below is the shipped code,
// not a re-implementation. Runs the same four-module chain, in the same order
// packing, as ZychanStripProcessor::processBlock.
//
// Usage:
//   ./bench                       human table
//   ./bench --csv                 one row per module: name,ns_per_sample
//   ./bench --null <file.f32>     append raw f32 interleaved output (null testing)
//   ./bench --preset unity|mastering|heavy
//
// The importer/exporter for null tests lives in tools/research/nullcmp.py.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../Source/Modules.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlock      = 256;
constexpr int    kSeconds    = 4;
constexpr int    kWarmupSecs = 1;
constexpr int    kChannels   = 2;
constexpr int    kRepeats    = 5;

using Clock = std::chrono::steady_clock;

enum Preset { Unity, Mastering, Heavy };

struct Chain
{
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<rack::ConsoleModule> console;
    std::unique_ptr<rack::OptoModule>    opto;
    std::unique_ptr<rack::FetModule>     fet;
    std::unique_ptr<rack::LimiterModule> limiter;
    rack::Module* byId[4] {};

    void build (Preset p)
    {
        using namespace rack;
        auto v = [this] (const std::string& id, float value) { apvts.set (id, value); };

        // Defaults mirroring the shipped parameter layout (PluginProcessor.cpp createLayout).
        v ("con_bypass", 0); v ("con_hpf", 20); v ("con_lf", 0); v ("con_lmf_f", 600); v ("con_lmf", 0);
        v ("con_hmf_f", 3000); v ("con_hmf", 0); v ("con_hf", 0); v ("con_thresh", 0);
        v ("con_ratio", 3); v ("con_attack", 10); v ("con_release", 200); v ("con_out", 0);
        v ("opto_bypass", 0); v ("opto_peak", 0); v ("opto_gain", 50); v ("opto_mode", 0);
        v ("fet_bypass", 0); v ("fet_input", 0); v ("fet_output", 0); v ("fet_attack", 3);
        v ("fet_release", 4); v ("fet_ratio", 0);
        v ("lim_bypass", 0); v ("lim_gain", 0); v ("lim_ceiling", -0.3f); v ("lim_release", 100);
        v ("lim_enhance", 0); v ("lim_auto_gain", 1);
        for (auto* id : Eq560::bandIds) v (std::string ("con_") + id, 0);

        switch (p)
        {
            case Unity: break;
            case Mastering:
                v ("con_eq_1k", 3); v ("con_eq_63", 2); v ("con_eq_16k", -2);
                v ("opto_peak", 35); v ("opto_gain", 25);
                v ("fet_input", 8); v ("fet_output", -2);
                v ("lim_gain", 6); v ("lim_ceiling", -1.0f); v ("lim_enhance", 30);
                break;
            case Heavy:
                v ("con_eq_1k", 6); v ("con_eq_63", 4); v ("con_eq_16k", -4); v ("con_eq_250", 3);
                v ("con_eq_4k", -3); v ("con_eq_125", 2); v ("con_eq_8k", 2);
                v ("opto_peak", 55); v ("opto_gain", 40); v ("opto_mode", 1);
                v ("fet_input", 18); v ("fet_output", -6); v ("fet_ratio", 3);
                v ("lim_gain", 12); v ("lim_ceiling", -1.0f); v ("lim_enhance", 60);
                break;
        }

        console = std::make_unique<ConsoleModule> (apvts, "con_");
        opto    = std::make_unique<OptoModule>    (apvts, "opto_");
        fet     = std::make_unique<FetModule>     (apvts, "fet_");
        limiter = std::make_unique<LimiterModule> (apvts, "lim_");
        byId[0] = console.get();
        byId[1] = opto.get();
        byId[2] = fet.get();
        byId[3] = limiter.get();

        for (auto* m : byId) m->prepare (kSampleRate, kBlock);
    }

    // Default rack order from PluginProcessor::getDefaultOrder().
    static constexpr std::array<int, 4> order { 0, 2, 1, 3 };
};
} // namespace

int main (int argc, char** argv)
{
    Preset preset = Mastering;
    bool csv = false;
    std::string nullPath;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--csv") csv = true;
        else if (a == "--null" && i + 1 < argc) nullPath = argv[++i];
        else if (a == "--preset" && i + 1 < argc)
        {
            const std::string p = argv[++i];
            preset = p == "unity" ? Unity : (p == "heavy" ? Heavy : Mastering);
        }
    }

    Chain chain;
    chain.build (preset);

    const int totalSamples = (int) (kSampleRate * kSeconds);
    const int blocks = totalSamples / kBlock;

    // Deterministic programme: multitone + transient bursts + noise, so every
    // module sees detector activity and no path can be optimised away.
    std::vector<float> left ((size_t) totalSamples), right ((size_t) totalSamples);
    uint32_t rng = 0x12345678u;
    auto nextNoise = [&rng]() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return ((float) (rng & 0xffffff) / (float) 0x800000) - 1.0f;
    };
    for (int i = 0; i < totalSamples; ++i)
    {
        const double t = (double) i / kSampleRate;
        const double burst = std::fmod (t, 0.25) < 0.05 ? 1.0 : 0.25;
        const double s = 0.30 * std::sin (2.0 * M_PI * 100.0 * t)
                       + 0.30 * std::sin (2.0 * M_PI * 1000.0 * t)
                       + 0.20 * std::sin (2.0 * M_PI * 8000.0 * t) * burst
                       + 0.01 * nextNoise();
        left[(size_t) i]  = (float) s;
        right[(size_t) i] = (float) (s * 0.9 + 0.02 * nextNoise());
    }

    juce::AudioBuffer<float> buf (kChannels, kBlock);
    std::vector<float> out;
    if (! nullPath.empty()) out.reserve ((size_t) totalSamples * kChannels);

    auto runBlocks = [&] (int from, int to, rack::Module* only, bool record)
    {
        for (int b = from; b < to; ++b)
        {
            for (int c = 0; c < kChannels; ++c)
            {
                float* d = buf.getWritePointer (c);
                const float* s = (c == 0 ? left.data() : right.data()) + (size_t) b * kBlock;
                std::copy_n (s, kBlock, d);
            }

            if (only != nullptr) only->run (buf);
            else for (int id : Chain::order) chain.byId[id]->run (buf);

            if (record)
                for (int i = 0; i < kBlock; ++i)
                    for (int c = 0; c < kChannels; ++c)
                        out.push_back (buf.getSample (c, i));
        }
    };

    const int warmBlocks = (int) (kSampleRate * kWarmupSecs) / kBlock;
    runBlocks (0, warmBlocks, nullptr, false);   // warm caches + settle detectors

    struct Result { std::string name; double nsPerSample; };
    std::vector<Result> results;

    auto measure = [&] (const std::string& name, rack::Module* only)
    {
        double best = 1e30;
        for (int r = 0; r < kRepeats; ++r)
        {
            chain.byId[0]->reset(); chain.byId[1]->reset();
            chain.byId[2]->reset(); chain.byId[3]->reset();
            const auto t0 = Clock::now();
            runBlocks (warmBlocks, blocks, only, false);
            const auto t1 = Clock::now();
            const double secs = std::chrono::duration<double> (t1 - t0).count();
            best = std::min (best, secs * 1e9 / (double) ((blocks - warmBlocks) * kBlock));
        }
        results.push_back ({ name, best });
    };

    measure ("Console (EQ560 + detector)", chain.console.get());
    measure ("Opto (leveler)",            chain.opto.get());
    measure ("FET (compressor)",          chain.fet.get());
    measure ("Limiter (look-ahead)",      chain.limiter.get());
    measure ("FULL CHAIN",                nullptr);

    runBlocks (warmBlocks, blocks, nullptr, ! nullPath.empty());

    if (! nullPath.empty())
    {
        if (FILE* f = std::fopen (nullPath.c_str(), "wb"))
        {
            std::fwrite (out.data(), sizeof (float), out.size(), f);
            std::fclose (f);
            std::fprintf (stderr, "wrote %zu floats to %s\n", out.size(), nullPath.c_str());
        }
        else std::fprintf (stderr, "could not open %s\n", nullPath.c_str());
    }

    if (csv)
    {
        for (auto& r : results)
            std::printf ("%s,%.2f\n", r.name.c_str(), r.nsPerSample);
    }
    else
    {
        const char* presetName = preset == Unity ? "unity" : (preset == Heavy ? "heavy" : "mastering");
        std::printf ("# zychanstrip DSP bench — preset %s, %.0f Hz, block %d, stereo, best of %d\n",
                     presetName, kSampleRate, kBlock, kRepeats);
        std::printf ("| module | ns / sample (stereo) | x realtime @48k |\n|---|---|---|\n");
        for (auto& r : results)
            std::printf ("| %s | %.1f | %.1fx |\n", r.name.c_str(), r.nsPerSample,
                         1e9 / (r.nsPerSample * kSampleRate));
    }
    return 0;
}
