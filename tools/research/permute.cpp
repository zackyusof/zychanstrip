//==============================================================================
// permute.cpp — headless reorder-interaction harness for zychanstrip.
//
// It #includes the REAL module DSP (Source/Modules.h) through a tiny JUCE shim
// (tools/research/shim) and reproduces the chain-level loop from
// Source/PluginProcessor.cpp:128-146, so measurements come from the shipped
// algorithms rather than a re-implementation.
//
// Build:  tools/research/build.sh
// Run:    ./build/permute > docs/dsp/measurements-raw.md
//
// Everything it prints is a measurement; nothing is simulated or estimated.
//==============================================================================
#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../../Source/Modules.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlock      = 256;
constexpr double kSeconds    = 3.0;
constexpr double kSettle     = 1.0;   // discarded before measuring
constexpr int    kNumModules = 4;

using Order = std::array<int, kNumModules>;

inline float db (double g) { return (float) (20.0 * std::log10 (std::max (g, 1.0e-12))); }
inline double dbToLin (double d) { return std::pow (10.0, d / 20.0); }

//==============================================================================
struct Signal
{
    std::string name;
    std::vector<float> left, right;
    double toneHz = 0.0;      // primary tone for Goertzel (0 = none)
};

struct Lcg
{
    uint32_t state = 0x12345678u;
    float next()                       // uniform in [-1, 1)
    {
        state = state * 1664525u + 1013904223u;
        return (float) ((int32_t) state) / 2147483648.0f;
    }
};

Signal makeTone (const std::string& name, double hz, double amp)
{
    Signal s;
    s.name = name;
    s.toneHz = hz;
    const int n = (int) (kSampleRate * kSeconds);
    s.left.resize ((size_t) n);
    s.right.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSampleRate;
        const float v = (float) (amp * std::sin (2.0 * M_PI * hz * t));
        s.left[(size_t) i] = s.right[(size_t) i] = v;
    }
    return s;
}

// Three simultaneous tones: low / mid / high. Used for spectral interaction.
Signal makeMultiTone()
{
    Signal s;
    s.name = "multitone(100/1k/8k)";
    s.toneHz = 1000.0;
    const int n = (int) (kSampleRate * kSeconds);
    s.left.resize ((size_t) n);
    s.right.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSampleRate;
        const double v = 0.30 * std::sin (2.0 * M_PI * 100.0 * t)
                       + 0.30 * std::sin (2.0 * M_PI * 1000.0 * t)
                       + 0.20 * std::sin (2.0 * M_PI * 8000.0 * t);
        s.left[(size_t) i] = s.right[(size_t) i] = (float) v;
    }
    return s;
}

// Kick-like transient train + noise hats: exercises attack/release and detector coupling.
Signal makeTransients()
{
    Signal s;
    s.name = "transients(kick+hats)";
    const int n = (int) (kSampleRate * kSeconds);
    s.left.resize ((size_t) n);
    s.right.resize ((size_t) n);
    Lcg rng;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSampleRate;
        const double phase = std::fmod (t, 0.25);
        double v = 0.0;
        if (phase < 0.12)                                  // kick body
        {
            const double env = std::exp (-phase * 22.0);
            v += 0.85 * env * std::sin (2.0 * M_PI * (55.0 + 60.0 * std::exp (-phase * 30.0)) * phase);
        }
        if (phase > 0.125 && phase < 0.135)                // hat burst
            v += 0.30 * rng.next() * std::exp (-(phase - 0.125) * 300.0);
        v += 0.01 * rng.next();                            // noise floor
        s.left[(size_t) i] = s.right[(size_t) i] = (float) juce::jlimit (-1.0, 1.0, v);
    }
    return s;
}

//==============================================================================
struct Preset
{
    std::string name;
    std::vector<std::pair<std::string, float>> values;
};

