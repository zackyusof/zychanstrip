#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Eq560.h"

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
    std::atomic<float> gainCompDb { 0.0f }; // Enhance Auto Gain correction, stereo linked

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
/** Channel strip: high-pass, ten-band EQ560 and a stereo-linked bus-style compressor. */
class ConsoleModule : public Module
{
public:
    ConsoleModule (juce::AudioProcessorValueTreeState& state, juce::String paramPrefix)
        : Module (state, std::move (paramPrefix))
    {
        for (size_t i = 0; i < bandParameters.size(); ++i)
            bandParameters[i] = apvts.getRawParameterValue (prefix + Eq560::bandIds[i]);
    }

    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        for (auto& filter : hpf) filter = {};
        updateFilters();
        eq.prepare (sr);
        env = 0.0f;
    }

protected:
    void process (juce::AudioBuffer<float>& buf) override
    {
        updateFilters();
        const int n = buf.getNumSamples();
        const int nch = std::min (2, buf.getNumChannels());

        for (int i = 0; i < n; ++i)
        {
            eq.beginSample();
            for (int ch = 0; ch < nch; ++ch)
                buf.setSample (ch, i, (float) eq.processSample (ch, hpf[(size_t) ch].tick (buf.getSample (ch, i))));
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
    // Same second-order Butterworth HPF as before, with stack-only coefficient updates.
    struct HighPass
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void configure (double sr, double hz)
        {
            const double k = std::tan (juce::MathConstants<double>::pi * hz / sr);
            const double norm = 1.0 / (1.0 + k / 0.707 + k * k);
            b0 = norm; b1 = -2.0 * norm; b2 = norm;
            a1 = 2.0 * (k * k - 1.0) * norm;
            a2 = (1.0 - k / 0.707 + k * k) * norm;
        }
        double tick (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
            return y;
        }
    };
    void updateFilters()
    {
        const float frequency = get ("hpf");
        for (auto& filter : hpf) filter.configure (sampleRate, frequency);
        std::array<float, 10> values;
        for (size_t i = 0; i < values.size(); ++i) values[i] = bandParameters[i]->load();
        eq.setBandGains (values);
    }

    std::array<HighPass, 2> hpf;
    Eq560 eq;
    std::array<std::atomic<float>*, 10> bandParameters {};
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
    LimiterModule (juce::AudioProcessorValueTreeState& state, juce::String paramPrefix)
        : Module (state, std::move (paramPrefix)),
          autoGainParameter (apvts.getRawParameterValue (prefix + "auto_gain")) {}

    void prepare (double sr, int mb) override
    {
        Module::prepare (sr, mb);
        la = std::max (2, (int) std::ceil (0.0015 * sr));
        for (auto& d : delay) d.assign ((size_t) la, 0.0f);
        for (auto& d : dryDelay) d.assign ((size_t) la, 0.0f);
        reqRing.assign ((size_t) la, 1.0f);
        avgRing.assign ((size_t) la, 1.0f);
        sum = (double) la; wp = 0; relGain = 1.0f;
        dryEnergy = wetEnergy = 0.0; compensationDb = 0.0f;
        energyCoef = std::exp (-1.0 / (0.2 * sr));
        compensationCoef = timeCoef (100.0f, sr);
        gainCompDb.store (0.0f);
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
        const bool autoGain = active && autoGainParameter->load() > 0.5f;
        float minG = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            float in[2] { 0.0f, 0.0f };
            for (int ch = 0; ch < nch; ++ch)
            {
                const float dry = buf.getSample (ch, i);
                dryDelay[ch][(size_t) wp] = dry;
                float x = dry * drive;
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
            float wet[2] { 0.0f, 0.0f };
            double dryPower = 0.0, wetPower = 0.0;
            for (int ch = 0; ch < nch; ++ch)
            {
                delay[ch][(size_t) wp] = in[ch];
                const float delayedDry = dryDelay[ch][(size_t) rd];
                const float y = delay[ch][(size_t) rd] * (active ? g : 1.0f);
                wet[ch] = active ? juce::jlimit (-ceil, ceil, y) : y;
                dryPower += (double) delayedDry * delayedDry;
                wetPower += (double) wet[ch] * wet[ch];
            }
            const double channelScale = nch > 0 ? 1.0 / nch : 1.0;
            dryEnergy = energyCoef * dryEnergy + (1.0 - energyCoef) * dryPower * channelScale;
            wetEnergy = energyCoef * wetEnergy + (1.0 - energyCoef) * wetPower * channelScale;
            float targetDb = 0.0f;
            if (autoGain && dryEnergy > 1.0e-12 && wetEnergy > 1.0e-16)
                targetDb = juce::jlimit (-36.0f, 36.0f, (float) (10.0 * std::log10 (dryEnergy / wetEnergy)));
            compensationDb = compensationCoef * compensationDb + (1.0f - compensationCoef) * targetDb;
            if (! active) { compensationDb = 0.0f; dryEnergy = wetEnergy = 0.0; }
            const float compensation = active ? dbToGain (compensationDb) : 1.0f;
            float outputPeak = 0.0f;
            for (int ch = 0; ch < nch; ++ch) outputPeak = std::max (outputPeak, std::abs (wet[ch] * compensation));
            // Linked safety gain preserves the stereo image while keeping the existing ceiling.
            const float safety = active && outputPeak > ceil ? ceil / outputPeak : 1.0f;
            for (int ch = 0; ch < nch; ++ch)
                buf.setSample (ch, i, wet[ch] * compensation * safety);
            wp = (wp + 1) % la;
        }
        grDb.store (active ? -gainToDb (minG) : 0.0f);
        gainCompDb.store (active ? compensationDb : 0.0f);
    }

    std::atomic<float>* autoGainParameter;
    std::vector<float> delay[2], dryDelay[2], reqRing, avgRing;
    int la = 64, wp = 0;
    double sum = 64.0;
    float relGain = 1.0f;
    double dryEnergy = 0.0, wetEnergy = 0.0, energyCoef = 0.0;
    float compensationDb = 0.0f, compensationCoef = 0.0f;
};

} // namespace rack
