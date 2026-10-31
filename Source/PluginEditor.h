#pragma once
#include "PluginProcessor.h"

class ModulePanel;
class RackLookAndFeel;

/** Master rack editor: photo-real hardware chassis holding four re-orderable module face plates. */
class RackStripEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RackStripEditor (RackStripProcessor&);
    ~RackStripEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Rectangle<int> slotBounds (int slot) const;
    int slotForCentreX (int x) const;
    std::array<int, 4> orderWithDragged (int id, int slot) const;
    void layoutFor (const std::array<int, 4>& order, int skipId, bool animate);

    RackStripProcessor& proc;
    std::unique_ptr<RackLookAndFeel> laf;
    juce::Component canvas { "Rack canvas" };
    std::array<std::unique_ptr<ModulePanel>, 4> panels;
    juce::ComponentAnimator animator;

    int draggingId = -1;
    std::array<int, 4> previewOrder { 0, 1, 2, 3 };

    // header: master level control
    juce::Slider masterFader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    juce::AffineTransform canvasTransform;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RackStripEditor)
};