Preset presetUnity()
{
    Preset p;
    p.name = "unity (all neutral, no dynamics action)";
    p.values = {
        { "con_bypass", 0 }, { "opto_bypass", 0 }, { "fet_bypass", 0 }, { "lim_bypass", 0 },
        { "con_eq_16k", 0 }, { "con_eq_8k", 0 }, { "con_eq_4k", 0 }, { "con_eq_2k", 0 },
        { "con_eq_1k", 0 }, { "con_eq_500", 0 }, { "con_eq_250", 0 }, { "con_eq_125", 0 },
        { "con_eq_63", 0 }, { "con_eq_31", 0 },
        { "opto_peak", 0 }, { "opto_gain", 0 }, { "opto_mode", 0 },
        { "fet_input", 0 }, { "fet_output", 0 }, { "fet_attack", 3 }, { "fet_release", 4 },
        { "fet_ratio", 0 },
        { "lim_gain", 0 }, { "lim_ceiling", -0.3f }, { "lim_release", 100 }, { "lim_enhance", 0 },
        { "lim_auto_gain", 0 },
        { "master_level", 0 }, { "master_mix", 100 },
    };
    return p;
}

Preset presetMastering (bool autoGain)
{
    Preset p;
    p.name = autoGain ? "mastering (auto-gain ON)" : "mastering (auto-gain OFF)";
    p.values = {
        { "con_bypass", 0 }, { "opto_bypass", 0 }, { "fet_bypass", 0 }, { "lim_bypass", 0 },
        { "con_eq_16k", -2 }, { "con_eq_8k", 0 }, { "con_eq_4k", 0 }, { "con_eq_2k", 0 },
        { "con_eq_1k", 3 }, { "con_eq_500", 0 }, { "con_eq_250", 0 }, { "con_eq_125", 0 },
        { "con_eq_63", 2 }, { "con_eq_31", 0 },
        { "opto_peak", 35 }, { "opto_gain", 25 }, { "opto_mode", 0 },
        { "fet_input", 8 }, { "fet_output", -2 }, { "fet_attack", 2 }, { "fet_release", 4 },
        { "fet_ratio", 0 },
        { "lim_gain", 6 }, { "lim_ceiling", -1.0f }, { "lim_release", 100 }, { "lim_enhance", 30 },
        { "lim_auto_gain", autoGain ? 1.0f : 0.0f },
        { "master_level", 0 }, { "master_mix", 100 },
    };
    return p;
}

//==============================================================================
struct Chain
{
    juce::AudioProcessorValueTreeState apvts;
    std::array<std::unique_ptr<rack::Module>, kNumModules> modules;
    float masterLevel = 0.0f;
    float mixPercent = 100.0f;

    Chain() = default;

    void configure (const Preset& p)
    {
        for (auto& kv : p.values) apvts.set (kv.first, kv.second);

        modules[0] = std::make_unique<rack::ConsoleModule> (apvts, juce::String ("con_"));
        modules[1] = std::make_unique<rack::OptoModule>    (apvts, juce::String ("opto_"));
        modules[2] = std::make_unique<rack::FetModule>     (apvts, juce::String ("fet_"));
        modules[3] = std::make_unique<rack::LimiterModule> (apvts, juce::String ("lim_"));

        for (auto& m : modules) m->prepare (kSampleRate, kBlock);
        masterLevel = apvts.getRawParameterValue (juce::String ("master_level"))->load();
        mixPercent  = apvts.getRawParameterValue (juce::String ("master_mix"))->load();
    }

    int totalLatency() const
    {
        int sum = 0;
        for (auto& m : modules) sum += m->getLatencySamples();
        return sum;
    }

