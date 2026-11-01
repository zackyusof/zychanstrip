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
// Analog Color & Saturation Models
//==============================================================================

/** API 2520 discrete op-amp & 2503 output transformer analog saturation.
    Produces API's classic punchy midrange with subtle 2nd harmonic asymmetry
    and 3rd harmonic transformer iron saturation. */
inline float apiConsoleColor (float x)
{
    const float x2 = x * x;
    const float shaped = x - 0.032f * x2 + 0.018f * x * x2;
    return std::tanh (shaped * 1.035f) * 0.966f;
}

/** Teletronix LA-2A 12AX7/12BH7 triode vacuum tube & UTC transformer analog color.
    Produces rich even-order (2nd harmonic) tube warmth with soft musical compression. */
inline float optoTubeColor (float x)
{
    const float x2 = x * x;
    const float tube = x + 0.075f * (x2 - std::abs (x) * x) - 0.025f * x * x2;
    const float a = std::abs (tube);
    if (a < 0.55f) return tube;
    const float sign = tube < 0.0f ? -1.0f : 1.0f;
    return sign * (0.55f + 0.45f * std::tanh ((a - 0.55f) / 0.45f));
}

/** Urei / UA 1176 Class-A FET gain reduction & 11754 output transformer analog color.
    Produces aggressive odd harmonics (predominantly 3rd harmonic), punchy bite and transformer iron saturation. */
inline float fetColor (float x)
{
    const float a = std::abs (x);
    if (a < 0.30f)
        return x * (1.0f - 0.06f * x * x);
    const float sign = x < 0.0f ? -1.0f : 1.0f;
    return sign * (0.30f + 0.70f * std::tanh ((a - 0.30f) / 0.70f));
}

/** Master console & tape saturation with Enhance harmonic exciter.
    Blends analog tape 3rd harmonic compression with subtle transformer warmth. */
inline float analogMasterSaturation (float x, float enh)
{
    if (enh <= 0.0f)
        return x;
    const float drive = 1.0f + enh * 0.45f;
    const float driven = x * drive;
    const float tape = std::tanh (driven);
    return (1.0f - enh * 0.35f) * x + (enh * 0.35f) * (tape / drive);
}

//==============================================================================
class Module
{
public:
    Module (juce::AudioProcessorValueTreeState& s, juce::String paramPrefix)
        : apvts (s), prefix (std::move (paramPrefix)) {}
    virtual ~Module() = default;

    virtual void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        maxBlockSize = maxBlock;
        outPeak.store (0.0f);
        reset();
    }

    /** Return the module's internal DSP state to its power-on condition.
        Never touches parameters; called from prepare() and from the audio thread after
        the operator's RESET request, so it must not allocate on the audio thread. */
    virtual void reset() {}

    virtual int  getLatencySamples() const { return 0; }

    void run (juce::AudioBuffer<float>& buf)
    {
        if (get ("bypass") > 0.5f) { processBypassed (buf); grDb.store (0.0f); outPeak.store (0.0f); }
        else                         process (buf);
    }

    /** Order-independent auto-gain reference, supplied by the processor each block.
        `chainInputRaw` is the block's per-sample input energy measured at the very start of the
        whole chain (before any module runs), so a correction derived from it cannot depend on
        where this module sits in the rack. */
    virtual void setChainReference (const float* chainInputRaw, int numSamples)
    {
        juce::ignoreUnused (chainInputRaw, numSamples);
    }

    /** True output of the whole chain (post master gain, post dry/wet) for this block.
        Auto Gain divides the chain input by THIS, never by its own output, so the correction is
        identical no matter where the module sits in the rack. Called once per block, after every
        module has run. */
    virtual void setFinalOutputReference (const float* finalOutEnergy, int numSamples)
    {
        juce::ignoreUnused (finalOutEnergy, numSamples);
    }

    std::atomic<float> grDb { 0.0f };   // gain reduction for the UI meter
    std::atomic<float> gainCompDb { 0.0f }; // Enhance Auto Gain correction, stereo linked
    std::atomic<float> outPeak { 0.0f }; // Output peak level for UI meters

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
    int maxBlockSize = 0;
};

//==============================================================================
/** Pure ten-band EQ560. Retired console dynamics controls remain registered for host compatibility. */
class ConsoleModule : public Module
{
public:
    ConsoleModule (juce::AudioProcessorValueTreeState& state, juce::String paramPrefix)
        : Module (state, std::move (paramPrefix))
    {
        for (size_t i = 0; i < bandParameters.size(); ++i)
            bandParameters[i] = apvts.getRawParameterValue (prefix + Eq560::bandIds[i]);
    }

    void reset() override
    {
        updateBands();
        eq.prepare (sampleRate);
        env = 0.0f;
        grDb.store (0.0f);
        gainCompDb.store (0.0f);
        outPeak.store (0.0f);
    }

protected:
    void process (juce::AudioBuffer<float>& buf) override
    {
        updateBands();
        const int n = buf.getNumSamples();
        const int nch = std::min (2, buf.getNumChannels());

        const float thresh = get ("thresh");
        const float ratio  = get ("ratio");
        const float atkMs  = get ("attack");
        const float relMs  = get ("release");
        const float outG   = dbToGain (get ("out"));
        const float aC = timeCoef (atkMs, sampleRate);
        const float rC = timeCoef (relMs, sampleRate);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            eq.beginSample();
            float pk = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
            {
                const float s = (float) eq.processSample (ch, buf.getSample (ch, i));
                buf.setSample (ch, i, apiConsoleColor (s));
                pk = std::max (pk, std::abs (buf.getSample (ch, i)));
            }
            env = smooth (env, gainReductionDb (gainToDb (pk), thresh, ratio, 2.0f), aC, rC);
            maxGr = std::max (maxGr, env);
            const float g = dbToGain (-env) * outG;
            for (int ch = 0; ch < nch; ++ch)
                buf.getWritePointer (ch)[i] *= g;
        }
        grDb.store (maxGr);
    }

