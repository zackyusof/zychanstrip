#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout RackStripProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addF = [&] (const juce::String& id, const juce::String& name, float lo, float hi, float def,
                     const juce::String& unit = {}, float skew = 1.0f)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, 0.0f, skew), def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
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

    return layout;
}

//==============================================================================
RackStripProcessor::RackStripProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    modules[Console] = std::make_unique<rack::ConsoleModule> (apvts, "con_");
    modules[Opto]    = std::make_unique<rack::OptoModule>    (apvts, "opto_");
    modules[Fet]     = std::make_unique<rack::FetModule>     (apvts, "fet_");
    modules[Limiter] = std::make_unique<rack::LimiterModule> (apvts, "lim_");
}

bool RackStripProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == l.getMainInputChannelSet();
}

void RackStripProcessor::prepareToPlay (double sr, int block)
{
    int latency = 0;
    for (auto& m : modules)
    {
        m->prepare (sr, block);
        latency += m->getLatencySamples();
    }
    setLatencySamples (latency);   // constant regardless of order or bypass
}

void RackStripProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    for (int id : getOrder())
        modules[(size_t) id]->run (buffer);
}

//==============================================================================
std::array<int, RackStripProcessor::NumModules> RackStripProcessor::getOrder() const
{
    const uint32_t p = packedOrder.load();
    std::array<int, NumModules> o {};
    for (int i = 0; i < NumModules; ++i) o[(size_t) i] = (int) ((p >> (8 * i)) & 0xffu);
    return o;
}

void RackStripProcessor::setOrder (const std::array<int, NumModules>& o)
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
void RackStripProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::StringArray ids;
    for (int id : getOrder()) ids.add (juce::String (id));
    state.setProperty ("order", ids.joinIntoString (","), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void RackStripProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
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
juce::AudioProcessorEditor* RackStripProcessor::createEditor() { return new RackStripEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RackStripProcessor(); }