    // Mirrors ZychanStripProcessor::processBlock (Source/PluginProcessor.cpp:113-150).
    // dryDelaySamples: 0 reproduces the shipped dry/wet path; >0 applies the
    // latency compensation that zychanstrip-mix-phase proposes.
    void run (const Signal& in, const Order& order, std::vector<float>& outL, std::vector<float>& outR,
              std::array<float, kNumModules>* maxGr, float* finalCompDb, int dryDelaySamples = 0)
    {
        const int total = (int) in.left.size();
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::AudioBuffer<float> dry (2, total);
        for (int i = 0; i < total; ++i) { dry.setSample (0, i, in.left[(size_t) i]); dry.setSample (1, i, in.right[(size_t) i]); }

        outL.assign ((size_t) total, 0.0f);
        outR.assign ((size_t) total, 0.0f);

        std::array<float, kNumModules> gr {};
        float compDb = 0.0f;
        const float masterGain = (float) dbToLin (masterLevel);
        const float wet = juce::jlimit (0.0f, 1.0f, mixPercent * 0.01f);

        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = std::min (kBlock, total - pos);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    buf.setSample (ch, i, ch == 0 ? in.left[(size_t) (pos + i)] : in.right[(size_t) (pos + i)]);

            for (int id : order) modules[(size_t) id]->run (buf);

            for (int ch = 0; ch < 2; ++ch)
                buf.applyGain (ch, 0, n, masterGain);

            if (wet < 0.9999f)
            {
                const float dryGain = 1.0f - wet;
                for (int ch = 0; ch < 2; ++ch)
                {
                    buf.applyGain (ch, 0, n, wet);
                    for (int i = 0; i < n; ++i)
                    {
                        const int src = pos + i - dryDelaySamples;
                        const float d = src >= 0 ? dry.getSample (ch, src) : 0.0f;
                        buf.setSample (ch, i, buf.getSample (ch, i) + dryGain * d);
                    }
                }
            }

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    (ch == 0 ? outL : outR)[(size_t) (pos + i)] = buf.getSample (ch, i);

            for (int id = 0; id < kNumModules; ++id)
                gr[(size_t) id] = std::max (gr[(size_t) id], modules[(size_t) id]->grDb.load());
            compDb = modules[3]->gainCompDb.load();
        }

        if (maxGr) *maxGr = gr;
        if (finalCompDb) *finalCompDb = compDb;
    }
};

//==============================================================================
double goertzelPower (const std::vector<float>& x, double hz, int start, int count)
{
    const double w = 2.0 * M_PI * hz / kSampleRate;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double s0 = (double) x[(size_t) (start + i)] + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return power / (double) count;
}

struct Metrics
{
    double rmsDb = 0.0;
    double peakDb = 0.0;
    double toneDb = 0.0;      // Goertzel magnitude at the signal's primary tone
    double tone1kDb = 0.0;    // always 1 kHz, for cross-signal comparison
};

Metrics measure (const std::vector<float>& l, const std::vector<float>& r, double toneHz)
{
    const int start = (int) (kSettle * kSampleRate);
    const int count = (int) ((kSeconds - kSettle) * kSampleRate);
    double sum = 0.0, peak = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double a = l[(size_t) (start + i)], b = r[(size_t) (start + i)];
        sum += 0.5 * (a * a + b * b);
        peak = std::max (peak, std::max (std::abs (a), std::abs (b)));
    }
    Metrics m;
    m.rmsDb = db (std::sqrt (sum / count));
    m.peakDb = db (peak);
    if (toneHz > 0.0)
    {
        const double p = 0.5 * (goertzelPower (l, toneHz, start, count) + goertzelPower (r, toneHz, start, count));
        m.toneDb = 10.0 * std::log10 (std::max (p, 1.0e-20));
    }
    const double p1k = 0.5 * (goertzelPower (l, 1000.0, start, count) + goertzelPower (r, 1000.0, start, count));
    m.tone1kDb = 10.0 * std::log10 (std::max (p1k, 1.0e-20));
    return m;
}

//==============================================================================
std::vector<Order> allPermutations()
{
    std::vector<Order> out;
    Order o { 0, 1, 2, 3 };
    do { out.push_back (o); } while (std::next_permutation (o.begin(), o.end()));
    return out;
}

