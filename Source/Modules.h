#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace rack
{
//==============================================================================
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float timeCoef (float ms, double sr) { return std::exp (-1.0f / (0.001f * ms * (float) sr)); }

/** One-pole smoother with separate attack (rising) and release (falling) coefficients. */
inline float smooth (float state, float target, float attackCoef, float releaseCoef)
{
    const float c = target > state ? attackCoef : releaseCoef;
    return c * state + (1.0f - c) * target;
}

/** Gain reduction in dB (>= 0) for a static compressor curve with optional soft knee. */
inline float gainReductionDb (float levelDb, float threshDb, float ratio, float kneeDb)
{
    const float over  = levelDb - threshDb;
    const float slope = 1.0f - 1.0f / ratio;

    if (kneeDb <= 0.0f)             return over > 0.0f ? slope * over : 0.0f;
    if (2.0f * over < -kneeDb)      return 0.0f;
    if (2.0f * std::abs (over) <= kneeDb)
    {
        const float x = over + kneeDb * 0.5f;
        return slope * x * x / (2.0f * kneeDb);
    }
    return slope * over;
}

/** Transparent below 0.7, smooth saturation above. Stand-in for analog output-stage behaviour. */
inline float softClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.7f) return x;
    const float y = 0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f);
    return x < 0.0f ? -y : y;
}

//==============================================================================
class Module
{
public:
    Module (juce::AudioProcessorValueTreeState& s, juce::String paramPrefix)
        : apvts (s), prefix (std::move (paramPrefix)) {}
    virtual ~Module() = default;

    virtual void prepare (double sr, int /*maxBlock*/) { sampleRate = sr; }
    virtual int  getLatencySamples() const { return 0; }

    void run (juce::AudioBuffer<float>& buf)
    {
        if (get ("bypass") > 0.5f) { processBypassed (buf); grDb.store (0.0f); }
        else                         process (buf);
    }

    std::atomic<float> grDb { 0.0f };   // gain reduction for the UI meter

protected:
    virtual void process (juce::AudioBuffer<float>&) = 0;
    virtual void processBypassed (juce::AudioBuffer<float>&) {}

    float get (const juce::String& name) const
    {
        return apvts.getRawParameterValue (prefix + name)->load();
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String prefix;
    double sampleRate = 44100.0;
};

//==============================================================================
/** Channel strip: high-pass, 4-band EQ and a stereo-linked bus-style compressor. */
class ConsoleModule : public Module
{
public:
    using Module::Module;

    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        for (int c = 0; c < 2; ++c) { hpf[c].reset(); lf[c].reset(); lmf[c].reset(); hmf[c].reset(); hf[c].reset(); }
        cacheValid = false;
        env = 0.0f;
    }

protected:
    void process (juce::AudioBuffer<float>& buf) override
    {
        updateFilters();
        const int n = buf.getNumSamples();
        const int nch = std::min (2, buf.getNumChannels());

        for (int ch = 0; ch < nch; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float x = hpf[ch].processSample (d[i]);
                x = lf[ch].processSample (x);
                x = lmf[ch].processSample (x);
                x = hmf[ch].processSample (x);
                x = hf[ch].processSample (x);
                d[i] = x;
            }
        }

        // Compressor
        const float thresh = get ("thresh"), ratio = get ("ratio");
        const float aC = timeCoef (get ("attack"), sampleRate);
        const float rC = timeCoef (get ("release"), sampleRate);
        const float outG = dbToGain (get ("out"));
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                pk = std::max (pk, std::abs (buf.getSample (ch, i)));

            env = smooth (env, gainReductionDb (gainToDb (pk), thresh, ratio, 4.0f), aC, rC);
            maxGr = std::max (maxGr, env);
            const float g = dbToGain (-env) * outG;

            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                buf.getWritePointer (ch)[i] = softClip (buf.getSample (ch, i) * g);
        }
        grDb.store (maxGr);
    }

private:
    using Filt = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    void updateFilters()
    {
        const std::array<float, 7> v { get ("hpf"), get ("lf"), get ("lmf_f"), get ("lmf"),
                                       get ("hmf_f"), get ("hmf"), get ("hf") };
        if (cacheValid && v == cache) return;
        cache = v; cacheValid = true;

        auto cH  = Coeffs::makeHighPass  (sampleRate, v[0], 0.707f);
        auto cLf = Coeffs::makeLowShelf  (sampleRate, 100.0f, 0.7f, dbToGain (v[1]));
        auto cLm = Coeffs::makePeakFilter (sampleRate, v[2], 0.9f, dbToGain (v[3]));
        auto cHm = Coeffs::makePeakFilter (sampleRate, v[4], 0.9f, dbToGain (v[5]));
        auto cHf = Coeffs::makeHighShelf (sampleRate, 8000.0f, 0.7f, dbToGain (v[6]));

        for (int c = 0; c < 2; ++c)
        {
            hpf[c].coefficients = cH;  lf[c].coefficients = cLf;
            lmf[c].coefficients = cLm; hmf[c].coefficients = cHm; hf[c].coefficients = cHf;
        }
    }

    Filt hpf[2], lf[2], lmf[2], hmf[2], hf[2];
    std::array<float, 7> cache {};
    bool cacheValid = false;
    float env = 0.0f;
};

