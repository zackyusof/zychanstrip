#pragma once

#include <cstdint>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Eq560.h"
#include "ColourEngine.h"

namespace rack
{
//==============================================================================
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float timeCoef (float ms, double sr) { return std::exp (-1.0f / (0.001f * ms * (float) sr)); }

//==============================================================================
/** Fast replacements for the three shapes the audio path calls per sample.

    Every one of these replaces a libm call (`pow`, `log10`, `tanh`) with `exp2`/one divide,
    which is what the prior-art survey found the mature implementations doing
    (`docs/research/dsp-optimization-prior-art.md`, F1/F2: JUCE's own
    `SpecialFunctions::besselI0` is a rational polynomial; Airwindows clipping is pure
    arithmetic). All four are within float rounding of the libm original; the null test
    (`tools/research/nullcmp.py`) is the gate, not the claim. */

/// exp2 for a float. Clang lowers `__builtin_exp2f` to the arm64 `fexp2` instruction, so
/// this is one cycle-ish and needs no inline asm (which is not legal under strict ISO C++).
inline float exp2Fast (float x) noexcept
{
#if defined (__clang__) || defined (__GNUC__)
    return __builtin_exp2f (x);
#else
    return std::exp2 (x);
#endif
}

/** 10^(db/20) — one exp2 instead of log10-then-pow. 1 ULP-class accuracy. */
inline float dbToGainFast (float db) noexcept
{
    return exp2Fast (db * 0.16609640474436813f);   // db * (log2(10) / 20)
}

/** 20*log10(x) with the same 1e-9 floor as `gainToDb`. */
inline float gainToDbFast (float g) noexcept
{
    const float x = g < 1.0e-9f ? 1.0e-9f : g;
    return 6.0205999132796239f * std::log2 (x);    // 20/log10(2)
}


namespace detail
{
constexpr int kTanhAbsTableSize = 524289;   // 87381 points/unit over [0, 6]; error ~ -140 dBFS

inline const float* tanhTable()
{
    // Single initialisation per process; the audio path only ever reads it.
    static const float* table = [] {
        auto* t = new float[kTanhAbsTableSize];
        for (int i = 0; i < kTanhAbsTableSize; ++i)
        {
            const double x = 6.0 * (double) i / (double) (kTanhAbsTableSize - 1);
            t[i] = (float) std::tanh (x);
        }
        return t;
    }();
    return table;
}
} // namespace detail

//==============================================================================
// The five instantaneous shapes below. `apiConsoleColor`, `optoTubeColor`, `fetColor` and
// `analogMasterSaturation` are PURE FUNCTIONS of one input sample - no state at all - so their
// `tanh` is evaluated through a lookup table instead of a per-sample libm call
// (`docs/research/dsp-optimization-prior-art.md`, F1). The table is built once per process
// (32 KiB) and linearly interpolated, giving < 1e-6 absolute error, far below the 24-bit floor.
// A closed-form rational replacement was measured and rejected: its 1.2e-2 error showed up as
// -32 dBFS in the null test.
/** tanh through a lookup table with linear interpolation.
    The call sites feed this arguments up to ~5 (e.g. `fetColor` maps |x| up to 4.0 through
    `(a - 0.3) / 0.7`), so the table covers ±6; clamping at ±4 measured as 1.2e-5 of error in
    the null test. Past ±6 the function returns ±1 - tanh(6) is 0.99998769, inside float rounding. */
/** tanh through a table of |x| in [0, 6] with linear interpolation.
    The table is built once per process and only read afterwards. The sign is split out
    explicitly so the index can never go negative (a `(x + 6)` form floors to -1 for inputs a
    hair below -6, which earlier cost 1.2e-5 of null-test error). */
inline float tableTanh (float x) noexcept
{
    const bool negative = x < 0.0f;
    const float ax = negative ? -x : x;
    if (ax >= 6.0f) return negative ? -1.0f : 1.0f;

    constexpr float scale = (float) (detail::kTanhAbsTableSize - 1) * (1.0f / 6.0f);
    float pos = ax * scale;
    if (pos > (float) (detail::kTanhAbsTableSize - 2)) pos = (float) (detail::kTanhAbsTableSize - 2);
    if (! (pos >= 0.0f)) pos = 0.0f;

    const int idx = (int) pos;
    const float t = pos - (float) idx;
    const float* tbl = detail::tanhTable();
    const float a = tbl[idx], b = tbl[idx + 1];
    const float y = a + (b - a) * t;
    return negative ? -y : y;
}


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

/** `gainReductionDb` for a curve whose threshold/ratio/knee are fixed for a whole block.
    The detector then feeds a LINEAR peak and the dB conversion is only performed when the
    signal is inside the knee or above it - below the knee the answer is exactly 0 dB, so the
    per-sample `log10` that `gainToDb (pk)` used to cost is skipped entirely. Above the knee the
    arithmetic is identical to `gainReductionDb`, so the output is bit-exact (prior-art report
    F2; verified by `tools/research/nullcmp.py`). */
struct DetectorCurve
{
    DetectorCurve (float threshIn, float ratioIn, float kneeIn) noexcept
        : threshDb (threshIn), ratio (ratioIn), kneeDb (kneeIn)
    {
        // `gainReductionDb` is exactly 0 only while `2 * over < -kneeDb`, i.e. below
        // `threshDb - kneeDb / 2`. That is the floor - not the knee's centre, which sits
        // kneeDb/2 ABOVE it and silently clipped the lower flank of every soft knee.
        // Linearising the comparison here turns the FLOOR into a multiply; the dB path
        // itself is still `gainToDb (peak)` inside `gainReductionDb`, untouched.
        linearFloor = threshDb > -160.0f ? dbToGain (threshDb - kneeDb * 0.5f) : 0.0f;
    }