std::string orderLabel (const Order& o)
{
    static const char* names[kNumModules] { "EQ", "OPTO", "FET", "LIM" };
    std::string s;
    for (int i = 0; i < kNumModules; ++i)
    {
        if (i) s += " > ";
        s += names[o[(size_t) i]];
    }
    return s;
}

//==============================================================================
void reportPermutationTable (const char* heading, const Preset& p, const Signal& sig)
{
    std::printf ("\n### %s — preset `%s`, signal `%s`\n\n", heading, p.name.c_str(), sig.name.c_str());
    std::printf ("| # | order | RMS dBFS | peak dBFS | 1 kHz bin dB | EQ gr | Opto gr | FET gr | Lim gr | auto-gain dB |\n");
    std::printf ("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");

    const auto perms = allPermutations();
    int index = 0;
    for (auto& o : perms)
    {
        Chain chain;
        chain.configure (p);
        std::vector<float> l, r;
        std::array<float, kNumModules> gr {};
        float comp = 0.0f;
        chain.run (sig, o, l, r, &gr, &comp);
        const auto m = measure (l, r, sig.toneHz);
        std::printf ("| %d | %s | %.2f | %.2f | %.1f | %.1f | %.1f | %.1f | %.1f | %+.2f |\n",
                     ++index, orderLabel (o).c_str(), m.rmsDb, m.peakDb, m.tone1kDb,
                     gr[0], gr[1], gr[2], gr[3], comp);
    }
}

// Spread of RMS across all 24 orders = how order-dependent the chain is.
void reportSpread (const char* /*heading*/, const Preset& p, const Signal& sig)
{
    double minRms = 1e9, maxRms = -1e9, minPeak = 1e9, maxPeak = -1e9;
    Order worstMin {}, worstMax {};
    for (auto& o : allPermutations())
    {
        Chain chain;
        chain.configure (p);
        std::vector<float> l, r;
        chain.run (sig, o, l, r, nullptr, nullptr);
        const auto m = measure (l, r, sig.toneHz);
        if (m.rmsDb < minRms) { minRms = m.rmsDb; worstMin = o; }
        if (m.rmsDb > maxRms) { maxRms = m.rmsDb; worstMax = o; }
        minPeak = std::min (minPeak, m.peakDb);
        maxPeak = std::max (maxPeak, m.peakDb);
    }
    std::printf ("| %s | %s | %.2f | %.2f | **%.2f** | %.2f | %.2f | **%.2f** | `%s` | `%s` |\n",
                 p.name.c_str(), sig.name.c_str(), minRms, maxRms, maxRms - minRms,
                 minPeak, maxPeak, maxPeak - minPeak,
                 orderLabel (worstMin).c_str(), orderLabel (worstMax).c_str());
}

//==============================================================================
} // namespace

