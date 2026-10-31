#pragma once
#include "PluginProcessor.h"
#include <array>

using namespace juce;

/** SSL-inspired metering page shown beside the rack.
    Pure presentation: all numbers come from the post-chain RackMeters tap. */
class MeterPanel : public juce::Component
{
public:
    explicit MeterPanel (RackStripProcessor& p);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    /** Called from the editor's 30 Hz timer. */
    void tick();

    void setScale (int index);                 // 0 linear, 1 K-20, 2 K-14, 3 K-12
    int  getScale() const { return scale; }
    static const char* scaleName (int index);

private:
    void paintHeader (juce::Graphics&);
    void paintMeters (juce::Graphics&);
    void paintScope (juce::Graphics&);
    void paintCorrelation (juce::Graphics&);

    /** Map dBFS to 0..1 for the active metering scale. */
    float normForScale (float db) const;

    RackStripProcessor& proc;

    int scale = 0;
    juce::Rectangle<int> resetBtn, scaleBtn, headerBar, meterArea, footerArea, scopeArea, corrArea;
    std::vector<RackMeters::ScopePoint> scopePoints;
    float corrSmoothed = 0.0f;
    std::array<float, 2> displayPeakDb { -100.0f, -100.0f };
    std::array<float, 2> displayHoldDb { -100.0f, -100.0f };
    std::array<float, 2> displayRmsDb  { -100.0f, -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterPanel)
};
