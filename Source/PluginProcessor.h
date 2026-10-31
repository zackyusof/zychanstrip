#pragma once
#include "Modules.h"

class RackStripProcessor : public juce::AudioProcessor
{
public:
    enum ModuleId { Console = 0, Opto, Fet, Limiter, NumModules };

    RackStripProcessor();
    ~RackStripProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Rack order: order[slot] = module id. Thread-safe.
    std::array<int, NumModules> getOrder() const;
    void setOrder (const std::array<int, NumModules>&);

    float getGainReduction (int moduleId) const { return modules[(size_t) moduleId]->grDb.load(); }

    float getAutoGainCompensation() const { return modules[Limiter]->gainCompDb.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<std::unique_ptr<rack::Module>, NumModules> modules;
    std::atomic<uint32_t> packedOrder { 0x03020100u };
    std::atomic<float>* masterLevel = nullptr;
    juce::SmoothedValue<float> masterGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackStripProcessor)
};
