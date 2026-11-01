#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace rackui
{
/** Shared header pill used by the RACK/METER-style buttons.

    At rest the face carries the page-toggle teal gradient; while held it drops to matte
    black, so a momentary action visibly bottoms out instead of flashing a highlight.
    Both pages (rack header and metering page) draw their buttons with this one function. */
inline void paintPill (juce::Graphics& g, juce::Rectangle<float> r, bool pressed,
                       const juce::String& text, float fontHeight = 10.0f)
{
    const auto labelFont = juce::Font (juce::FontOptions (fontHeight, juce::Font::bold));

    if (pressed)
    {
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r.reduced (2.0f), 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setFont (labelFont);
        g.setColour (juce::Colours::white.withAlpha (0.80f));
        g.drawText (text, r.toNearestInt(), juce::Justification::centred, false);
        return;
    }

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff57d4c2), r.getX(), r.getY(),
                                             juce::Colour (0xff2f9d8f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r.reduced (2.0f), 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setFont (labelFont);
    g.setColour (juce::Colour (0xff06201d));
    g.drawText (text, r.toNearestInt(), juce::Justification::centred, false);
}
} // namespace rackui