    /** Peak gain reduction in dB for a linear peak amplitude (>= 0). */
    float reductionDb (float peak) const noexcept
    {
        if (peak <= linearFloor)
            return 0.0f;
        return gainReductionDb (gainToDb (peak), threshDb, ratio, kneeDb);
    }

private:
    float threshDb = 0.0f, ratio = 1.0f, kneeDb = 0.0f, linearFloor = 0.0f;
};

/** Transparent below 0.7, smooth saturation above. Stand-in for analog output-stage behaviour. */
inline float softClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.7f) return x;
    const float y = 0.7f + 0.3f * tableTanh ((a - 0.7f) / 0.3f);
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
    return tableTanh (shaped * 1.035f) * 0.966f;
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
    return sign * (0.55f + 0.45f * tableTanh ((a - 0.55f) / 0.45f));
}

/** Urei / UA 1176 Class-A FET gain reduction & 11754 output transformer analog color.
    Produces aggressive odd harmonics (predominantly 3rd harmonic), punchy bite and transformer iron saturation. */
inline float fetColor (float x)
{
    const float a = std::abs (x);
    if (a < 0.30f)
        return x * (1.0f - 0.06f * x * x);
    const float sign = x < 0.0f ? -1.0f : 1.0f;
    return sign * (0.30f + 0.70f * tableTanh ((a - 0.30f) / 0.70f));
}

/** Master console & tape saturation with Enhance harmonic exciter.
    Blends analog tape 3rd harmonic compression with subtle transformer warmth. */
