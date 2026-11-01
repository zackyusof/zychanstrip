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
    void mouseUp (const juce::MouseEvent&) override;

    /** Called from the editor's 30 Hz timer. */
    void tick();

    /** Invoked when the operator asks to return to the processor (rack) page. */
    std::function<void()> onBack;

    /** Invoked by the header RESET button. The editor points this at its full reset action. */
    std::function<void()> onReset;

    void setScale (int index);                 // 0 linear, 1 K-20, 2 K-14, 3 K-12
    int  getScale() const { return scale; }
    void setRtaAveraging (int index);
    static const char* scaleName (int index);

private:
    void paintHeader (juce::Graphics&);
    void paintMeters (juce::Graphics&);
    void paintScope (juce::Graphics&);
    void paintCorrelation (juce::Graphics&);
    void paintRta (juce::Graphics&);

    /** Map dBFS to 0..1 for the active metering scale. */
    float normForScale (float db) const;

    RackStripProcessor& proc;

    /** Which momentary header button is currently held, so it can draw black while pressed. */
    enum class Pressed { None, Back, Reset };
    Pressed pressed = Pressed::None;

    int scale = 0;
    juce::Rectangle<int> resetBtn, scaleBtn, backBtn, headerBar, meterArea, footerArea, scopeArea, corrArea, rtaArea;
    std::array<float, RackMeters::kBands> bandDb {};
    std::array<float, RackMeters::kBands> bandHoldDb {};
    int rtaAverage = 0;                        // 0 fast .. 3 slow (visual smoothing)
    std::vector<RackMeters::ScopePoint> scopePoints;
    float corrSmoothed = 0.0f;
    std::array<float, 2> displayPeakDb { -100.0f, -100.0f };
    std::array<float, 2> displayHoldDb { -100.0f, -100.0f };
    std::array<float, 2> displayRmsDb  { -100.0f, -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterPanel)
};
