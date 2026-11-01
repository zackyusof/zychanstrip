#pragma once
#include "Modules.h"
#include "Meters.h"

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
    static std::array<int, NumModules> getDefaultOrder() { return { Console, Fet, Opto, Limiter }; }
    std::array<int, NumModules> getOrder() const;
    void setOrder (const std::array<int, NumModules>&);

    /** Restore every parameter to its declared default, return the rack to the factory order
        and clear all metering. Message thread only; the module DSP state itself is cleared on
        the next audio block so the reset is sample-accurate and RT-safe. */
    void resetToDefaults();

    float getGainReduction (int moduleId) const { return modules[(size_t) moduleId]->grDb.load(); }

    float getAutoGainCompensation() const { return modules[Limiter]->gainCompDb.load(); }

    float getLimiterOutput() const { return modules[Limiter]->outPeak.load(); }

    /** Post-chain metering tap (read-only; never alters audio). */
    RackMeters& getMeters() noexcept { return meters; }
    const RackMeters& getMeters() const noexcept { return meters; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<std::unique_ptr<rack::Module>, NumModules> modules;
    // Default rack order is 560 EQ -> 1176 FET -> LA-2A opto -> Enhance.
    // ModuleId is Console=0, Opto=1, Fet=2, Limiter=3, so the slots are [0,2,1,3].
    std::atomic<uint32_t> packedOrder { 0x03010200u };
    std::atomic<bool> resetRequested { false };
    RackMeters meters;
    juce::AudioBuffer<float> dryBuffer;   // untouched input for the parallel mix
    // Latency compensation for the dry path: the wet signal is delayed by the rack's reported
    // latency, so summing a raw dry copy against it combs (or cancels) partial mixes. This ring
    // holds the most recent `latencySamples` frames of the dry signal.
    std::array<std::vector<float>, 2> dryDelay;
    int dryDelayLen = 0;
    int dryWp = 0;
    // Auto-gain reference: chain input energy, captured before any module runs.
    std::vector<float> chainInputRef;   // per-sample linear input energy (stereo mean of squares)
    std::vector<float> finalOutRef;     // per-sample linear output energy (post master + mix)
    std::vector<float> prevChainInput;  // input energy paired with the output of the previous block
    std::vector<float> prevFinalOut;    // output energy of the previous block
    void publishAutoGainPair (int numSamples);
    static void measureEnergy (std::vector<float>& dest, const juce::AudioBuffer<float>& in, int numSamples);
    void feedChainReference (const juce::AudioBuffer<float>& in, int numSamples);
    void feedFinalOutputReference (const juce::AudioBuffer<float>& out, int numSamples);
    std::atomic<float>* mixAmount = nullptr;
    std::atomic<float>* masterLevel = nullptr;
    juce::SmoothedValue<float> masterGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackStripProcessor)
};