inline float analogMasterSaturation (float x, float enh)
{
    if (enh <= 0.0f)
        return x;
    const float drive = 1.0f + enh * 0.45f;
    const float driven = x * drive;
    const float tape = tableTanh (driven);
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
        colourMode = apvts.getRawParameterValue ("colour_mode");
        const int model = prefix == "con_" ? 0 : prefix == "opto_" ? 1 : prefix == "fet_" ? 2 : 3;
        colour.prepare (sr, model, (int) colourMode->load());
        reset();
    }

    /** Return the module's internal DSP state to its power-on condition.
        Never touches parameters; called from prepare() and from the audio thread after
        the operator's RESET request, so it must not allocate on the audio thread. */
    virtual void reset() {}

    virtual int  getLatencySamples() const { return colour.latency(); }

    void run (juce::AudioBuffer<float>& buf)
    {
        if (bypassParameter->load (std::memory_order_relaxed) > 0.5f)
        {
            processBypassed (buf);
            grDb.store (0.0f, std::memory_order_relaxed);
            outPeak.store (0.0f, std::memory_order_relaxed);
        }
        else process (buf);
    }

    // The processor manages level matching after the complete reorderable rack.
    // Standalone module users retain the local input/output matcher.
    virtual void setRackAutoGainManaged (bool) {}
    virtual void applyRackAutoGain (juce::AudioBuffer<float>&, const float*, int) {}

    std::atomic<float> grDb { 0.0f };   // gain reduction for the UI meter
    std::atomic<float> gainCompDb { 0.0f }; // Actual applied Enhance Auto Gain correction
    std::atomic<bool> autoGainLimited { false };
    std::atomic<float> outPeak { 0.0f }; // Output peak level for UI meters

protected:
    virtual void process (juce::AudioBuffer<float>&) = 0;
    virtual void processBypassed (juce::AudioBuffer<float>& buf) {
        const int channels = std::min(2, buf.getNumChannels());
        for(int i=0;i<buf.getNumSamples();++i) {
            float raw[2]{},out[2]{};
            for(int c=0;c<channels;++c) raw[c]=buf.getSample(c,i);
            colour.frame(raw,raw,out,channels,2);
            for(int c=0;c<channels;++c) buf.setSample(c,i,out[c]);
        }
    }
    void colourFrame(float* raw, float* algo, float* out, int channels, float amount=1.0f, bool bypass=false, float namDrive=1.0f) {
        colour.frame(raw,algo,out,channels,bypass?2:(int)colourMode->load(std::memory_order_relaxed),amount,namDrive);
    }
    ColourEngine colour;
    std::atomic<float>* colourMode=nullptr;

    /** Resolve a parameter ONCE, in the constructor, and only ever `load()` the atomic
        afterwards. The previous string-keyed lookup built a juce::String and hashed it on
        the audio thread on every call (report `docs/research/dsp-optimization-prior-art.md`,
        F4). Never call this from process(); it exists for constructors only. */
    std::atomic<float>* parameter (const char* name) const
    {
        auto* p = apvts.getRawParameterValue (prefix + name);
        jassert (p != nullptr);
        return p;
    }

    static float read (const std::atomic<float>* p) noexcept
    {
        return p->load (std::memory_order_relaxed);
    }

    /** Constructor-time resolution of the shared "bypass" switch every module has. */
    void prepareBypassParameter() { bypassParameter = parameter ("bypass"); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String prefix;
    double sampleRate = 44100.0;
    int maxBlockSize = 0;

private:
    std::atomic<float>* bypassParameter = nullptr;
};

//==============================================================================
/** Pure ten-band EQ560. Retired console dynamics controls remain registered for host compatibility. */
class ConsoleModule : public Module
{
public:
    ConsoleModule (juce::AudioProcessorValueTreeState& state, juce::String paramPrefix)
        : Module (state, std::move (paramPrefix))
    {
        prepareBypassParameter();
        for (size_t i = 0; i < bandParameters.size(); ++i)
            bandParameters[i] = apvts.getRawParameterValue (prefix + Eq560::bandIds[i]);
        threshParam = parameter ("thresh");
        ratioParam  = parameter ("ratio");
        attackParam = parameter ("attack");
        releaseParam = parameter ("release");
        outParam    = parameter ("out");
    }

