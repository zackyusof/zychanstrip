#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ZychanStripProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addF = [&] (const juce::String& id, const juce::String& name, float lo, float hi, float def,
                     const juce::String& unit = {}, float skew = 1.0f)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.0f, skew), def,
            juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); })));
    };
    auto addB = [&] (const juce::String& id, const juce::String& name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    // Console strip
    addB ("con_bypass", "Console Bypass", false);
    addF ("con_hpf",    "Console HPF",        20.0f,  300.0f, 20.0f,  "Hz", 0.4f);
    addF ("con_lf",     "Console LF Gain",   -15.0f,   15.0f,  0.0f,  "dB");
    addF ("con_lmf_f",  "Console LMF Freq",  200.0f, 2500.0f, 600.0f, "Hz", 0.5f);
    addF ("con_lmf",    "Console LMF Gain",  -15.0f,   15.0f,  0.0f,  "dB");
    addF ("con_hmf_f",  "Console HMF Freq", 1500.0f, 8000.0f, 3000.0f, "Hz", 0.5f);
    addF ("con_hmf",    "Console HMF Gain",  -15.0f,   15.0f,  0.0f,  "dB");
    addF ("con_hf",     "Console HF Gain",   -15.0f,   15.0f,  0.0f,  "dB");
    addF ("con_thresh", "Console Threshold",  -40.0f,    0.0f,  0.0f,  "dB");
    addF ("con_ratio",  "Console Ratio",        1.5f,   10.0f,  3.0f,  ":1");
    addF ("con_attack", "Console Attack",       0.3f,   30.0f, 10.0f,  "ms", 0.5f);
    addF ("con_release","Console Release",     50.0f, 1200.0f, 200.0f, "ms", 0.5f);
    addF ("con_out",    "Console Output",     -12.0f,   12.0f,  0.0f,  "dB");

    // Opto leveller
    addB ("opto_bypass", "Opto Bypass", false);
    addF ("opto_peak",   "Opto Peak Reduction", 0.0f, 100.0f, 0.0f);
    addF ("opto_gain",   "Opto Gain",           0.0f, 100.0f, 0.0f);
    addB ("opto_mode",   "Opto Limit Mode", false);

    // FET compressor
    addB ("fet_bypass", "FET Bypass", false);
    addF ("fet_input",  "FET Input",   0.0f, 30.0f, 0.0f, "dB");
    addF ("fet_output", "FET Output", -20.0f, 20.0f, 0.0f, "dB");
    addF ("fet_attack", "FET Attack",   1.0f,  7.0f, 3.0f);
    addF ("fet_release","FET Release",  1.0f,  7.0f, 4.0f);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "fet_ratio", 1 }, "FET Ratio", juce::StringArray { "4", "8", "12", "20", "30" }, 0));

    // Limiter
    addB ("lim_bypass",  "Limiter Bypass", false);
    addF ("lim_gain",    "Limiter Gain",    0.0f,  18.0f,  0.0f, "dB");
    addF ("lim_ceiling", "Limiter Ceiling", -12.0f, 0.0f, -0.3f, "dB");
    addF ("lim_release", "Limiter Release", 10.0f, 1000.0f, 100.0f, "ms", 0.4f);
    addF ("lim_enhance", "Limiter Enhance", 0.0f, 100.0f, 0.0f);

    // Append new controls so existing host parameter indices remain stable.
    addF ("master_level", "Master Level", -60.0f, 6.0f, 0.0f, "dB");
    // Parallel processing: blends the processed master output against the untouched input.
    // Defaults to 100% wet so existing sessions sound exactly as before.
    addF ("master_mix", "Dry/Wet Mix", 0.0f, 100.0f, 100.0f, "%");

    // New EQ560 controls follow all legacy parameters, including the master.
    for (size_t i = 0; i < rack::Eq560::bandIds.size(); ++i)
        addF (juce::String ("con_") + rack::Eq560::bandIds[i],
              "EQ560 " + juce::String (rack::Eq560::frequencies[i], 0) + " Hz", -12.0f, 12.0f, 0.0f, "dB");
    addB ("lim_auto_gain", "Enhance Auto Gain", true);
    // Append-only: the new automation ID must never shift any existing host parameter index.
    addB ("unity", "UNITY Level Match", false);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "colour_mode", 1 }, "Colour Engine",
        juce::StringArray { "ALGO", "NAM", "CLEAN" }, 0));
    // Final-stage 2x oversampling antialiasing filter. Appended last so existing host
    // parameter indices stay stable. Default off to preserve the cheap ALGO/CLEAN path.
    // Runs after master/mix/UNITY, so it antialiases the completed output without
    // disturbing the dry/wet blend. Latency is included in the reported plugin latency.
    addB ("final_oversampling", "Final 2x Oversampling", false);
    return layout;
}

