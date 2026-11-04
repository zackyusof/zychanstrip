#pragma once
#include "PluginProcessor.h"
#include <memory>

class ModulePanel;
class RackLookAndFeel;
class MeterPanel;

/** Master rack editor: photo-real hardware chassis holding four re-orderable module face plates. */
class ZychanStripEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ZychanStripEditor (ZychanStripProcessor&);
    ~ZychanStripEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (bool meters);

public:
    /** Test seam: switch pages without synthesising a mouse event. */
    void setMeterPageVisible (bool show) { showPage (show); }
    void pumpTimerForTest() { timerCallback(); }
    bool isMeterPageVisible() const { return metersShown; }
    juce::Component* getUnityButtonForTest() { return unityButton.get(); }
    juce::Component* getModulePanelForTest (int id);
    juce::Component* getColourSelectorForTest() { return &colourSelector; }
    /** Test seam: perform the RESET action without synthesising a mouse event. */
    void resetForTest() { resetRackToDefaults(); }

private:

    juce::Rectangle<int> slotBounds (int slot) const;
    int slotForCentreX (int x) const;
    std::array<int, 4> orderWithDragged (int id, int slot) const;
    void layoutFor (const std::array<int, 4>& order, int skipId, bool animate);
    /** RESET: every module back to its default control values, factory rack order, meters cleared. */
    void resetRackToDefaults();
    void setUnityEnabled (bool enabled);

    ZychanStripProcessor& proc;
    std::unique_ptr<RackLookAndFeel> laf;
    juce::Component canvas { "Rack canvas" };
    std::array<std::unique_ptr<ModulePanel>, 4> panels;
    juce::ComponentAnimator animator;

    int draggingId = -1;
    std::array<int, 4> previewOrder { 0, 1, 2, 3 };

    // header: master level control
    juce::Slider masterFader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    // Parallel processing: dry/wet after the master volume
    juce::Slider mixFader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    juce::AffineTransform canvasTransform;

    // Metering page (SSL-style analyser) shown instead of the rack
    std::unique_ptr<MeterPanel> meterPanel;
    /** Header switch that swaps the rack for the metering page. A real child component,
        because the rack canvas intercepts clicks and the editor never sees them. */
    class PageToggle : public juce::Component
    {
    public:
        PageToggle() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        std::function<void (bool)> onChanged;
        bool meters = false;
        void setMeters (bool m) { meters = m; repaint(); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent& e) override;
    };
    PageToggle pageToggle;
    bool metersShown = false;

    class ColourSelector : public juce::Component
    {
    public:
        void connect (juce::RangedAudioParameter& p);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
    private:
        int mode = 0;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };
    ColourSelector colourSelector;

    // Header RESET and UNITY controls sit left of master/dry-wet.
    class ResetButton : public juce::Component
    {
    public:
        std::function<void()> onReset;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
    private:
        bool armed = false;
    };
    ResetButton resetButton;

    class UnityButton : public juce::Component
    {
    public:
        explicit UnityButton (ZychanStripProcessor& processor) : proc (processor) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        std::function<void (bool)> onChanged;
        void mouseExit (const juce::MouseEvent&) override;
    private:
        ZychanStripProcessor& proc;
        bool armed = false;
    };
    std::unique_ptr<UnityButton> unityButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZychanStripEditor)
};