    void reset() override
    {
        colour.reset();
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

        const float thresh = read (threshParam);
        const float ratio  = read (ratioParam);
        const float atkMs  = read (attackParam);
        const float relMs  = read (releaseParam);
        const float outG   = dbToGain (read (outParam));
        const float aC = timeCoef (atkMs, sampleRate);
        const float rC = timeCoef (relMs, sampleRate);
        float maxGr = 0.0f;

        // The detector curve is fixed for the whole block here, so the threshold is
        // pre-transformed to linear amplitude once instead of per sample (F2).
        const DetectorCurve curve (thresh, ratio, 2.0f);
        float* const* data = buf.getArrayOfWritePointers();

        for (int i = 0; i < n; ++i)
        {
            eq.beginSample();
            float pk = 0.0f;
            float raw[2]{}, shaped[2]{}, coloured[2]{};
            for (int ch = 0; ch < nch; ++ch) {
                raw[ch] = eq.processSample (ch, data[ch][i]);
                shaped[ch] = apiConsoleColor (raw[ch]);
            }
            colourFrame(raw,shaped,coloured,nch);
            for (int ch = 0; ch < nch; ++ch) {
                data[ch][i]=coloured[ch];
                pk = std::max(pk,std::abs(coloured[ch]));
            }
            env = smooth (env, curve.reductionDb (pk), aC, rC);
            maxGr = std::max (maxGr, env);
            const float g = dbToGainFast (-env) * outG;
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
    std::atomic<float>* threshParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* outParam = nullptr;
    float env = 0.0f;
};

//==============================================================================
/** Optical-style leveller: program-dependent two-stage release, soft knee, compress/limit switch. */
class OptoModule : public Module
{
public:
    OptoModule (juce::AudioProcessorValueTreeState& state, juce::String paramPrefix)
        : Module (state, std::move (paramPrefix))
    {
        prepareBypassParameter();
        peakParam = parameter ("peak");
        gainParam = parameter ("gain");
        modeParam = parameter ("mode");
    }

    void reset() override
    {
        colour.reset();
        fast = slow = 0.0f;
        grDb.store (0.0f);
        gainCompDb.store (0.0f);
        outPeak.store (0.0f);
    }

protected:
    void process (juce::AudioBuffer<float>& buf) override
    {
        const int n = buf.getNumSamples(), nch = buf.getNumChannels();
        const float pr = read (peakParam) * 0.01f;
        const float makeup = dbToGain (read (gainParam) * 0.01f * 30.0f);
        const bool limit = read (modeParam) > 0.5f;
        const float thresh = -pr * 40.0f;
        const float ratio = limit ? 12.0f : 3.0f;

        const float aC = timeCoef (10.0f, sampleRate);
        const float fC = timeCoef (60.0f, sampleRate);
        const float sC = timeCoef (1800.0f, sampleRate);
        float maxGr = 0.0f;

        const DetectorCurve curve (thresh, ratio, 10.0f);
        float* const* data = buf.getArrayOfWritePointers();

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            for (int ch = 0; ch < nch; ++ch) pk = std::max (pk, std::abs (data[ch][i]));

            const float gr = curve.reductionDb (pk);
            fast = smooth (fast, gr, aC, fC);
            slow = smooth (slow, gr * 0.6f, aC, sC);
            const float applied = std::max (fast, slow);
            maxGr = std::max (maxGr, applied);

            const float g = dbToGainFast (-applied) * makeup;
            float raw[2]{}, shaped[2]{}, coloured[2]{};
            for (int ch=0;ch<nch;++ch) { raw[ch]=data[ch][i]*g; shaped[ch]=optoTubeColor(raw[ch]); }
            colourFrame(raw,shaped,coloured,nch);
            for (int ch=0;ch<nch;++ch) data[ch][i]=coloured[ch];
        }
        grDb.store (maxGr);
    }

private:
    std::atomic<float>* peakParam = nullptr;
    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    float fast = 0.0f, slow = 0.0f;
};

//==============================================================================
/** FET-style compressor: input drives a fixed threshold, very fast attack, selectable ratio. */
class FetModule : public Module
{
public:
    FetModule (juce::AudioProcessorValueTreeState& state, juce::String paramPrefix)
        : Module (state, std::move (paramPrefix))
    {
        prepareBypassParameter();
        inputParam = parameter ("input");
        outputParam = parameter ("output");
        attackParam = parameter ("attack");
        releaseParam = parameter ("release");
        ratioParam = parameter ("ratio");
    }