private:
    void updateBands()
    {
        std::array<float, 10> values;
        for (size_t i = 0; i < values.size(); ++i) values[i] = bandParameters[i]->load();
        eq.setBandGains (values);
    }

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
    void reset() override
    {
        fast = slow = 0.0f;
        grDb.store (0.0f);
        gainCompDb.store (0.0f);
        outPeak.store (0.0f);
    }

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
                buf.getWritePointer (ch)[i] = optoTubeColor (buf.getSample (ch, i) * g);
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
    void reset() override
    {
        env = 0.0f;
        grDb.store (0.0f);
        gainCompDb.store (0.0f);
        outPeak.store (0.0f);
    }

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
                buf.getWritePointer (ch)[i] = fetColor (buf.getSample (ch, i) * g);
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

    void reset() override
    {
        // Ring sizes follow the sample rate; resize only when the rate actually changed so a
        // reset from the audio thread cannot allocate in the common case.
        const int wanted = std::max (2, (int) std::ceil (0.0015 * sampleRate));
        la = wanted;
        for (auto& d : delay) if ((int) d.size() != la) d.assign ((size_t) la, 0.0f); else std::fill (d.begin(), d.end(), 0.0f);
        for (auto& d : dryDelay) if ((int) d.size() != la) d.assign ((size_t) la, 0.0f); else std::fill (d.begin(), d.end(), 0.0f);
        if ((int) reqRing.size() != la) reqRing.assign ((size_t) la, 1.0f); else std::fill (reqRing.begin(), reqRing.end(), 1.0f);
        if ((int) avgRing.size() != la) avgRing.assign ((size_t) la, 1.0f); else std::fill (avgRing.begin(), avgRing.end(), 1.0f);
        sum = (double) la; wp = 0; relGain = 1.0f;
        dryEnergy = wetEnergy = 0.0; compensationDb = 0.0f;
        energyCoef = std::exp (-1.0 / (0.2 * sampleRate));
        compensationCoef = timeCoef (100.0f, sampleRate);
        grDb.store (0.0f);
        gainCompDb.store (0.0f);
        outPeak.store (0.0f);
    }

    int getLatencySamples() const override { return la - 1; }

    void setChainReference (const float* chainInputRaw, int numSamples) override
    {
        chainInput = chainInputRaw;
        chainInputSamples = numSamples;
    }

    void setFinalOutputReference (const float* finalOutEnergy, int numSamples) override
    {
        finalOut = finalOutEnergy;
        finalOutSamples = numSamples;
    }

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
        float blockOutPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            float in[2] { 0.0f, 0.0f };
            for (int ch = 0; ch < nch; ++ch)
            {
                const float dry = buf.getSample (ch, i);
                dryDelay[ch][(size_t) wp] = dry;
                float x = dry * drive;
                if (active) x = analogMasterSaturation (x, enh);
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
            // Reference the TRUE chain input (measured before any module) against the TRUE chain
            // output (measured after master + mix), so upstream Console/FET/Opto gain and any
            // module downstream of the limiter cannot skew the correction. Falls back to the
            // module's own dry/wet pair when the processor supplies no reference (unit use).
            const bool haveChainRef = chainInput != nullptr && i < chainInputSamples;
            const double refPower = haveChainRef ? (double) chainInput[i] : dryPower * channelScale;
            dryEnergy = energyCoef * dryEnergy + (1.0 - energyCoef) * refPower;
            const bool haveFinal = finalOut != nullptr && i < finalOutSamples;
            const double outPower = haveFinal ? (double) finalOut[i] : wetPower * channelScale;
            wetEnergy = energyCoef * wetEnergy + (1.0 - energyCoef) * outPower;
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
            {
                const float s = wet[ch] * compensation * safety;
                buf.setSample (ch, i, s);
                blockOutPeak = std::max (blockOutPeak, std::abs (s));
            }
            wp = (wp + 1) % la;
        }
        grDb.store (active ? -gainToDb (minG) : 0.0f);
        gainCompDb.store (active ? compensationDb : 0.0f);
        outPeak.store (active ? blockOutPeak : 0.0f);
    }

    std::atomic<float>* autoGainParameter;
    const float* chainInput = nullptr;   // block input energy from the chain start
    int chainInputSamples = 0;
    const float* finalOut = nullptr;     // true chain output energy, post master + mix
    int finalOutSamples = 0;
    std::vector<float> delay[2], dryDelay[2], reqRing, avgRing;
    int la = 64, wp = 0;
    double sum = 64.0;
    float relGain = 1.0f;
    double dryEnergy = 0.0, wetEnergy = 0.0, energyCoef = 0.0;
    float compensationDb = 0.0f, compensationCoef = 0.0f;
};

} // namespace rack