//==============================================================================
ZychanStripProcessor::ZychanStripProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    modules[Console] = std::make_unique<rack::ConsoleModule> (apvts, "con_");
    modules[Opto]    = std::make_unique<rack::OptoModule>    (apvts, "opto_");
    modules[Fet]     = std::make_unique<rack::FetModule>     (apvts, "fet_");
    modules[Limiter] = std::make_unique<rack::LimiterModule> (apvts, "lim_");
    modules[Limiter]->setRackAutoGainManaged (true);
    masterLevel = apvts.getRawParameterValue ("master_level");
    mixAmount   = apvts.getRawParameterValue ("master_mix");
    unityEnabled = apvts.getRawParameterValue ("unity");
    finalOversampling = apvts.getRawParameterValue ("final_oversampling");
}

bool ZychanStripProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == l.getMainInputChannelSet();
}

void ZychanStripProcessor::prepareToPlay (double sr, int block)
{
    int moduleLatency = 0;
    for (auto& m : modules)
    {
        m->prepare (sr, block);
        moduleLatency += m->getLatencySamples();
    }
    // Final-stage oversampling adds fixed latency; include it in the reported plugin
    // latency so host PDC stays constant whether the oversampler is on or off.
    finalOversampler.initProcessing (block);
    finalOversamplingLatency = (int) finalOversampler.getLatencyInSamples();
    setLatencySamples (moduleLatency + finalOversamplingLatency);
    dryBuffer.setSize (2, juce::jmax (1, block), false, false, true);
    // Dry-path delay matches module latency only; oversampling sits after the mix so
    // it applies equally to the summed wet+dry output (no dry/wet misalignment).
    dryDelayLen = moduleLatency > 0 ? (int) moduleLatency + 1 : 0;
    for (auto& d : dryDelay) d.assign ((size_t) juce::jmax (1, dryDelayLen), 0.0f);
    dryWp = 0;
    chainInputRef.assign ((size_t) juce::jmax (1, block), 0.0f);
    mixGain.reset (sr, 0.02);
    mixGain.setCurrentAndTargetValue (mixAmount->load() * 0.01f);
    masterGain.reset (sr, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterLevel->load()));
    unityGain.reset (sr, 0.05);
    unityGain.setCurrentAndTargetValue (1.0f);
    unityInputEnergy = unityOutputEnergy = 0.0;
    unityEnergyCoef = std::exp (-1.0 / (0.25 * sr));
    unityGainDb.store (0.0f, std::memory_order_relaxed);
    meters.prepare (sr, block);
}

void ZychanStripProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // Operator RESET: clear every module's internal state on the audio thread, then snap the
    // master smoother instead of gliding to the restored default.
    if (resetRequested.exchange (false))
    {
        for (auto& m : modules) m->reset();
        finalOversampler.reset();
        for (auto& delay : dryDelay) std::fill (delay.begin(), delay.end(), 0.f);
        dryWp = 0;
        mixGain.setCurrentAndTargetValue (mixAmount->load() * 0.01f);
        masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterLevel->load()));
        unityGain.setCurrentAndTargetValue (1.0f);
        unityInputEnergy = unityOutputEnergy = 0.0;
    }

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // Keep the untouched input for the parallel (dry/wet) blend and for UNITY. Prepared in
    // prepareToPlay; grown here only if the host sends a larger block than it declared.
    if (dryBuffer.getNumChannels() < buffer.getNumChannels() || dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize (juce::jmax (buffer.getNumChannels(), dryBuffer.getNumChannels()),
                           juce::jmax (numSamples, dryBuffer.getNumSamples()), false, false, true);

    const float requestedWet = juce::jlimit (0.0f, 1.0f, mixAmount->load() * 0.01f);
    const bool matchUnity = unityEnabled != nullptr && unityEnabled->load (std::memory_order_relaxed) > 0.5f;
    mixGain.setTargetValue (requestedWet);
    // Always advance the dry reference, including 0/100% wet. Freezing its ring
    // at either endpoint leaves stale audio when returning to a parallel blend.
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        dryBuffer.copyFrom (c, 0, buffer, c, 0, numSamples);
    if (dryDelayLen > 0)
        for (int i = 0; i < numSamples; ++i)
        {
            const int read = (dryWp + 1) % dryDelayLen;
            for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
            {
                auto& delay = dryDelay[(size_t) c];
                delay[(size_t) dryWp] = dryBuffer.getSample (c, i);
                dryBuffer.setSample (c, i, delay[(size_t) read]);
            }
            dryWp = read;
        }
    // Both energy references now describe the same delayed input time interval.
    feedChainReference (dryBuffer, numSamples);

    for (int id : getOrder())
        modules[(size_t) id]->run (buffer);

    // Match the complete rack against its untouched input, before master/mix.
    // The rack output is still uncorrected here: no previous-block feedback and
    // no gain applied ahead of downstream nonlinear modules when order changes.
    modules[Limiter]->applyRackAutoGain (buffer, chainInputRef.data(), numSamples);

    // The master always follows the complete rack, independently of module order.
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterLevel->load()));
    masterGain.applyGain (buffer, numSamples);

    // Keep the host-reported delay at EVERY mix value, including fully dry.
    // Zero/100% endpoints are exact selections; intermediate targets ramp 20 ms.
    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = mixGain.getNextValue();
        if (wet == 1.f) continue;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            const float dry = dryBuffer.getSample (c, i);
            buffer.setSample (c, i, wet == 0.f ? dry
                : dry + wet * (buffer.getSample (c, i) - dry));
        }
    }

    // Optional final stereo-linked loudness match. It measures the untouched input against
    // the completed post-master/post-mix output, holds through silence, and ramps gain smoothly.
    if (! matchUnity)
    {
        unityGain.setTargetValue (1.0f);
        unityInputEnergy = unityOutputEnergy = 0.0;
        unityGainDb.store (0.0f, std::memory_order_relaxed);
    }
    else if (numSamples == 0)
        unityGainDb.store (0.0f, std::memory_order_relaxed);
    else if (unityInputEnergy <= 1.0e-12 || unityOutputEnergy <= 1.0e-12)
        unityGain.setTargetValue (1.0f);
    const int matchChannels = juce::jmin (2, buffer.getNumChannels());
    bool blockHasSignal = false;
    const int unityStep = matchUnity ? numSamples - 1 : numSamples;
    for (int i = 0; i < numSamples; ++i)
    {
        if (matchUnity && i < unityStep)
        {
            double inPower = 0.0, outPower = 0.0;
            for (int ch = 0; ch < matchChannels; ++ch)
            {
                const double x = dryBuffer.getSample (ch, i);
                const double y = buffer.getSample (ch, i);
                inPower += x * x;
                outPower += y * y;
            }
            const double channelScale = matchChannels > 0 ? 1.0 / matchChannels : 1.0;
            if (inPower <= 1.0e-12)
                unityInputEnergy = unityOutputEnergy = 0.0;
            else
            {
                unityInputEnergy = unityEnergyCoef * unityInputEnergy + (1.0 - unityEnergyCoef) * inPower * channelScale;
                unityOutputEnergy = unityEnergyCoef * unityOutputEnergy + (1.0 - unityEnergyCoef) * outPower * channelScale;
            }
            blockHasSignal |= inPower > 1.0e-12 && outPower > 1.0e-12;
            // The energy ratio only changes once per block, so the log10 and the gain curve are
            // evaluated once here rather than per sample (it used to call decibelsToGain 48 000
            // times a second for a value that moved once every 256 samples).
            if (unityInputEnergy > 1.0e-12 && unityOutputEnergy > 1.0e-12)
            {
                const float targetDb = juce::jlimit (-18.0f, 18.0f,
                    (float) (10.0 * std::log10 (unityInputEnergy / unityOutputEnergy)));
                unityGain.setTargetValue (juce::Decibels::decibelsToGain (targetDb));
            }
            else if (unityInputEnergy <= 1.0e-12 || unityOutputEnergy <= 1.0e-12)
                unityGain.setTargetValue (1.0f);
        }
        const float g = unityGain.getNextValue();
        for (int ch = 0; ch < matchChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
    const float gainReadout = 20.0f * std::log10 (std::max (1.0e-9f, unityGain.getCurrentValue()));
    if (matchUnity && blockHasSignal)
        unityGainDb.store (gainReadout, std::memory_order_relaxed);
    else if (matchUnity)
        unityGainDb.store (0.0f, std::memory_order_relaxed);

    // Final-stage 2x oversampling antialiasing filter. Runs on the completed post-mix
    // output so it antialiases the summed signal without disturbing the dry/wet blend.
    // When off, output is early by finalOversamplingLatency relative to reported PDC;
    // internal dry/wet alignment is unaffected because the oversampler is after the mix.
    if (finalOversampling != nullptr && finalOversampling->load (std::memory_order_relaxed) > 0.5f)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        finalOversampler.processSamplesUp (block);
        finalOversampler.processSamplesDown (block);
    }

    // Metering tap sits after UNITY as well, so the meters show what actually leaves.
    meters.process (buffer);

}