    void reset() override
    {
        colour.reset();
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
        const float inG  = dbToGain (read (inputParam));
        const float outG = dbToGain (read (outputParam));
        const float atkMs = juce::jmap (read (attackParam),  1.0f, 7.0f, 0.8f, 0.02f);
        const float relMs = juce::jmap (read (releaseParam), 1.0f, 7.0f, 1100.0f, 50.0f);
        const float ratio = ratios[juce::jlimit (0, 4, (int) read (ratioParam))];
        const float aC = timeCoef (atkMs, sampleRate), rC = timeCoef (relMs, sampleRate);
        float maxGr = 0.0f;

        const DetectorCurve curve (-10.0f, ratio, 2.0f);
        float* const* data = buf.getArrayOfWritePointers();

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
            {
                data[ch][i] *= inG;
                pk = std::max (pk, std::abs (data[ch][i]));
            }
            env = smooth (env, curve.reductionDb (pk), aC, rC);
            maxGr = std::max (maxGr, env);
            const float g = dbToGainFast (-env) * outG;
            float raw[2]{}, shaped[2]{}, coloured[2]{};
            for (int ch=0;ch<nch;++ch) { raw[ch]=data[ch][i]*g; shaped[ch]=fetColor(raw[ch]); }
            colourFrame(raw,shaped,coloured,nch);
            for (int ch=0;ch<nch;++ch) data[ch][i]=coloured[ch];
        }
        grDb.store (maxGr);
    }

private:
    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* ratioParam = nullptr;
    float env = 0.0f;
};

//==============================================================================
/** Look-ahead brick-wall limiter (sliding-min + moving-average) with a drive/saturation control. */
class LimiterModule : public Module
{
public:
    struct SlidingMinimum
    {
        void prepare (int window)
        {
            values.assign ((size_t) window, 1.0f);
            indices.assign ((size_t) window, 0);
            slots.assign ((size_t) window, 0);
            capacity = window;
            reset();
        }

        void reset()
        {
            std::fill (values.begin(), values.end(), 1.0f);
            head = count = 0;
            sequence = 0;
            for (int i = 0; i < capacity; ++i)
            {
                const uint64_t id = (uint64_t) i;
                indices[(size_t) count] = id;
                slots[(size_t) count] = i;
                ++count;
            }
        }

        float push (float value)
        {
            while (count > 0 && sequence - indices[(size_t) head] >= (uint64_t) capacity)
            {
                head = (head + 1) % capacity;
                --count;
            }

            const int slot = (int) (sequence % (uint64_t) capacity);
            values[(size_t) slot] = value;
            while (count > 0)
            {
                const int back = (head + count - 1) % capacity;
                const int backSlot = slots[(size_t) back];
                if (values[(size_t) backSlot] < value) break;
                --count;
            }
            if (count == 0)
            {
                head = (head + 1) % capacity;
                count = 0;
            }
            const int tail = (head + count) % capacity;
            indices[(size_t) tail] = sequence;
            slots[(size_t) tail] = slot;
            ++count;

            const int minSlot = slots[(size_t) head];
            const float result = values[(size_t) minSlot];
            ++sequence;
            return result;
        }

        std::vector<float> values;
        std::vector<uint64_t> indices;
        std::vector<int> slots;
        int capacity = 0, head = 0, count = 0;
        uint64_t sequence = 0;
    };