int main()
{
    std::printf ("# Measured reorder behaviour — zychanstrip DSP harness\n\n");
    std::printf ("Generated by `tools/research/permute.cpp` (includes the real `Source/Modules.h`).\n");
    std::printf ("Sample rate %.0f Hz, block %d, %.1f s per run, first %.1f s discarded.\n",
                 kSampleRate, kBlock, kSeconds, kSettle);
    std::printf ("`gr` columns are each module's own `grDb` (block maximum, dB).\n");

    const auto tone = makeTone ("sine 1 kHz @ -6 dBFS", 1000.0, 0.5);
    const auto multi = makeMultiTone();
    const auto trans = makeTransients();

    //------------------------------------------------------------------ spreads
    std::printf ("\n## 1. Order-dependence summary (all 24 permutations)\n\n");
    std::printf ("| preset | signal | RMS min | RMS max | **RMS spread** | peak min | peak max | **peak spread** | quietest order | loudest order |\n");
    std::printf ("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
    reportSpread ("", presetUnity(), multi);
    reportSpread ("", presetMastering (false), multi);
    reportSpread ("", presetMastering (true), multi);
    reportSpread ("", presetUnity(), trans);
    reportSpread ("", presetMastering (false), trans);
    reportSpread ("", presetMastering (true), trans);

    //------------------------------------------------------- full permutation
    std::printf ("\n## 2. Full permutation tables\n");
    reportPermutationTable ("Unity preset", presetUnity(), multi);
    reportPermutationTable ("Mastering, auto-gain OFF", presetMastering (false), multi);
    reportPermutationTable ("Mastering, auto-gain ON", presetMastering (true), multi);
    reportPermutationTable ("Mastering, auto-gain ON, transients", presetMastering (true), trans);

    //----------------------------------------------------------- dry/wet phase
    std::printf ("\n## 3. Dry/wet phase (`zychanstrip-mix-phase`)\n\n");
    {
        Preset p = presetUnity();
        for (auto& kv : p.values)
            if (kv.first == "lim_gain") kv.second = 0.0f;
        Chain chain;
        chain.configure (p);
        const int lat = chain.totalLatency();
        std::printf ("Reported chain latency: **%d samples** (%.3f ms at %.0f Hz).\n\n",
                     lat, 1000.0 * lat / kSampleRate, kSampleRate);
        std::printf ("| mix | dry delay | RMS dBFS | 1 kHz bin dB | vs 100%% wet |\n| --- | --- | --- | --- | --- |\n");

        std::vector<float> wetL, wetR;
        chain.run (tone, Order { 0, 1, 2, 3 }, wetL, wetR, nullptr, nullptr);
        const auto wet = measure (wetL, wetR, 1000.0);

        struct Case { const char* label; float mix; int delay; };
        const Case cases[] {
            { "100 % (reference)", 100.0f, 0 },
            { "50 %, uncompensated (shipped)", 50.0f, 0 },
            { "50 %, dry delayed by latency (proposed fix)", 50.0f, lat },
            { "25 %, uncompensated (shipped)", 25.0f, 0 },
            { "25 %, dry delayed by latency (proposed fix)", 25.0f, lat },
        };
        for (auto& c : cases)
        {
            Chain ch;
            ch.configure (p);
            ch.mixPercent = c.mix;
            std::vector<float> l, r;
            ch.run (tone, Order { 0, 1, 2, 3 }, l, r, nullptr, nullptr, c.delay);
            const auto m = measure (l, r, 1000.0);
            std::printf ("| %s | %d | %.2f | %.1f | %+.2f dB |\n",
                         c.label, c.delay, m.rmsDb, m.tone1kDb, m.tone1kDb - wet.tone1kDb);
        }
        const double notchHz = kSampleRate / (2.0 * lat);
        std::printf ("\nFirst comb notch for an uncompensated %d-sample dry path: **%.1f Hz** "
                     "(and every odd multiple).\n", lat, notchHz);
    }

    //------------------------------------------------------- auto-gain reference
    std::printf ("\n## 4. Auto-gain reference (`zychanstrip-autogain-order`)\n\n");
    {
        const Order limLast { 0, 2, 1, 3 };
        const Order limFirst { 3, 0, 2, 1 };
        std::printf ("| preset | order | RMS dBFS | limiter auto-gain dB |\n| --- | --- | --- | --- |\n");
        for (int ag = 0; ag <= 1; ++ag)
        {
            for (auto& o : { limLast, limFirst })
            {
                Chain chain;
                chain.configure (presetMastering (ag == 1));
                std::vector<float> l, r;
                float comp = 0.0f;
                chain.run (multi, o, l, r, nullptr, &comp);
                const auto m = measure (l, r, multi.toneHz);
                std::printf ("| %s | %s | %.2f | %+.2f |\n",
                             ag ? "auto-gain ON" : "auto-gain OFF", orderLabel (o).c_str(), m.rmsDb, comp);
            }
        }
    }

    //----------------------------------------------------- ceiling vs order
    std::printf ("\n## 5. Limiter ceiling vs module order\n\n");
    {
        std::printf ("Ceiling set to -1.0 dBFS. Any peak above that means a module ran after the limiter.\n\n");
        std::printf ("| order | peak dBFS | exceeds ceiling |\n| --- | --- | --- |\n");
        for (auto& o : allPermutations())
        {
            Chain chain;
            chain.configure (presetMastering (false));
            std::vector<float> l, r;
            chain.run (multi, o, l, r, nullptr, nullptr);
            const auto m = measure (l, r, multi.toneHz);
            std::printf ("| %s | %.2f | %s |\n", orderLabel (o).c_str(), m.peakDb,
                         m.peakDb > -1.0 ? "**YES**" : "no");
        }
    }

    //----------------------------------------------- detector coupling detail
    std::printf ("\n## 6. Detector coupling (mastering preset, multitone)\n\n");
    {
        std::printf ("`gr` = gain reduction reported by each module for the whole run.\n\n");
        std::printf ("| order | EQ gr | Opto gr | FET gr | Lim gr | auto-gain dB | RMS dBFS |\n| --- | --- | --- | --- | --- | --- | --- |\n");
        for (auto& o : { Order { 0, 2, 1, 3 }, Order { 0, 1, 2, 3 }, Order { 2, 0, 1, 3 }, Order { 3, 0, 2, 1 } })
        {
            Chain chain;
            chain.configure (presetMastering (false));
            std::vector<float> l, r;
            std::array<float, kNumModules> gr {};
            float comp = 0.0f;
            chain.run (multi, o, l, r, &gr, &comp);
            const auto m = measure (l, r, multi.toneHz);
            std::printf ("| %s | %.1f | %.1f | %.1f | %.1f | %+.2f | %.2f |\n",
                         orderLabel (o).c_str(), gr[0], gr[1], gr[2], gr[3], comp, m.rmsDb);
        }
    }

    //----------------------------------------------------------- colour stacking
    std::printf ("\n## 7. Colour-stage stacking (EQ colour + FET colour + tube colour + Enhance)\n\n");
    {
        std::printf ("Same preset, only the relative order of EQ / FET / Opto changes; the limiter is last.\n\n");
        std::printf ("| order | 100 Hz bin | 1 kHz bin | 8 kHz bin | RMS dBFS |\n| --- | --- | --- | --- | --- |\n");
        for (auto& o : { Order { 0, 2, 1, 3 }, Order { 2, 1, 0, 3 }, Order { 1, 2, 0, 3 }, Order { 2, 0, 1, 3 } })
        {
            Chain chain;
            chain.configure (presetMastering (false));
            std::vector<float> l, r;
            chain.run (multi, o, l, r, nullptr, nullptr);
            const int start = (int) (kSettle * kSampleRate);
            const int count = (int) ((kSeconds - kSettle) * kSampleRate);
            auto bin = [&] (double hz)
            {
                const double p = 0.5 * (goertzelPower (l, hz, start, count) + goertzelPower (r, hz, start, count));
                return 10.0 * std::log10 (std::max (p, 1.0e-20));
            };
            const auto m = measure (l, r, 1000.0);
            std::printf ("| %s | %.1f | %.1f | %.1f | %.2f |\n",
                         orderLabel (o).c_str(), bin (100.0), bin (1000.0), bin (8000.0), m.rmsDb);
        }
    }

    //-------------------------------------------------------------- invariance
    std::printf ("\n## 8. Invariants that re-ordering cannot change\n\n");
    {
        Chain chain;
        chain.configure (presetMastering (true));
        std::printf ("- reported chain latency: %d samples for every order (module latencies are summed once in `prepareToPlay`)\n",
                     chain.totalLatency());
        std::printf ("- per-module latency: EQ %d, Opto %d, FET %d, Limiter %d\n",
                     chain.modules[0]->getLatencySamples(), chain.modules[1]->getLatencySamples(),
                     chain.modules[2]->getLatencySamples(), chain.modules[3]->getLatencySamples());
    }

    std::printf ("\nDONE\n");
    return 0;
}