//==============================================================================
void ZychanStripProcessor::feedChainReference (const juce::AudioBuffer<float>& in, int numSamples)
{
    measureEnergy (chainInputRef, in, numSamples);
}

void ZychanStripProcessor::measureEnergy (std::vector<float>& dest, const juce::AudioBuffer<float>& in, int numSamples)
{
    if ((int) dest.size() < numSamples)
        dest.assign ((size_t) juce::jmax (numSamples, (int) dest.size()), 0.0f);

    const int nch = juce::jmin (2, in.getNumChannels());
    for (int i = 0; i < numSamples; ++i)
    {
        float e = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            const float x = in.getSample (c, i);
            e += x * x;
        }
        dest[(size_t) i] = nch > 0 ? e / (float) nch : 0.0f;
    }
}

//==============================================================================
std::array<int, ZychanStripProcessor::NumModules> ZychanStripProcessor::getOrder() const
{
    const uint32_t p = packedOrder.load();
    std::array<int, NumModules> o {};
    for (int i = 0; i < NumModules; ++i) o[(size_t) i] = (int) ((p >> (8 * i)) & 0xffu);
    return o;
}

void ZychanStripProcessor::setOrder (const std::array<int, NumModules>& o)
{
    std::array<bool, NumModules> seen {};
    uint32_t p = 0;
    for (int i = 0; i < NumModules; ++i)
    {
        const int id = o[(size_t) i];
        if (id < 0 || id >= NumModules || seen[(size_t) id]) return;   // not a permutation
        seen[(size_t) id] = true;
        p |= (uint32_t) id << (8 * i);
    }
    packedOrder.store (p);
}