    LimiterModule (juce::AudioProcessorValueTreeState& state, juce::String paramPrefix)
        : Module (state, std::move (paramPrefix)),
          autoGainParameter (apvts.getRawParameterValue (prefix + "auto_gain"))
    {
        prepareBypassParameter();
        parameterBypass = parameter ("bypass");
        enhanceParam = parameter ("enhance");
        gainParam    = parameter ("gain");
        ceilingParam = parameter ("ceiling");
        releaseParam = parameter ("release");
    }

    void reset() override
    {
        colour.reset();
        // Ring sizes follow the sample rate; resize only when the rate actually changed so a
        // reset from the audio thread cannot allocate in the common case.
        const int wanted = std::max (2, (int) std::ceil (0.0015 * sampleRate));
        la = wanted;
        for (auto& d : delay) if ((int) d.size() != la) d.assign ((size_t) la, 0.0f); else std::fill (d.begin(), d.end(), 0.0f);
        for (auto& d : dryDelay) if ((int) d.size() != la) d.assign ((size_t) la, 0.0f); else std::fill (d.begin(), d.end(), 0.0f);
        if ((int) reqRing.size() != la) reqRing.assign ((size_t) la, 1.0f); else std::fill (reqRing.begin(), reqRing.end(), 1.0f);
        if (slidingMin.capacity != la) slidingMin.prepare (la); else slidingMin.reset();
        if ((int) avgRing.size() != la) avgRing.assign ((size_t) la, 1.0f); else std::fill (avgRing.begin(), avgRing.end(), 1.0f);
        sum = (double) la; wp = 0; relGain = 1.0f;
        dryEnergy = wetEnergy = 0.0; compensationDb = 0.0f;
        energyCoef = std::exp (-1.0 / (0.2 * sampleRate));
        compensationCoef = timeCoef (100.0f, sampleRate);
        for (auto* smoother : { &inputGain, &colourDrive, &enhanceAmount, &ceilingGain, &releaseCoef, &autoGainCeiling })
            smoother->reset (sampleRate, 0.02);
        inputGain.setCurrentAndTargetValue (dbToGain (read (gainParam)));
        enhanceAmount.setCurrentAndTargetValue (read (enhanceParam) * 0.01f);
        colourDrive.setCurrentAndTargetValue (dbToGain (read (enhanceParam) * 0.12f));
        ceilingGain.setCurrentAndTargetValue (dbToGain (read (ceilingParam)));
        autoGainCeiling.setCurrentAndTargetValue (dbToGain (read (ceilingParam)));
        releaseCoef.setCurrentAndTargetValue (timeCoef (read (releaseParam), sampleRate));
        const int referenceSize = colour.latency() + 1;
        for (auto& d : colourDryDelay)
            if ((int) d.size() != referenceSize) d.assign ((size_t) referenceSize, 0.0f);
            else std::fill (d.begin(), d.end(), 0.0f);
        colourDryWp = 0;
        grDb.store (0.0f);
        gainCompDb.store (0.0f);
        autoGainLimited.store (false);
        outPeak.store (0.0f);
    }

    int getLatencySamples() const override { return la - 1 + colour.latency(); }

    void setRackAutoGainManaged (bool managed) override { rackAutoGainManaged = managed; }

    void applyRackAutoGain (juce::AudioBuffer<float>& buffer, const float* inputEnergy, int count) override
    {
        if (! rackAutoGainManaged) return;
        const bool active = read (parameterBypass) < 0.5f;
        const bool enabled = active && autoGainParameter->load() > 0.5f;
        const float requestedCeiling = dbToGain (read (ceilingParam));
        autoGainCeiling.setTargetValue (requestedCeiling);
        const int channels = std::min (2, buffer.getNumChannels());
        double rawEnergy = 0, correctedEnergy = 0;
        float peak = 0;
        bool limited = false;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float wet[2] {};
            double power = 0;
            for (int c = 0; c < channels; ++c)
            {
                wet[c] = buffer.getSample (c, i);
                power += (double) wet[c] * wet[c];
            }
            power /= std::max (1, channels);
            const double reference = inputEnergy != nullptr && i < count ? inputEnergy[i] : power;
            const float ceiling = std::min (requestedCeiling, autoGainCeiling.getNextValue());
            applyCompensation (wet, channels, reference, power, active, enabled, ceiling, limited);
            rawEnergy += power;
            for (int c = 0; c < channels; ++c)
            {
                buffer.setSample (c, i, wet[c]);
                correctedEnergy += (double) wet[c] * wet[c] / std::max (1, channels);
                peak = std::max (peak, std::abs (wet[c]));
            }
        }
        gainCompDb.store (active && rawEnergy > 1.e-16 && correctedEnergy > 1.e-16
            ? (float) (10 * std::log10 (correctedEnergy / rawEnergy)) : 0.f);
        autoGainLimited.store (limited);
        if (enabled || std::abs (compensationDb) > 1.e-5f) outPeak.store (peak);
    }

