#pragma once
// Minimal JUCE stand-in for the headless DSP research harness.
// It implements ONLY the surface Source/Modules.h touches, so the harness can
// #include the real module code instead of re-implementing the DSP.
// Not part of the plugin build; see tools/research/README.md.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace juce
{

//==============================================================================
class String
{
public:
    String() = default;
    String (const char* s) : text (s ? s : "") {}
    String (std::string s) : text (std::move (s)) {}

    String operator+ (const String& other) const { return String (text + other.text); }
    String operator+ (const char* other) const { return String (text + (other ? other : "")); }

    const std::string& toStdString() const { return text; }

private:
    std::string text;
};

inline String operator+ (const char* a, const String& b)
{
    return String (std::string (a ? a : "") + b.toStdString());
}

//==============================================================================
template <typename Type>
class AudioBuffer
{
public:
    AudioBuffer() = default;
    AudioBuffer (int numChannels, int numSamples) { setSize (numChannels, numSamples); }

    void setSize (int numChannels, int numSamples, bool = false, bool = false, bool = false)
    {
        chans.assign ((size_t) std::max (1, numChannels), std::vector<Type> ((size_t) std::max (0, numSamples), Type (0)));
    }

    int getNumChannels() const { return (int) chans.size(); }
    int getNumSamples() const { return chans.empty() ? 0 : (int) chans[0].size(); }

    Type getSample (int channel, int index) const { return chans[(size_t) channel][(size_t) index]; }
    void setSample (int channel, int index, Type value) { chans[(size_t) channel][(size_t) index] = value; }

    Type* getWritePointer (int channel) { return chans[(size_t) channel].data(); }
    const Type* getReadPointer (int channel) const { return chans[(size_t) channel].data(); }

    void clear (int channel, int start, int num)
    {
        auto& c = chans[(size_t) channel];
        std::fill (c.begin() + start, c.begin() + start + num, Type (0));
    }

    void copyFrom (int destCh, int destStart, const AudioBuffer& src, int srcCh, int srcStart, int num)
    {
        std::copy_n (src.chans[(size_t) srcCh].begin() + srcStart, num, chans[(size_t) destCh].begin() + destStart);
    }

    void addFrom (int destCh, int destStart, const AudioBuffer& src, int srcCh, int srcStart, int num, Type gain = Type (1))
    {
        for (int i = 0; i < num; ++i)
            chans[(size_t) destCh][(size_t) (destStart + i)] += src.chans[(size_t) srcCh][(size_t) (srcStart + i)] * gain;
    }

    void applyGain (int channel, int start, int num, Type gain)
    {
        for (int i = 0; i < num; ++i)
            chans[(size_t) channel][(size_t) (start + i)] *= gain;
    }

private:
    std::vector<std::vector<Type>> chans;
};

//==============================================================================
class AudioProcessorValueTreeState
{
public:
    AudioProcessorValueTreeState() = default;

    std::atomic<float>* getRawParameterValue (const String& id)
    {
        auto key = id.toStdString();
        auto it = params.find (key);
        if (it == params.end())
            it = params.emplace (key, std::make_unique<std::atomic<float>> (0.0f)).first;
        return it->second.get();
    }

    void set (const std::string& id, float value) { getRawParameterValue (String (id))->store (value); }

private:
    std::map<std::string, std::unique_ptr<std::atomic<float>>> params;
};

//==============================================================================
template <typename Type>
Type jlimit (Type lower, Type upper, Type value)
{
    return value < lower ? lower : (value > upper ? upper : value);
}

template <typename Type>
Type jmap (Type value, Type sourceMin, Type sourceMax, Type targetMin, Type targetMax)
{
    if (sourceMax == sourceMin) return targetMin;
    const Type proportion = (value - sourceMin) / (sourceMax - sourceMin);
    return targetMin + proportion * (targetMax - targetMin);
}

} // namespace juce