//==============================================================================
void ZychanStripProcessor::resetToDefaults()
{
    // The Hardware Neutral Starting Profile is separate from APVTS defaults for compatibility.
    // Use explicit 0 dB manual gains; hardware knob positions are not dB gain values.
    // Values are in each parameter's native units; the profile does not claim universal hardware
    // calibration where the original controls have no fixed numeric unity point.
    static const std::array<std::pair<const char*, float>, 40> resetProfile {{
        { "con_bypass", 0 }, { "con_hpf", 20 }, { "con_lf", 0 }, { "con_lmf_f", 600 },
        { "con_lmf", 0 }, { "con_hmf_f", 3000 }, { "con_hmf", 0 }, { "con_hf", 0 },
        { "con_thresh", 0 }, { "con_ratio", 3 }, { "con_attack", 10 }, { "con_release", 200 },
        { "con_out", 0 }, { "opto_bypass", 0 }, { "opto_peak", 0 }, { "opto_gain", 0 },
        { "opto_mode", 0 }, { "fet_bypass", 0 }, { "fet_input", 0 }, { "fet_output", 0 },
        { "fet_attack", 3 }, { "fet_release", 4 }, { "lim_bypass", 0 },
        { "lim_gain", 0 }, { "lim_ceiling", -0.3f }, { "lim_release", 100 },
        { "lim_enhance", 0 }, { "master_level", 0 }, { "master_mix", 100 },
        { "lim_auto_gain", 1 }, { "con_eq_16k", 0 }, { "con_eq_8k", 0 },
        { "con_eq_4k", 0 }, { "con_eq_2k", 0 }, { "con_eq_1k", 0 }, { "con_eq_500", 0 },
        { "con_eq_250", 0 }, { "con_eq_125", 0 }, { "con_eq_63", 0 }, { "con_eq_31", 0 }
    }};
    for (auto* parameter : getParameters())
    {
        if (parameter == nullptr) continue;
        float resetValue = parameter->getDefaultValue();
        for (const auto& entry : resetProfile)
            if (auto* target = apvts.getParameter (entry.first); target == parameter)
                resetValue = target->convertTo0to1 (entry.second);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (resetValue);
        parameter->endChangeGesture();
    }
    if (auto* parameter = apvts.getParameter ("fet_ratio"); parameter != nullptr)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (0.0f);
        parameter->endChangeGesture();
    }
    if (auto* parameter = apvts.getParameter ("unity"); parameter != nullptr)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (0.0f);
        parameter->endChangeGesture();
    }

    setOrder (getDefaultOrder());
    meters.reset();
    unityGainDb.store (0.0f, std::memory_order_relaxed);

    // Module/UNITY state (envelopes, filters, delay/energy followers) clears by the next audio
    // block, so this call never touches DSP state from the message thread.
    resetRequested.store (true);
}

//==============================================================================
void ZychanStripProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::StringArray ids;
    for (int id : getOrder()) ids.add (juce::String (id));
    state.setProperty ("order", ids.joinIntoString (","), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ZychanStripProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            // Sessions saved before the master existed must restore unity, even
            // when loaded into an instance whose current master is not at unity.
            if (! state.getChildWithProperty ("id", "master_level").isValid())
            {
                juce::ValueTree masterState ("PARAM");
                masterState.setProperty ("id", "master_level", nullptr);
                masterState.setProperty ("value", 0.0f, nullptr);
                state.appendChild (masterState, nullptr);
            }
            auto addMissingParameter = [&] (const juce::String& id, float value)
            {
                if (state.getChildWithProperty ("id", id).isValid()) return;
                juce::ValueTree parameter ("PARAM");
                parameter.setProperty ("id", id, nullptr);
                parameter.setProperty ("value", value, nullptr);
                state.appendChild (parameter, nullptr);
            };
            for (const auto* id : rack::Eq560::bandIds)
                addMissingParameter (juce::String ("con_") + id, 0.0f);
            // Previously saved sessions retain their original limiter loudness.
            addMissingParameter ("lim_auto_gain", 0.0f);
            addMissingParameter ("unity", 0.0f);
            addMissingParameter ("colour_mode", 0.0f);
            addMissingParameter ("final_oversampling", 0.0f);
            apvts.replaceState (state);

            auto tokens = juce::StringArray::fromTokens (state.getProperty ("order", "0,1,2,3").toString(), ",", "");
            if (tokens.size() == NumModules)
            {
                std::array<int, NumModules> o {};
                for (int i = 0; i < NumModules; ++i) o[(size_t) i] = tokens[i].getIntValue();
                setOrder (o);
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ZychanStripProcessor::createEditor() { return new ZychanStripEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZychanStripProcessor(); }