protected:
    void process (juce::AudioBuffer<float>& buf) override         { runLimiter (buf, true); }
    void processBypassed (juce::AudioBuffer<float>& buf) override { runLimiter (buf, false); }

private:
    void applyCompensation (float* wet, int channels, double reference, double output,
                            bool active, bool enabled, float ceiling, bool& limited)
    {
        // Both powers are measured BEFORE this correction, so this is a feed-forward
        // ratio. Feeding corrected output back here halves the required dB correction.
        dryEnergy = energyCoef * dryEnergy + (1.0 - energyCoef) * reference;
        wetEnergy = energyCoef * wetEnergy + (1.0 - energyCoef) * output;
        float targetDb = 0;
        if (enabled && dryEnergy > 1.e-12 && wetEnergy > 1.e-16)
            targetDb = juce::jlimit (-36.f, 36.f, (float) (10 * std::log10 (dryEnergy / wetEnergy)));
        compensationDb = compensationCoef * compensationDb + (1.f - compensationCoef) * targetDb;
        if (! active) { compensationDb = 0; dryEnergy = wetEnergy = 0; }
        const float correction = active ? dbToGainFast (compensationDb) : 1.f;
        float peak = 0;
        for (int c = 0; c < channels; ++c) peak = std::max (peak, std::abs (wet[c] * correction));
        const bool enforceCeiling = active && (enabled || std::abs (compensationDb) > 1.e-5f);
        const float safety = enforceCeiling && peak > ceiling ? ceiling / peak : 1.f;
        limited |= enabled && safety < 0.9999f;
        for (int c = 0; c < channels; ++c) wet[c] *= correction * safety;
    }

    void runLimiter (juce::AudioBuffer<float>& buf, bool active)
    {
        const int n = buf.getNumSamples(), nch = std::min (2, buf.getNumChannels());
        inputGain.setTargetValue (dbToGain (read (gainParam)));
        enhanceAmount.setTargetValue (read (enhanceParam) * 0.01f);
        colourDrive.setTargetValue (dbToGain (read (enhanceParam) * 0.12f));
        const float safetyCeiling = dbToGain (read (ceilingParam));
        ceilingGain.setTargetValue (safetyCeiling);
        releaseCoef.setTargetValue (timeCoef (read (releaseParam), sampleRate));
        const bool autoGain = active && autoGainParameter->load() > 0.5f;
        float minG = 1.0f;
        float blockOutPeak = 0.0f;
        double blockRawEnergy = 0, blockCorrectedEnergy = 0;
        bool limited = false;
        float* const* data = buf.getArrayOfWritePointers();

        for (int i = 0; i < n; ++i)
        {
            const float gain = inputGain.getNextValue();
            const float colourGain = colourDrive.getNextValue();
            const float enh = enhanceAmount.getNextValue();
            const float ceil = ceilingGain.getNextValue();
            const float rC = releaseCoef.getNextValue();
            const float manualGain = active ? gain : 1.0f;
            const float drive = active ? gain * colourGain : 1.0f;
            float pk = 0.0f;
            float in[2] { 0.0f, 0.0f };
            float raw[2]{}, shaped[2]{};
            for (int ch=0;ch<nch;++ch) {
                const float dry=data[ch][i];
                colourDryDelay[ch][(size_t) colourDryWp] = dry;

                // CLEAN disables Enhance's colour drive; limiter gain remains active.
                raw[ch]=dry*manualGain;
                const float driven=active?dry*drive:dry;
                shaped[ch]=active?analogMasterSaturation(driven,enh):raw[ch];
            }
            // NAM supplies the tape's level-dependent residual, not its complete
            // EQ/phase response. Add a bounded amount to dry; actual input gain
            // still drives the capture. ALGO retains its own saturation law.
            colourFrame(raw,shaped,in,nch,enh * 0.30f,!active);
            for (int ch=0;ch<nch;++ch) {
                pk=std::max(pk,std::abs(in[ch]));
                // Delay the ungained input itself: dividing delayed audio by the
                // current moving gain corrupts Auto Gain's automation reference.
                dryDelay[ch][(size_t)wp] = colourDryDelay[ch][(size_t)
                    ((colourDryWp + 1) % (int) colourDryDelay[ch].size())];
            }

            colourDryWp = (colourDryWp + 1) % (int) colourDryDelay[0].size();
            const float req = (active && pk > ceil) ? ceil / pk : 1.0f;
            reqRing[(size_t) wp] = req;
            const float mn = slidingMin.push (req);

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
            // The module's own ceiling remains a hard bound even when rack
            // Auto Gain is off or delegated to the processor's final rack tap.
            float corePeak = 0;
            for (int ch = 0; ch < nch; ++ch) corePeak = std::max (corePeak, std::abs (wet[ch]));
            const float coreCeiling = std::min (safetyCeiling, ceil);
            const float coreSafety = active && corePeak > coreCeiling ? coreCeiling / corePeak : 1.f;
            wetPower = 0;
            for (int ch = 0; ch < nch; ++ch)
            {
                wet[ch] *= coreSafety;
                wetPower += (double) wet[ch] * wet[ch];
            }
            if (! rackAutoGainManaged)
                applyCompensation (wet, nch, dryPower / std::max (1, nch), wetPower / std::max (1, nch),
                                   active, autoGain, std::min (safetyCeiling, ceil), limited);
            blockRawEnergy += wetPower;
            for (int ch = 0; ch < nch; ++ch)
            {
                data[ch][i] = wet[ch];
                blockCorrectedEnergy += (double) wet[ch] * wet[ch];
                blockOutPeak = std::max (blockOutPeak, std::abs (wet[ch]));
            }
            wp = (wp + 1) % la;
        }
        grDb.store (active ? -gainToDb (minG) : 0.0f);
        if (! rackAutoGainManaged)
        {
            gainCompDb.store (active && blockRawEnergy > 1.e-16 && blockCorrectedEnergy > 1.e-16
                ? (float) (10 * std::log10 (blockCorrectedEnergy / blockRawEnergy)) : 0.f);
            autoGainLimited.store (limited);
        }
        outPeak.store (active ? blockOutPeak : 0.0f);
    }

    std::atomic<float>* autoGainParameter;
    std::atomic<float>* enhanceParam = nullptr;
    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* ceilingParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    bool rackAutoGainManaged = false;
    std::atomic<float>* parameterBypass = nullptr;
    std::vector<float> delay[2], dryDelay[2], reqRing, avgRing;
    std::vector<float> colourDryDelay[2];
    int colourDryWp = 0;
    juce::SmoothedValue<float> inputGain, colourDrive, enhanceAmount, ceilingGain, releaseCoef, autoGainCeiling;
    SlidingMinimum slidingMin;
    int la = 64, wp = 0;
    double sum = 64.0;
    float relGain = 1.0f;
    double dryEnergy = 0.0, wetEnergy = 0.0, energyCoef = 0.0;
    float compensationDb = 0.0f, compensationCoef = 0.0f;
};

} // namespace rack