//==============================================================================
/** Optical-style leveller: program-dependent two-stage release, soft knee, compress/limit switch. */
class OptoModule : public Module
{
public:
    using Module::Module;
    void prepare (double sr, int mb) override { Module::prepare (sr, mb); fast = slow = 0.0f; }

protected:
    void process (juce::AudioBuffer<float>& buf) override
    {
        const int n = buf.getNumSamples(), nch = buf.getNumChannels();
        const float pr = get ("peak") * 0.01f;
        const float makeup = dbToGain (get ("gain") * 0.01f * 30.0f);
        const bool limit = get ("mode") > 0.5f;
        const float thresh = -pr * 40.0f;
        const float ratio = limit ? 12.0f : 3.0f;

        const float aC = timeCoef (10.0f, sampleRate);
        const float fC = timeCoef (60.0f, sampleRate);
        const float sC = timeCoef (1800.0f, sampleRate);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            for (int ch = 0; ch < nch; ++ch) pk = std::max (pk, std::abs (buf.getSample (ch, i)));

            const float gr = gainReductionDb (gainToDb (pk), thresh, ratio, 10.0f);
            fast = smooth (fast, gr, aC, fC);
            slow = smooth (slow, gr * 0.6f, aC, sC);
            const float applied = std::max (fast, slow);
            maxGr = std::max (maxGr, applied);

            const float g = dbToGain (-applied) * makeup;
            for (int ch = 0; ch < nch; ++ch)
                buf.getWritePointer (ch)[i] = softClip (buf.getSample (ch, i) * g);
        }
        grDb.store (maxGr);
    }

private:
    float fast = 0.0f, slow = 0.0f;
};

//==============================================================================
/** FET-style compressor: input drives a fixed threshold, very fast attack, selectable ratio. */
class FetModule : public Module
{
public:
    using Module::Module;
    void prepare (double sr, int mb) override { Module::prepare (sr, mb); env = 0.0f; }

protected:
    void process (juce::AudioBuffer<float>& buf) override
    {
        static constexpr float ratios[] { 4.0f, 8.0f, 12.0f, 20.0f, 30.0f };
        const int n = buf.getNumSamples(), nch = buf.getNumChannels();
        const float inG  = dbToGain (get ("input"));
        const float outG = dbToGain (get ("output"));
        const float atkMs = juce::jmap (get ("attack"),  1.0f, 7.0f, 0.8f, 0.02f);
        const float relMs = juce::jmap (get ("release"), 1.0f, 7.0f, 1100.0f, 50.0f);
        const float ratio = ratios[juce::jlimit (0, 4, (int) get ("ratio"))];
        const float aC = timeCoef (atkMs, sampleRate), rC = timeCoef (relMs, sampleRate);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
            {
                float* d = buf.getWritePointer (ch);
                d[i] *= inG;
                pk = std::max (pk, std::abs (d[i]));
            }
            env = smooth (env, gainReductionDb (gainToDb (pk), -10.0f, ratio, 2.0f), aC, rC);
            maxGr = std::max (maxGr, env);
            const float g = dbToGain (-env) * outG;
            for (int ch = 0; ch < nch; ++ch)
                buf.getWritePointer (ch)[i] = softClip (buf.getSample (ch, i) * g);
        }
        grDb.store (maxGr);
    }

private:
    float env = 0.0f;
};

//==============================================================================
/** Look-ahead brick-wall limiter (sliding-min + moving-average) with a drive/saturation control. */
class LimiterModule : public Module
{
public:
    using Module::Module;

    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        la = std::max (2, (int) std::ceil (0.0015 * sr));
        for (auto& d : delay) d.assign ((size_t) la, 0.0f);
        reqRing.assign ((size_t) la, 1.0f);
        avgRing.assign ((size_t) la, 1.0f);
        sum = (double) la; wp = 0; relGain = 1.0f;
    }

    int getLatencySamples() const override { return la - 1; }

protected:
    void process (juce::AudioBuffer<float>& buf) override         { runLimiter (buf, true); }
    void processBypassed (juce::AudioBuffer<float>& buf) override { runLimiter (buf, false); }

private:
    void runLimiter (juce::AudioBuffer<float>& buf, bool active)
    {
        const int n = buf.getNumSamples(), nch = std::min (2, buf.getNumChannels());
        const float enh = get ("enhance") * 0.01f;
        const float drive = active ? dbToGain (get ("gain") + enh * 12.0f) : 1.0f;
        const float ceil = dbToGain (get ("ceiling"));
        const float rC = timeCoef (get ("release"), sampleRate);
        float minG = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            float in[2] { 0.0f, 0.0f };
            for (int ch = 0; ch < nch; ++ch)
            {
                float x = buf.getSample (ch, i) * drive;
                if (active && enh > 0.0f) x = x + (std::tanh (x) - x) * enh * 0.5f;
                in[ch] = x;
                pk = std::max (pk, std::abs (x));
            }

            const float req = (active && pk > ceil) ? ceil / pk : 1.0f;
            reqRing[(size_t) wp] = req;
            float mn = 1.0f;
            for (float r : reqRing) mn = std::min (mn, r);

            sum += (double) mn - (double) avgRing[(size_t) wp];
            avgRing[(size_t) wp] = mn;
            const float avg = (float) (sum / la);

            relGain = avg < relGain ? avg : rC * relGain + (1.0f - rC) * avg;
            const float g = std::min (relGain, avg);
            minG = std::min (minG, g);

            const int rd = (wp + 1) % la;
            for (int ch = 0; ch < nch; ++ch)
            {
                delay[ch][(size_t) wp] = in[ch];
                float y = delay[ch][(size_t) rd] * (active ? g : 1.0f);
                buf.getWritePointer (ch)[i] = active ? juce::jlimit (-ceil, ceil, y) : y;
            }
            wp = (wp + 1) % la;
        }
        grDb.store (active ? -gainToDb (minG) : 0.0f);
    }

    std::vector<float> delay[2], reqRing, avgRing;
    int la = 64, wp = 0;
    double sum = 64.0;
    float relGain = 1.0f;
};

} // namespace rack
