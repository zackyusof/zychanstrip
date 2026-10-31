#include "PluginEditor.h"

using namespace juce;

//==============================================================================
//  Palette + small drawing helpers
//==============================================================================
namespace
{
constexpr int kMargin = 10;      // window chrome margin
constexpr int kHeaderH = 52;     // header bar height
constexpr int kGripH = 26;       // space above each plate for the drag grip
constexpr int kGap = 16;         // gap between plates
constexpr int kEditorW = 1376;
constexpr int kEditorH = 768;

Font fnt (float h, bool bold = true) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

namespace pal
{
const Colour chassisTop     { 0xff2a2e30 };
const Colour chassisBottom  { 0xff0b0d0e };
const Colour headerBar      { 0xff272b2e };
const Colour faceDark       { 0xff1b1c1e };
const Colour faceDarkLow    { 0xff070808 };
const Colour faceLightTop   { 0xff9a9ea1 };
const Colour faceLightLow   { 0xff6e7276 };
const Colour fetTop         { 0xff1f2124 };
const Colour fetLow         { 0xff0e0f10 };
const Colour limTop         { 0xff4b555c };
const Colour limLow         { 0xff191d20 };
const Colour cream          { 0xfff3e8bf };
const Colour creamLow       { 0xffd9c48c };
const Colour needleInk      { 0xff231a10 };
const Colour engrave        { 0xffdedede };
const Colour engraveDim     { 0xff9a9a9a };
const Colour seam           { 0xff000000 };
const Colour screwHi        { 0xffb9bcc0 };
const Colour screwLo        { 0xff2c2e31 };
const Colour redCap         { 0xffc8322b };
const Colour blueCap        { 0xff2f6fbf };
const Colour greenCap       { 0xff1f7a43 };
const Colour blackCap       { 0xff24252a };
const Colour silverCap      { 0xffb9bdc2 };
const Colour btnOff         { 0xff1d2833 };
const Colour btnOn          { 0xff2f7bc4 };
}

Colour ledColour (float t)   // 0 = amber, 1 = red
{
    const Colour amber (0xffffb000), orange (0xffff6a00), red (0xffff2a1a);
    t = jlimit (0.0f, 1.0f, t);
    return t < 0.5f ? amber.interpolatedWith (orange, t * 2.0f) : orange.interpolatedWith (red, (t - 0.5f) * 2.0f);
}

/** Rounded metal plate with a vertical gradient, brushed streaks and a hairline rim. */
void paintMetalPlate (Graphics& g, Rectangle<float> r, Colour top, Colour bottom, float corner, int seed)
{
    Path shape;
    shape.addRoundedRectangle (r, corner);

    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), corner);

    g.saveState();
    g.reduceClipRegion (shape);
    g.setGradientFill (ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
    g.fillRect (r);

    // horizontal brushed streaks
    Random rnd (seed);
    for (float y = r.getY(); y < r.getBottom(); y += 1.0f + rnd.nextFloat() * 1.6f)
    {
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (rnd.nextFloat() * 0.030f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }

    // vertical sheen down the middle
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f), r.getCentreX(), r.getY(),
                                       Colours::transparentWhite, r.getCentreX(), r.getY() + r.getHeight() * 0.5f, true));
    g.fillRect (r);
    g.restoreState();

    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r.reduced (0.75f), corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r.reduced (0.25f), corner, 1.4f);
}

/** Screw with a bevelled slot head. */
void paintScrew (Graphics& g, Point<float> p, float rad, float slotAngle)
{
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p).translated (0.0f, 1.0f));

    g.setGradientFill (ColourGradient (pal::screwHi, p.x - rad, p.y - rad, pal::screwLo, p.x + rad, p.y + rad, false));
    g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p).reduced (0.6f), 0.8f);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (p).reduced (0.2f), 1.0f);

    // slot
    Path slot;
    slot.addRoundedRectangle (-rad * 0.72f, -1.1f, rad * 1.44f, 2.2f, 1.1f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillPath (slot, AffineTransform::rotation (slotAngle).translated (p.x, p.y));
    g.setColour (Colours::white.withAlpha (0.16f));
    g.fillPath (slot, AffineTransform::rotation (slotAngle).translated (p.x, p.y - 0.9f));
}

/** Six-dot drag grip sitting on the chassis above a plate. */
void paintGrip (Graphics& g, Rectangle<float> r, bool hot)
{
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 6.0f);
    g.setColour (Colours::white.withAlpha (hot ? 0.13f : 0.07f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 6.0f, 0.8f);

    const auto c = r.getCentre();
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
        {
            const Point<float> p (c.x - 5.0f + (float) col * 10.0f, c.y - 9.0f + (float) row * 9.0f);
            g.setColour (Colours::black.withAlpha (0.55f));
            g.fillEllipse (Rectangle<float> (3.4f, 3.4f).withCentre (p.translated (0.0f, 0.7f)));
            g.setColour (Colours::white.withAlpha (hot ? 0.85f : 0.55f));
            g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (p));
        }
}

/** Engraved-style caption: dark shadow underneath, bright text on top. */
void engraveText (Graphics& g, const String& text, Rectangle<int> area, Justification j, float height,
                  Colour colour, bool bold = true)
{
    g.setFont (fnt (height, bold));
    g.setColour (Colours::black.withAlpha (0.65f));
    g.drawText (text, area.translated (0, 1), j);
    g.setColour (colour);
    g.drawText (text, area, j);
}

/** Tick marks + numeric legends around a rotary control. */
struct KnobScale
{
    float a0 = MathConstants<float>::pi * 1.22f;
    float a1 = MathConstants<float>::pi * 2.78f;
    int majorTicks = 11;
    bool minorTicks = true;
};

void paintScale (Graphics& g, Point<float> centre, float radius, const KnobScale& sc, Colour c)
{
    const int total = sc.minorTicks ? (sc.majorTicks - 1) * 4 + 1 : sc.majorTicks;
    for (int i = 0; i < total; ++i)
    {
        const bool major = (i % 4) == 0;
        const float a = sc.a0 + (sc.a1 - sc.a0) * (float) i / (float) (total - 1);
        const Point<float> dir (std::sin (a), -std::cos (a));
        const float len = major ? 6.0f : 3.0f;
        g.setColour (c.withAlpha (major ? 0.95f : 0.55f));
        g.drawLine (Line<float> (centre + dir * radius, centre + dir * (radius + len)), major ? 1.5f : 1.0f);
    }
}

/** Brass-dial VU movement inside a dark bezel. The dial is an ellipse whose height drives the
    pivot and scale radius, so the same code works for the large opto meter and the small GR meter. */
void paintVuDial (Graphics& g, Rectangle<float> r, float angle, float a0, float a1,
                  const String& leftCaption, const String& rightCaption, float labelSize,
                  const String& unitCaption = {})
{
    const auto c = r.getCentre();

    // bezel: moulded dark ring with a bright inner lip
    const Rectangle<float> bezel = r.expanded (jmax (5.0f, r.getHeight() * 0.055f));
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (bezel.translated (0.0f, 3.0f));
    g.setGradientFill (ColourGradient (Colour (0xff57595b), bezel.getX(), bezel.getY(),
                                       Colour (0xff101112), bezel.getRight(), bezel.getBottom(), false));
    g.fillEllipse (bezel);
    g.setColour (Colours::white.withAlpha (0.28f));
    g.drawEllipse (bezel.reduced (0.7f), 1.2f);

    // glass face
    g.setGradientFill (ColourGradient (pal::cream, c.x, r.getY() - r.getHeight() * 0.15f,
                                       pal::creamLow, c.x, r.getBottom() + r.getHeight() * 0.10f, false));
    g.fillEllipse (r);
    g.setColour (Colour (0xff0a0906));
    g.drawEllipse (r, 1.6f);

    // glass reflection (upper-left sweep) + lower vignette
    g.saveState();
    Path clip; clip.addEllipse (r); g.reduceClipRegion (clip);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), r.getX() + r.getWidth() * 0.18f, r.getY(),
                                       Colours::transparentWhite, r.getX() + r.getWidth() * 0.45f, r.getY() + r.getHeight() * 0.62f, false));
    g.fillRect (r);
    g.setGradientFill (ColourGradient (Colours::transparentBlack, c.x, r.getY() + r.getHeight() * 0.45f,
                                       Colour (0x44000000), c.x, r.getBottom(), false));
    g.fillRect (r);
    g.restoreState();

    // Pivot sits low in the dial; the arc radius is derived from the dial height so the scale
    // never escapes the ellipse and the numerals sit just inside the rim.
    const float pivotY = r.getY() + r.getHeight() * 0.74f;
    const Point<float> pivot (c.x, pivotY);
    const float radius = r.getHeight() * 0.31f;
    auto dir = [] (float a) { return Point<float> (std::sin (a), -std::cos (a)); };

    // scale arc
    Path arc; arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, a0, a1, true);
    g.setColour (pal::needleInk.withAlpha (0.8f));
    g.strokePath (arc, PathStrokeType (1.1f));

    // ticks + numerals
    const int ticks = 21;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / (float) (ticks - 1);
        const auto d = dir (a);
        const bool major = (i % 4) == 0;
        g.setColour (pal::needleInk.withAlpha (0.9f));
        g.drawLine (Line<float> (pivot + d * radius, pivot + d * (radius + (major ? 7.0f : 4.0f))), major ? 1.4f : 0.8f);

        if (major && labelSize > 0.0f)
        {
            g.setFont (fnt (labelSize, false));
            const auto centre = pivot + d * (radius + 15.0f);
            g.drawText (String (i * 5),
                        Rectangle<float> (20.0f, 12.0f).withCentre (centre), Justification::centred);
        }
    }

    // captions inside the dial
    if (leftCaption.isNotEmpty())
    {
        g.setFont (fnt (jmax (8.0f, labelSize * 0.95f), false));
        g.setColour (pal::needleInk.withAlpha (0.8f));
        g.drawText (leftCaption, Rectangle<float> (pivot.x - radius * 0.95f, pivot.y - radius * 0.42f,
                                                   radius * 0.95f, 11.0f), Justification::centredRight);
    }
    if (unitCaption.isNotEmpty())
    {
        g.setFont (fnt (jmax (9.0f, labelSize * 1.05f), true));
        g.setColour (pal::needleInk.withAlpha (0.92f));
        // placed below the pivot so it never collides with the needle or the numerals
        g.drawText (unitCaption, Rectangle<float> (pivot.x - radius * 1.35f, pivot.y + 10.0f,
                                                   radius * 2.7f, 14.0f), Justification::centred);
    }
    if (rightCaption.isNotEmpty())
    {
        g.setFont (fnt (jmax (8.0f, labelSize * 0.95f), true));
        g.setColour (Colour (0xffa52f18));
        g.drawText (rightCaption, Rectangle<float> (pivot.x + radius * 0.05f, pivot.y - radius * 0.42f,
                                                    radius, 11.0f), Justification::centredLeft);
        Path redArc;
        redArc.addCentredArc (pivot.x, pivot.y, radius + 3.0f, radius + 3.0f, 0.0f, a1 - 0.34f, a1, true);
        g.setColour (Colour (0xffa52f18).withAlpha (0.9f));
        g.strokePath (redArc, PathStrokeType (3.2f));
    }

    // needle + pivot screw
    const float a = a0 + (a1 - a0) * jlimit (0.0f, 1.0f, angle);
    const auto nd = dir (a);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawLine (Line<float> (pivot + Point<float> (1.5f, 1.5f), pivot + nd * (radius + 1.0f) + Point<float> (1.5f, 1.5f)), 2.2f);
    Path needle;
    needle.addRoundedRectangle (-0.9f, -(radius + 1.0f), 1.8f, radius - 1.0f, 0.9f);
    g.setColour (pal::needleInk);
    g.fillPath (needle, AffineTransform::rotation (a).translated (pivot.x, pivot.y));
    g.setColour (Colour (0xff15100a));
    g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pivot));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pivot), 0.8f);
}

/** Engraved badge (e.g. model plate). */
void paintBadge (Graphics& g, Rectangle<float> r, const String& text)
{
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setFont (fnt (r.getHeight() * 0.62f));
    g.setColour (Colour (0xffe9e2c8));
    g.drawText (text, r, Justification::centred);
}

/** Tall segmented LED ladder. Segments light bottom-up. */
void paintLadder (Graphics& g, Rectangle<float> r, float lit01, Colour low, Colour high, int segments)
{
    g.setColour (Colour (0xff0a0c0d));
    g.fillRoundedRectangle (r.expanded (2.0f), 3.0f);
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r.expanded (0.5f), 2.0f, 1.0f);

    const float h = r.getHeight() / (float) segments;
    const int lit = (int) std::round (jlimit (0.0f, 1.0f, lit01) * (float) segments);
    for (int i = 0; i < segments; ++i)
    {
        const float t = (float) i / (float) (segments - 1);
        const auto c = low.interpolatedWith (high, t);
        const Rectangle<float> seg (r.getX() + 1.0f, r.getBottom() - (float) (i + 1) * h + 0.8f, r.getWidth() - 2.0f, h - 1.6f);
        const bool on = (segments - i) <= lit;
        g.setColour (on ? c : c.withMultipliedBrightness (0.14f).withAlpha (0.45f));
        g.fillRoundedRectangle (seg, 1.0f);
        if (on)
        {
            g.setColour (c.withAlpha (0.20f));
            g.fillRoundedRectangle (seg.expanded (2.0f), 2.0f);
        }
    }
}
} // namespace

//==============================================================================
//  Look and feel
//==============================================================================
class RackLookAndFeel : public LookAndFeel_V4
{
public:
    RackLookAndFeel()
    {
        setColour (Slider::rotarySliderFillColourId, pal::silverCap);
        setColour (Slider::thumbColourId, Colours::white);
        setColour (Label::textColourId, pal::engrave);
        setColour (ToggleButton::textColourId, pal::engrave);
        setColour (ToggleButton::tickColourId, Colour (0xffff3b30));
        setColour (TextButton::buttonColourId, pal::btnOff);
        setColour (TextButton::buttonOnColourId, pal::btnOn);
    }

    //-------------------------------------------------------------------- knobs
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                           float /*pos*/, float a0, float a1, Slider& s) override
    {
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float d = jmin (r.getWidth(), r.getHeight());
        auto b = r.withSizeKeepingCentre (d, d);
        const auto c = b.getCentre();
        const auto cap = s.findColour (Slider::rotarySliderFillColourId);

        // Honour the parameter's skew so the pointer agrees with the real value.
        const float pos = s.valueToProportionOfLength (s.getValue());
        const float angle = a0 + pos * (a1 - a0);

        // ---- skirt: machined dark ring the scale sits on
        const float skirtR = d * 0.50f;
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c).translated (0.0f, 1.5f));
        g.setGradientFill (ColourGradient (Colour (0xff34383b), c.x, c.y - skirtR,
                                           Colour (0xff0a0b0c), c.x, c.y + skirtR, false));
        g.fillEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c), 0.8f);

        KnobScale sc; sc.a0 = a0; sc.a1 = a1;
        paintScale (g, c, skirtR * 0.78f, sc, Colours::white.withAlpha (0.80f));

        // ---- body
        const float bodyR = d * 0.355f;
        auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);

        g.setColour (Colours::black.withAlpha (0.60f));
        g.fillEllipse (body.expanded (3.0f).translated (0.0f, 3.0f));

        // chrome/metal rim catches the light at the top-left
        g.setGradientFill (ColourGradient (Colour (0xff6b6f73), body.getX(), body.getY(),
                                           Colour (0xff0e0f10), body.getRight(), body.getBottom(), false));
        g.fillEllipse (body.expanded (2.6f));

        // cap
        auto capBody = body.reduced (bodyR * 0.05f);
        g.setGradientFill (ColourGradient (cap.brighter (0.50f), capBody.getX() + capBody.getWidth() * 0.30f,
                                           capBody.getY() + capBody.getHeight() * 0.10f,
                                           cap.darker (0.72f), capBody.getRight() * 1.0f, capBody.getBottom(), true));
        g.fillEllipse (capBody);

        g.saveState();
        Path clip; clip.addEllipse (capBody); g.reduceClipRegion (clip);

        // broad top sheen
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.42f), capBody.getX(), capBody.getY(),
                                           Colours::transparentWhite, capBody.getX(), capBody.getY() + capBody.getHeight() * 0.62f, false));
        g.fillRect (capBody.withHeight (capBody.getHeight() * 0.62f));

        // tight specular hot-spot
        const Point<float> hi (capBody.getX() + capBody.getWidth() * 0.34f, capBody.getY() + capBody.getHeight() * 0.26f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), hi.x, hi.y,
                                           Colours::transparentWhite, hi.x + capBody.getWidth() * 0.34f,
                                           hi.y + capBody.getHeight() * 0.34f, true));
        g.fillEllipse (Rectangle<float> (capBody.getWidth() * 0.75f, capBody.getHeight() * 0.75f).withCentre (hi));

        // bottom bounce light
        g.setGradientFill (ColourGradient (Colours::transparentWhite, capBody.getCentreX(), capBody.getBottom() - capBody.getHeight() * 0.34f,
                                           Colours::white.withAlpha (0.16f), capBody.getCentreX(), capBody.getBottom(), false));
        g.fillRect (capBody.withTop (capBody.getBottom() - capBody.getHeight() * 0.34f));

        // knurling
        g.setColour (Colours::black.withAlpha (0.14f));
        for (int i = 0; i < 56; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 56.0f;
            const Point<float> dir (std::sin (a), -std::cos (a));
            g.drawLine (Line<float> (c + dir * (capBody.getWidth() * 0.455f), c + dir * (capBody.getWidth() * 0.50f)), 0.6f);
        }
        g.restoreState();

        g.setColour (Colours::black.withAlpha (0.70f));
        g.drawEllipse (capBody, 1.0f);
        g.setColour (Colours::white.withAlpha (0.26f));
        g.drawEllipse (capBody.reduced (0.9f), 0.8f);

        // ---- indicator with a bevelled highlight
        const float indLen = capBody.getHeight() * 0.36f;
        Path ind;
        ind.addRoundedRectangle (-1.5f, -capBody.getHeight() * 0.44f, 3.0f, indLen, 1.5f);
        g.setColour (Colours::black.withAlpha (0.40f));
        g.fillPath (ind, AffineTransform::rotation (angle).translated (c.x, c.y + 1.3f));
        g.setColour (s.findColour (Slider::thumbColourId).withAlpha (0.97f));
        g.fillPath (ind, AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    //-------------------------------------------------------------------- faders
    void drawLinearSlider (Graphics& g, int x, int y, int w, int h,
                           float pos, float minPos, float maxPos,
                           Slider::SliderStyle style, Slider& s) override
    {
        if (style == Slider::LinearVertical)      drawChromeFader (g, x, y, w, h, pos, minPos, maxPos, s);
        else if (style == Slider::LinearHorizontal) drawMasterBar (g, x, y, w, h, pos, s);
        else LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
    }

    static void drawChromeFader (Graphics& g, int x, int y, int w, int h,
                                 float pos, float minPos, float maxPos, Slider& s)
    {
        const float cx = (float) x + (float) w * 0.5f;
        const Rectangle<float> track (cx - 4.0f, (float) y, 8.0f, (float) h);

        // recessed slot
        g.setGradientFill (ColourGradient (Colour (0xff0a0b0c), track.getX(), 0.0f, Colour (0xff1e2123), track.getRight(), 0.0f, false));
        g.fillRoundedRectangle (track, 4.0f);
        g.setColour (Colours::black.withAlpha (0.9f));
        g.drawRoundedRectangle (track, 4.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawLine (track.getX() + 0.5f, track.getY(), track.getX() + 0.5f, track.getBottom(), 1.0f);

        // travel line
        g.setColour (Colour (0xff0d3a6b));
        g.fillRect (Rectangle<float> (cx - 1.0f, (float) y + 4.0f, 2.0f, (float) h - 8.0f));

        // tick ladder on both sides
        g.setColour (Colours::white.withAlpha (0.35f));
        for (int i = 0; i <= 20; ++i)
        {
            const float ty = (float) y + (float) h * (float) i / 20.0f;
            const float len = (i % 5 == 0) ? 7.0f : 4.0f;
            g.drawLine (track.getX() - 2.0f, ty, track.getX() - 2.0f - len, ty, 1.0f);
            g.drawLine (track.getRight() + 2.0f, ty, track.getRight() + 2.0f + len, ty, 1.0f);
        }

        // chrome cap
        const float capH = 46.0f, capW = 30.0f;
        const Rectangle<float> cap (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);

        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (1.5f, 3.0f), 4.0f);

        g.setGradientFill (ColourGradient (Colour (0xfff4fdff), cap.getX() + 2.0f, cap.getY(),
                                           Colour (0xff7d9aa9), cap.getRight() - 2.0f, cap.getY(), false));
        g.fillRoundedRectangle (cap, 4.0f);

        // bright vertical band
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), cap.getCentreX() - 6.0f, 0.0f,
                                           Colours::white.withAlpha (0.85f), cap.getCentreX(), 0.0f, false));
        g.fillRect (Rectangle<float> (cap.getCentreX() - 6.0f, cap.getY() + 2.0f, 6.0f, cap.getHeight() - 4.0f));

        // knurl grooves
        g.setColour (Colour (0xff20303a).withAlpha (0.45f));
        for (float gy = cap.getY() + 5.0f; gy < cap.getBottom() - 4.0f; gy += 4.0f)
            g.drawHorizontalLine ((int) gy, cap.getX() + 3.0f, cap.getRight() - 3.0f);

        g.setColour (Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (cap, 4.0f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (cap.expanded (0.9f), 5.0f, 1.2f);
        (void) s; (void) minPos; (void) maxPos;
    }

    static void drawMasterBar (Graphics& g, int x, int y, int w, int h, float pos, Slider& s)
    {
        const Rectangle<float> track ((float) x, (float) y + (float) h * 0.5f - 4.0f, (float) w, 8.0f);
        g.setColour (Colour (0xff0a0c0d));
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (track, 3.0f, 1.0f);

        // LED segments to the right of the thumb
        const int n = 7;
        const float segW = 9.0f, segGap = 3.0f;
        const float totalW = (float) n * segW + (float) (n - 1) * segGap;
        const float sx = track.getRight() + 8.0f;
        const float t = s.valueToProportionOfLength (s.getValue());
        (void) pos;
        for (int i = 0; i < n; ++i)
        {
            const float lt = (float) i / (float) (n - 1);
            const auto c = Colour (0xff35d24a).interpolatedWith (Colour (0xffe8b21a), jlimit (0.0f, 1.0f, lt * 1.2f - 0.2f))
                                            .interpolatedWith (Colour (0xffe0452a), jlimit (0.0f, 1.0f, lt * 1.6f - 0.6f));
            const bool on = lt <= t + 0.02f;
            const Rectangle<float> seg (sx + (float) i * (segW + segGap), track.getCentreY() - 4.0f, segW, 8.0f);
            g.setColour (on ? c : c.withMultipliedBrightness (0.16f));
            g.fillRoundedRectangle (seg, 1.5f);
            if (on) { g.setColour (c.withAlpha (0.22f)); g.fillRoundedRectangle (seg.expanded (1.5f), 2.0f); }
        }
        (void) totalW;

        // chrome thumb
        const Rectangle<float> thumb ((float) x + t * (float) w - 9.0f, track.getCentreY() - 11.0f, 18.0f, 22.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (thumb.translated (1.0f, 1.5f), 3.0f);
        g.setGradientFill (ColourGradient (Colour (0xfff6feff), thumb.getX(), thumb.getY(),
                                           Colour (0xff8ea7b4), thumb.getRight(), thumb.getY(), false));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawLine (thumb.getCentreX(), thumb.getY() + 4.0f, thumb.getCentreX(), thumb.getBottom() - 4.0f, 1.0f);
    }

    //-------------------------------------------------------------------- toggles
    void drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        const auto c = b.findColour (ToggleButton::tickColourId);

        // round LED set in a recessed cup
        const float ledR = jmin (7.0f, r.getHeight() * 0.30f);
        const bool ledFirst = b.getProperties().getWithDefault ("ledFirst", true);
        const Point<float> ledC (ledFirst ? r.getX() + ledR + 5.0f
                                          : r.getRight() - ledR - 5.0f, r.getCentreY());
        const Rectangle<float> cup (ledC.x - ledR - 3.5f, ledC.y - ledR - 3.5f,
                                   (ledR + 3.5f) * 2.0f, (ledR + 3.5f) * 2.0f);

        g.setColour (Colour (0xff08090a));
        g.fillEllipse (cup);
        g.setColour (Colours::white.withAlpha (0.14f));
        g.drawEllipse (cup, 0.9f);

        const Rectangle<float> led (ledC.x - ledR, ledC.y - ledR, ledR * 2.0f, ledR * 2.0f);
        if (on)
        {
            const Rectangle<float> glow (ledC.x - ledR * 3.5f, ledC.y - ledR * 3.5f, ledR * 7.0f, ledR * 7.0f);
            g.setGradientFill (ColourGradient (c.withAlpha (0.55f), ledC.x, ledC.y,
                                               c.withAlpha (0.0f), ledC.x, ledC.y + ledR * 3.4f, true));
            g.fillEllipse (glow);
        }
        g.setGradientFill (ColourGradient (on ? c.brighter (0.65f) : Colour (0xff3a3d40), led.getX() + ledR * 0.6f, led.getY() + ledR * 0.4f,
                                           on ? c.darker (0.35f) : Colour (0xff131516), led.getX() + ledR * 0.9f, led.getY() + ledR * 1.8f, true));
        g.fillEllipse (led);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawEllipse (led, 0.7f);

        auto textArea = ledFirst ? r.withTrimmedLeft (ledC.x + ledR + 8.0f)
                                 : r.withTrimmedRight (r.getRight() - ledC.x + ledR + 8.0f);
        g.setFont (fnt (12.0f));
        g.setColour (Colours::black.withAlpha (0.65f));
        g.drawText (b.getButtonText(), textArea.translated (0, 1), Justification::centredLeft);
        g.setColour (b.findColour (ToggleButton::textColourId).withAlpha (highlighted ? 1.0f : 0.9f));
        g.drawText (b.getButtonText(), textArea, Justification::centredLeft);
    }

    //-------------------------------------------------------------------- buttons
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        auto base = on ? b.findColour (TextButton::buttonOnColourId) : b.findColour (TextButton::buttonColourId);
        if (highlighted) base = base.brighter (0.12f);

        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 3.0f);

        g.setGradientFill (ColourGradient (base.brighter (on ? 0.55f : 0.30f), 0.0f, r.getY(),
                                           base.darker (on ? 0.20f : 0.55f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);

        if (on)
        {
            g.setColour (base.brighter (0.75f).withAlpha (0.55f));
            g.fillRoundedRectangle (r.reduced (1.0f).withHeight (r.getHeight() * 0.42f), 2.0f);
            g.setColour (base.brighter (0.9f).withAlpha (0.35f));
            g.drawRoundedRectangle (r.expanded (2.0f), 4.0f, 2.5f);
        }
        g.setColour (Colours::white.withAlpha (on ? 0.45f : 0.16f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setFont (fnt (14.0f));
        const bool on = b.getToggleState();
        g.setColour (on ? Colours::white : Colour (0xff9fb6c9));
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
    }

    Font getTextButtonFont (TextButton&, int) override { return fnt (14.0f); }
};

//==============================================================================
//  Module plate base
//==============================================================================
class ModulePanel : public Component
{
public:
    ModulePanel (RackStripProcessor& p, int id, const String& prefixText,
                 Colour top, Colour bottom, Colour engraving)
        : moduleId (id), proc (p), prefix (prefixText), plateTop (top), plateLow (bottom), labelColour (engraving)
    {
        setOpaque (false);
        addAndMakeVisible (captionLayer);
        captionLayer.setInterceptsMouseClicks (false, false);
        bypass.setColour (ToggleButton::tickColourId, Colour (0xffff3b30));
        bypass.setColour (ToggleButton::textColourId, labelColour);
        bypassAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, prefix + "bypass", bypass);
        addAndMakeVisible (bypass);
    }

    void tick()
    {
        const float target = proc.getGainReduction (moduleId);
        gr += (target - gr) * 0.35f;
        if (std::abs (target - gr) < 0.002f) gr = target;
        repaint();
    }

    void paint (Graphics& g) override
    {
        paintChassisGrip (g);
        const auto plate = plateBounds().toFloat();
        paintMetalPlate (g, plate, plateTop, plateLow, 5.0f, 101 + moduleId);
        paintScrewsAndBadge (g, plate);
        if (faceImage.isValid()) g.drawImageAt (faceImage, 0, 0);
        paintMeter (g);
    }

    void resized() override
    {
        content = contentBounds();
        // Top-right of the plate; narrow panels shift it left so it never lands on a legend row.
        bypass.setBounds (content.getRight() - 84, plateBounds().getY() + 6, 80, 22);
        layoutContent (content);

        if (getWidth() > 0 && getHeight() > 0)
        {
            faceImage = Image (Image::ARGB, getWidth(), getHeight(), true);
            Graphics fg (faceImage);
            paintFace (fg);

            // Legends are drawn on a separate image and shown through a child component that
            // sits under the controls, because the controls cover the parent's own rendering.
            Image caps (Image::ARGB, getWidth(), getHeight(), true);
            Graphics cg (caps);
            paintFaceExtra (cg);
            captionLayer.image = caps;
            captionLayer.setBounds (0, 0, getWidth(), getHeight());
            captionLayer.setVisible (true);
            captionLayer.toBack();
        }
    }

    void mouseMove (const MouseEvent& e) override
    {
        const bool hot = gripBounds().toFloat().contains (e.position);
        if (hot != gripHot) { gripHot = hot; repaint (gripBounds().expanded (4.0f).toNearestInt()); }
        setMouseCursor (hot ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    }

    void mouseExit (const MouseEvent&) override
    {
        if (gripHot) { gripHot = false; repaint(); }
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (! gripBounds().toFloat().contains (e.position)) return;
        dragging = true;
        grabOffset = e.x;
        toFront (true);
        if (onDragStart) onDragStart (*this);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragging || getParentComponent() == nullptr) return;
        auto* parent = getParentComponent();
        const int px = parent->getLocalPoint (this, e.getPosition()).x;
        setTopLeftPosition (px - grabOffset, getY());
        if (onDragMove) onDragMove (*this);
    }

    void mouseUp (const MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        if (onDragEnd) onDragEnd (*this);
    }

    std::function<void (ModulePanel&)> onDragStart, onDragMove, onDragEnd;
    const int moduleId;

protected:
    struct Knob
    {
        Slider slider { Slider::RotaryVerticalDrag, Slider::NoTextBox };
        std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void setupKnob (Knob& k, const String& id, Colour cap, Colour indicator = Colours::white)
    {
        k.slider.setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
        k.slider.setColour (Slider::rotarySliderFillColourId, cap);
        k.slider.setColour (Slider::thumbColourId, indicator);
        k.slider.setPopupDisplayEnabled (true, true, nullptr);
        k.slider.setDoubleClickReturnValue (true, k.slider.getDoubleClickReturnValue());
        k.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, prefix + id, k.slider);
        addAndMakeVisible (k.slider);
    }

    static void place (Knob& k, Rectangle<int> cell)
    {
        k.slider.setBounds (cell);
    }

    /** The drawn knob occupies the top portion of its square; the rest is legend space. */
    static Rectangle<int> knobCircle (const Slider& sl)
    {
        auto b = sl.getBounds();
        const int d = jmin (b.getWidth(), b.getHeight());
        return b.withHeight (d);
    }

    Rectangle<int> plateBounds() const
    {
        return getLocalBounds().withTrimmedTop (kGripH).expanded (0, 0);
    }

    Rectangle<int> gripBounds() const
    {
        return { getWidth() / 2 - 22, 2, 44, 20 };
    }

    Rectangle<int> contentBounds() const
    {
        return plateBounds().reduced (18, 14).withTrimmedTop (10);
    }

    void caption (Graphics& g, const String& text, Rectangle<int> area, float size = 13.0f,
                  Justification j = Justification::centred, bool bold = true)
    {
        engraveText (g, text, area, j, size, labelColour, bold);
    }

    virtual void layoutContent (Rectangle<int>) = 0;
    virtual void paintFaceBase (Graphics&) {}       // grain / texture only, cached with the plate
    virtual void paintFaceExtra (Graphics&) {}      // text + legends, cached under the controls
    virtual void paintMeter (Graphics&) {}

    void drawKnobCaption (Graphics& g, const Slider& s, const String& text, float size = 13.0f)
    {
        caption (g, text, s.getBounds().withY (s.getBottom() - 15).withHeight (18), size);
    }

    RackStripProcessor& proc;
    String prefix;
    Colour plateTop, plateLow, labelColour;
    Rectangle<int> content;
    float gr = 0.0f;
    ToggleButton bypass { "BYPASS" };

private:
    void paintChassisGrip (Graphics& g) { paintGrip (g, gripBounds().toFloat(), gripHot); }

    void paintScrewsAndBadge (Graphics& g, Rectangle<float> plate)
    {
        const float inset = 13.0f, rad = 5.0f;
        const Point<float> pts[] {
            { plate.getX() + inset, plate.getY() + inset }, { plate.getRight() - inset, plate.getY() + inset },
            { plate.getX() + inset, plate.getBottom() - inset }, { plate.getRight() - inset, plate.getBottom() - inset }
        };
        const float angles[] { 0.5f, -0.9f, 1.1f, -0.3f };
        for (int i = 0; i < 4; ++i) paintScrew (g, pts[i], rad, angles[i] + (float) moduleId * 0.4f);
    }

    void paintFace (Graphics& g) { paintFaceBase (g); }

    Image faceImage;

    /** Caption layer: a child that sits under every control so legends are never covered. */
    struct CaptionLayer : public Component
    {
        Image image;
        void paint (Graphics& g) override { if (image.isValid()) g.drawImageAt (image, 0, 0); }
    };
    CaptionLayer captionLayer;
    bool dragging = false, gripHot = false;
    int grabOffset = 0;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
};

//==============================================================================
//  Module 1 — console strip: EQ + dynamics  (reference: 550B + 1176 controls)
//==============================================================================
class ConsolePanel : public ModulePanel
{
public:
    explicit ConsolePanel (RackStripProcessor& p)
        : ModulePanel (p, RackStripProcessor::Console, "con_",
                       pal::faceDark, pal::faceDarkLow, pal::engrave)
    {
        setupKnob (lf,   "lf",    pal::redCap);
        setupKnob (lmfG, "lmf",   pal::redCap);
        setupKnob (hmfG, "hmf",   pal::blueCap);
        setupKnob (hf,   "hf",    pal::blueCap);
        setupKnob (lmfF, "lmf_f", pal::greenCap, Colour (0xffe8fff0));
        setupKnob (hmfF, "hmf_f", pal::greenCap, Colour (0xffe8fff0));
        setupKnob (hpf,  "hpf",   pal::blackCap);

        setupKnob (thr, "thresh",  pal::blackCap);
        setupKnob (rat, "ratio",   pal::blackCap);
        setupKnob (atk, "attack",  pal::blackCap);
        setupKnob (rel, "release", pal::blackCap);
        setupKnob (out, "out",     pal::blackCap);

        for (auto* b : { &compLed, &gateLed })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (9001);
            b->setColour (ToggleButton::tickColourId, Colour (0xffffffff));
            b->setColour (ToggleButton::textColourId, labelColour);
            b->getProperties().set ("ledFirst", false);
            addAndMakeVisible (b);
        }
        compLed.setToggleState (true, dontSendNotification);
    }

protected:
    void layoutContent (Rectangle<int> c) override
    {
        const int colW = c.getWidth() / 2;
        const int knob = 74;
        const int cell = knob + 20;          // knob + legend line
        const int trim = 48;
        const int trimCell = trim + 16;

        int y = c.getY() + 20;

        // --- EQ section -------------------------------------------------
        eqTop = y;
        eqRow1 = y;
        eqRow2 = y + cell + 4;
        place (lf,   { c.getX() + colW / 2 - knob / 2, eqRow1, knob, knob });
        place (hmfG, { c.getX() + colW + colW / 2 - knob / 2, eqRow1, knob, knob });
        place (lmfG, { c.getX() + colW / 2 - knob / 2, eqRow2, knob, knob });
        place (hf,   { c.getX() + colW + colW / 2 - knob / 2, eqRow2, knob, knob });

        y = eqRow2 + cell + 10;
        eqRule = y;

        trimRow = y + 8;
        place (lmfF, { c.getX() + colW / 2 - trim / 2, trimRow, trim, trim });
        place (hmfF, { c.getX() + colW + colW / 2 - trim / 2, trimRow, trim, trim });

        // --- Dynamics section -------------------------------------------
        y = trimRow + trimCell + 18;
        dynTitle = { c.getX(), y, c.getWidth(), 20 };

        y = dynTitle.getBottom() + 6;
        ledRow = { c.getX(), y, c.getWidth(), 24 };
        gCtx = ledRow;

        y = ledRow.getBottom() + 12;
        thrRow = y;
        compLed.setBounds (c.getCentreX() - 88, ledRow.getY(), 66, 24);
        gateLed.setBounds (c.getCentreX() + 22, ledRow.getY(), 66, 24);

        place (thr, { c.getX() + colW / 2 - knob / 2, y, knob, knob });
        place (rat, { c.getX() + colW + colW / 2 - knob / 2, y, knob, knob });

        y = thrRow + cell + 6;
        atkRow = y;
        place (atk, { c.getX() + 10,          y, knob, knob });
        place (rel, { c.getRight() - knob - 10, y, knob, knob });
        place (out, { c.getCentreX() - 22,    y + 30, 44, 44 });

        y = atkRow + cell + 6;
        place (hpf, { c.getCentreX() - 26, y, 52, 52 });

        badge = { c.getX() + 4, c.getBottom() - 32, 74, 28 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        constexpr float capSize = 12.0f;

        engraveText (g, "EQ", { content.getX(), eqTop - 18, content.getWidth(), 18 },
                     Justification::centred, 18.0f, labelColour, false);

        caption (g, "Low",    knobCaption (lf),   capSize);
        caption (g, "Hi-Mid", knobCaption (hmfG), capSize);
        caption (g, "Low",    knobCaption (lmfG), capSize);
        caption (g, "High",   knobCaption (hf),   capSize);
        caption (g, "Q",      trimCaption (lmfF), 10.0f);
        caption (g, "Gain",   trimCaption (hmfF), 10.0f);

        g.setColour (Colours::white.withAlpha (0.20f));
        g.fillRect (Rectangle<int> (content.getX() + 4, eqRule, content.getWidth() - 8, 1));

        caption (g, "Dynamics", dynTitle, 16.0f);

        // Comp LED ... Gate LED, labels to the left of each lamp and clear of each other
        caption (g, "Comp", { compLed.getBounds().getX(), gCtx.getY(), 40, gCtx.getHeight() }, 13.0f);
        caption (g, "Gate", { gateLed.getBounds().getX(), gCtx.getY(), 40, gCtx.getHeight() }, 13.0f);

        caption (g, "Threshold", knobCaption (thr), capSize);
        caption (g, "Ratio",     knobCaption (rat), capSize);
        caption (g, "Attack",    knobCaption (atk), capSize);
        caption (g, "Release",   knobCaption (rel), capSize);
        caption (g, "Bypass",    trimCaption (out), 9.0f);
        caption (g, "HPF",       trimCaption (hpf), 9.0f);

        paintBadge (g, badge.toFloat(), "550B");
    }

    void paintMeter (Graphics&) override {}

private:
    static Rectangle<int> knobCaption (const Knob& k)
    {
        auto b = k.slider.getBounds();
        return { b.getX() - 18, b.getBottom() + 4, b.getWidth() + 36, 16 };
    }
    static Rectangle<int> trimCaption (const Knob& k)
    {
        auto b = k.slider.getBounds();
        return { b.getX() - 14, b.getBottom() + 4, b.getWidth() + 28, 12 };
    }

    Knob lf, lmfG, hmfG, hf, lmfF, hmfF, hpf, out, thr, rat, atk, rel;
    ToggleButton compLed { "" }, gateLed { "" };
    int eqTop = 0, eqRow1 = 0, eqRow2 = 0, eqRule = 0, trimRow = 0, thrRow = 0, atkRow = 0;
    Rectangle<int> dynTitle, ledRow, gCtx, badge { 20, 20, 74, 28 };
};

//==============================================================================
//  Module 2 — opto leveller (reference: LA-2A)
//==============================================================================
class OptoPanel : public ModulePanel
{
public:
    explicit OptoPanel (RackStripProcessor& p)
        : ModulePanel (p, RackStripProcessor::Opto, "opto_",
                       pal::faceLightTop, pal::faceLightLow, Colour (0xff141414))
    {
        labelColour = Colour (0xff141414);
        bypass.setColour (ToggleButton::textColourId, labelColour);

        setupKnob (gain, "gain", pal::blackCap);
        setupKnob (peak, "peak", pal::blackCap);

        // ON engages the unit's limit position: the ratio switches from ~3:1 to ~12:1.
        onBtn.setButtonText ("ON  (LIMIT)");
        onBtn.setClickingTogglesState (true);
        onBtn.setColour (ToggleButton::tickColourId, Colour (0xffffb000));
        onBtn.setColour (ToggleButton::textColourId, labelColour);
        onAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "opto_mode", onBtn);
        addAndMakeVisible (onBtn);

        // METER is a faceplate lamp with no audio effect: this unit always displays reduction.
        meterBtn.setButtonText ("METER");
        meterBtn.setClickingTogglesState (true);
        meterBtn.setToggleState (true, dontSendNotification);
        meterBtn.setColour (ToggleButton::tickColourId, Colour (0xffffb000));
        meterBtn.setColour (ToggleButton::textColourId, labelColour);
        addAndMakeVisible (meterBtn);
    }

protected:
    void layoutContent (Rectangle<int> c) override
    {
        // Large VU in the upper half (reference: dial ~1/3 of the plate height).
        const int vw = jmin (c.getWidth() - 26, 210);
        vu = { c.getCentreX() - vw / 2, c.getY() + 10, vw, vw * 3 / 4 };

        const int kw = 104;
        const int ky = vu.getBottom() + 54;
        place (gain, { c.getX() + 4,                 ky, kw, kw });
        place (peak, { c.getRight() - kw - 4,        ky, kw, kw });

        const int ty = ky + kw + 34;
        meterBtn.setBounds (c.getCentreX() - 46, ty, 92, 26);
        onBtn.setBounds (c.getCentreX() - 46, ty + 40, 92, 26);
    }

    void paintFaceBase (Graphics& g) override
    {
        Random rnd (99);   // fine wrinkle grain over the brushed metal
        for (int i = 0; i < 3000; ++i)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (rnd.nextFloat() * 0.07f));
            g.fillRect (rnd.nextFloat() * (float) getWidth(),
                        (float) kGripH + rnd.nextFloat() * (float) (getHeight() - kGripH), 1.3f, 1.3f);
        }
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "INPUT",  { gain.slider.getBounds().getCentreX() - 60, gain.slider.getY() - 22, 120, 18 }, 14.0f);
        caption (g, "GAIN",   { gain.slider.getBounds().getCentreX() - 60, gain.slider.getY() - 8,  120, 16 }, 14.0f);
        caption (g, "PEAK",   { peak.slider.getBounds().getCentreX() - 60, peak.slider.getY() - 22, 120, 18 }, 14.0f);
        caption (g, "REDUCTION", { peak.slider.getBounds().getCentreX() - 60, peak.slider.getY() - 8, 120, 16 }, 14.0f);

        paintBadge (g, Rectangle<float> ((float) content.getX() + 4.0f, (float) content.getBottom() - 34.0f, 78.0f, 30.0f), "LA-2A");
    }

    void paintMeter (Graphics& g) override
    {
        const auto r = vu.toFloat();
        g.setGradientFill (ColourGradient (Colour (0x3affd27a), r.getCentreX(), r.getCentreY(),
                                           Colour (0x00ffd27a), r.getCentreX(), r.getCentreY() - r.getHeight(), true));
        g.fillEllipse (r.expanded (20.0f));

        const float f = jlimit (0.0f, 1.0f, gr / 20.0f);
        paintVuDial (g, r, f, -0.95f, 0.95f, "", "", 10.0f, "REDUCTION");
    }

private:
    Knob gain, peak;
    ToggleButton onBtn { "ON" }, meterBtn { "METER" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    Rectangle<int> vu;
};

//==============================================================================
//  Module 3 — FET compressor (reference: 1176)
//==============================================================================
class FetPanel : public ModulePanel
{
public:
    explicit FetPanel (RackStripProcessor& p)
        : ModulePanel (p, RackStripProcessor::Fet, "fet_",
                       pal::fetTop, pal::fetLow, pal::engrave)
    {
        setupKnob (input,  "input",  pal::silverCap, Colour (0xff202020));
        setupKnob (output, "output", pal::silverCap, Colour (0xff202020));
        setupKnob (atk,    "attack", pal::silverCap, Colour (0xff202020));
        setupKnob (rel,    "release",pal::silverCap, Colour (0xff202020));

        ratioParam = proc.apvts.getParameter ("fet_ratio");
        const char* names[] { "4:1", "8:1", "12:1", "20:1", "ALL" };
        for (int i = 0; i < 5; ++i)
        {
            auto& b = ratioBtns[(size_t) i];
            b.setButtonText (names[i]);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (4711);
            b.onClick = [this, i] { ratioAtt->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        ratioAtt = std::make_unique<ParameterAttachment> (*ratioParam, [this] (float v)
        {
            const int idx = roundToInt (v);
            for (int i = 0; i < 5; ++i)
                ratioBtns[(size_t) i].setToggleState (i == idx, dontSendNotification);
        });
        ratioAtt->sendInitialUpdate();
    }

protected:
    void layoutContent (Rectangle<int> c) override
    {
        // two big silver knobs side by side at the top
        const int ky = c.getY() + 34;
        place (input,  { c.getX() + 2,               ky, 116, 116 });
        place (output, { c.getRight() - 118,         ky, 116, 116 });

        ratioLabel = { c.getX(), ky + 128, c.getWidth(), 20 };
        const int bw = (c.getWidth() - 8 - 4 * 5) / 5;
        for (int i = 0; i < 5; ++i)
            ratioBtns[(size_t) i].setBounds (c.getX() + 4 + i * (bw + 5), ratioLabel.getBottom() + 2, bw, 26);

        meter = { c.getX() + 10, ratioBtns[0].getBounds().getBottom() + 16, c.getWidth() - 20, 104 };

        const int ay = meter.getBottom() + 34;
        place (atk, { c.getX() + 8,        ay, 100, 100 });
        place (rel, { c.getRight() - 108,  ay, 100, 100 });
        grLabel = { c.getX() + c.getWidth() / 2 - 40, ay + 44, 80, 18 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "INPUT",  { input.slider.getBounds().getCentreX() - 50, input.slider.getY() - 20, 100, 18 }, 13.0f);
        caption (g, "OUTPUT", { output.slider.getBounds().getCentreX() - 50, output.slider.getY() - 20, 100, 18 }, 13.0f);
        caption (g, "RATIO",  ratioLabel, 14.0f);
        caption (g, "ATTACK", { atk.slider.getBounds().getCentreX() - 50, atk.slider.getY() - 16, 100, 18 }, 13.0f);
        caption (g, "RELEASE",{ rel.slider.getBounds().getCentreX() - 50, rel.slider.getY() - 16, 100, 18 }, 13.0f);

        // UREI-style wordmark in the top centre
        engraveText (g, "urei", { content.getX(), content.getY() - 6, content.getWidth(), 22 },
                     Justification::centred, 21.0f, Colour (0xffe8e8e8), false);

        paintBadge (g, { content.getCentreX() - 44.0f, (float) content.getBottom() - 30.0f, 88.0f, 26.0f }, "1176");
    }

    void paintMeter (Graphics& g) override
    {
        const auto r = meter.toFloat();
        g.setColour (Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (r.expanded (4.0f), 3.0f);
        g.setGradientFill (ColourGradient (Colour (0xfff6eccb), r.getCentreX(), r.getY(),
                                           Colour (0xffd8c091), r.getCentreX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.0f);

        const float f = jlimit (0.0f, 1.0f, gr / 20.0f);
        paintVuDial (g, r.reduced (3.0f), f, -0.95f, 0.95f, "db", "GR", 9.0f);
    }

private:
    Knob input, output, atk, rel;
    std::array<TextButton, 5> ratioBtns;
    RangedAudioParameter* ratioParam = nullptr;
    std::unique_ptr<ParameterAttachment> ratioAtt;
    Rectangle<int> meter, ratioLabel, grLabel;
};

//==============================================================================
//  Module 4 — master limiter (reference: SSL-style fader + ladders)
//==============================================================================
class LimiterPanel : public ModulePanel
{
public:
    explicit LimiterPanel (RackStripProcessor& p)
        : ModulePanel (p, RackStripProcessor::Limiter, "lim_",
                       pal::limTop, pal::limLow, pal::engrave)
    {
        setupKnob (gain,    "gain",    pal::blackCap, Colour (0xffffb000));
        setupKnob (ceiling, "ceiling", pal::blackCap, Colour (0xffffb000));
        setupKnob (release, "release", pal::blackCap, Colour (0xffffb000));

        fader.setSliderStyle (Slider::LinearVertical);
        fader.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        fader.setPopupDisplayEnabled (true, true, nullptr);
        faderAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "lim_enhance", fader);
        addAndMakeVisible (fader);
    }

protected:
    void layoutContent (Rectangle<int> c) override
    {
        const int kh = 84;
        const int kw = c.getWidth() / 3;
        const int ky = c.getY() + 26;
        place (gain,    { c.getX(),                ky, kw, kh });
        place (ceiling, { c.getX() + kw,           ky, kw, kh });
        place (release, { c.getX() + kw * 2,       ky, kw, kh });

        const int fy = ky + kh + 22;
        const int fh = c.getBottom() - fy - 30;
        ladderA = { c.getX() + 24, fy + 22, 26, fh };
        ladderB = { c.getRight() - 50, fy + 22, 26, fh };
        fader.setBounds (c.getCentreX() - 26, fy, 52, fh + 22);
        scaleRect = { c.getCentreX() - 62, fy + 22, 124, fh };
        enhanceLabel = { c.getX(), fader.getBottom() + 2, c.getWidth(), 20 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "THRESHOLD", { gain.slider.getBounds().getX() - 16,    gain.slider.getY() - 19, gain.slider.getBounds().getWidth() + 32, 17 }, 11.0f);
        caption (g, "CEILING",   { ceiling.slider.getBounds().getX() - 16, ceiling.slider.getY() - 19, ceiling.slider.getBounds().getWidth() + 32, 17 }, 11.0f);
        caption (g, "RELEASE",   { release.slider.getBounds().getX() - 16, release.slider.getY() - 19, release.slider.getBounds().getWidth() + 32, 17 }, 11.0f);
        caption (g, "ENHANCE", enhanceLabel, 15.0f);

        // dB scale beside the fader
        g.setFont (fnt (10.0f, false));
        for (int i = 0; i <= 6; ++i)
        {
            const float t = (float) i / 6.0f;
            const float y = (float) scaleRect.getY() + (float) scaleRect.getHeight() * t;
            const int db = (int) std::round (12.0f - 24.0f * t);
            g.setColour (Colours::black.withAlpha (0.65f));
            g.drawText (String (db), Rectangle<float> ((float) scaleRect.getX(), y - 7.0f, 26.0f, 14.0f), Justification::centredRight);
            g.setColour (Colours::white.withAlpha (0.5f));
            g.drawText (String (db), Rectangle<float> ((float) scaleRect.getX(), y - 8.0f, 26.0f, 14.0f), Justification::centredRight);
        }
        for (int i = 0; i <= 12; ++i)
        {
            const float y = (float) scaleRect.getY() + (float) scaleRect.getHeight() * (float) i / 12.0f;
            g.setColour (Colours::white.withAlpha (0.22f));
            g.drawLine ((float) scaleRect.getCentreX() + 14.0f, y, (float) scaleRect.getCentreX() + 22.0f, y, 1.0f);
            g.drawLine ((float) scaleRect.getCentreX() - 22.0f, y, (float) scaleRect.getCentreX() - 14.0f, y, 1.0f);
        }

        // slots beside the ladders
        for (auto s : { Rectangle<int> (ladderA.getX() - 14, ladderA.getY() - 6, 4, ladderA.getHeight() + 12),
                        Rectangle<int> (ladderB.getRight() + 10, ladderB.getY() - 6, 4, ladderB.getHeight() + 12) })
        {
            g.setColour (Colour (0xff080a0b));
            g.fillRoundedRectangle (s.toFloat(), 2.0f);
            g.setColour (Colours::white.withAlpha (0.16f));
            g.drawRoundedRectangle (s.toFloat(), 2.0f, 1.0f);
        }
    }

    void paintMeter (Graphics& g) override
    {
        // Ladder A = gain reduction (amber), ladder B = output level (green→blue). Both always show
        // activity so the panel reads like the hardware even when nothing is being limited.
        const float red = jlimit (0.0f, 1.0f, gr / 14.0f);
        paintLadder (g, ladderA.toFloat(), red * 0.85f + 0.06f, Colour (0xffc06a10), Colour (0xffffd23a), 28);
        paintLadder (g, ladderB.toFloat(), juce::jlimit (0.0f, 1.0f, 0.30f + red * 0.55f),
                     Colour (0xff17b04a), Colour (0xff5fe1f5), 28);

        // level readout under the ladders
        engraveText (g, "-" + String (gr, 1) + " dB", { ladderA.getX() - 18, ladderA.getBottom() + 8, ladderA.getWidth() + 36, 16 },
                     Justification::centred, 11.0f, labelColour, false);
    }

private:
    Knob gain, ceiling, release;
    Slider fader;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> faderAtt;
    Rectangle<int> ladderA, ladderB, scaleRect, enhanceLabel;
};

//==============================================================================
//  Editor
//==============================================================================
RackStripEditor::RackStripEditor (RackStripProcessor& p)
    : AudioProcessorEditor (p), proc (p), laf (std::make_unique<RackLookAndFeel>())
{
    setLookAndFeel (laf.get());

    panels[RackStripProcessor::Console] = std::make_unique<ConsolePanel> (proc);
    panels[RackStripProcessor::Opto]    = std::make_unique<OptoPanel> (proc);
    panels[RackStripProcessor::Fet]     = std::make_unique<FetPanel> (proc);
    panels[RackStripProcessor::Limiter] = std::make_unique<LimiterPanel> (proc);

    for (auto& panel : panels)
    {
        panel->onDragStart = [this] (ModulePanel& m)
        {
            draggingId = m.moduleId;
            previewOrder = proc.getOrder();
            animator.cancelAnimation (&m, false);
        };
        panel->onDragMove = [this] (ModulePanel& m)
        {
            auto o = orderWithDragged (m.moduleId, slotForCentreX (m.getBounds().getCentreX()));
            if (o != previewOrder) { previewOrder = o; layoutFor (o, m.moduleId, true); }
        };
        panel->onDragEnd = [this] (ModulePanel&)
        {
            proc.setOrder (previewOrder);
            draggingId = -1;
            layoutFor (previewOrder, -1, true);
        };
        addAndMakeVisible (*panel);
    }

    // master level in the header (a display-only trim over the limiter output)
    masterFader.setRange (0.0, 1.0, 0.0);
    masterFader.setValue (0.82, dontSendNotification);
    masterFader.setColour (Slider::thumbColourId, Colours::white);
    addAndMakeVisible (masterFader);

    setSize (kEditorW, kEditorH);
    startTimerHz (30);
}

RackStripEditor::~RackStripEditor()
{
    stopTimer();
    animator.cancelAllAnimations (false);
    setLookAndFeel (nullptr);
}

Rectangle<int> RackStripEditor::slotBounds (int slot) const
{
    const int pw = (getWidth() - kMargin * 2 - kGap * 3) / 4;
    const int top = kHeaderH + kGripH + 4;
    return { kMargin + slot * (pw + kGap), top, pw, getHeight() - top - kMargin };
}

int RackStripEditor::slotForCentreX (int x) const
{
    const int pw = (getWidth() - kMargin * 2 - kGap * 3) / 4;
    return jlimit (0, 3, (x - kMargin) / (pw + kGap));
}

std::array<int, 4> RackStripEditor::orderWithDragged (int id, int slot) const
{
    std::vector<int> v;
    for (int m : proc.getOrder()) if (m != id) v.push_back (m);
    v.insert (v.begin() + jlimit (0, (int) v.size(), slot), id);
    return { v[0], v[1], v[2], v[3] };
}

void RackStripEditor::layoutFor (const std::array<int, 4>& order, int skipId, bool animate)
{
    for (int s = 0; s < 4; ++s)
    {
        const int id = order[(size_t) s];
        if (id == skipId) continue;
        auto* c = panels[(size_t) id].get();
        if (animate) animator.animateComponent (c, slotBounds (s), 1.0f, 140, false, 1.0, 1.0);
        else         c->setBounds (slotBounds (s));
    }
}

void RackStripEditor::resized()
{
    animator.cancelAllAnimations (false);
    layoutFor (proc.getOrder(), -1, false);
    masterFader.setBounds (getWidth() - kMargin - 150, kHeaderH / 2 - 12, 130, 24);

    if (getWidth() > 0 && getHeight() > 0)
    {
        chromeImage = Image (Image::ARGB, getWidth(), getHeight(), true);
        Graphics cg (chromeImage);
        paint (cg);
    }
}

void RackStripEditor::timerCallback()
{
    for (auto& p : panels) p->tick();
}

void RackStripEditor::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // outer chassis
    g.setColour (Colour (0xff08090a));
    g.fillRect (full);
    g.setGradientFill (ColourGradient (pal::chassisTop, 0.0f, 0.0f, pal::chassisBottom, 0.0f, full.getBottom(), false));
    g.fillRoundedRectangle (full.reduced (2.0f), 10.0f);

    // inner recess the plates sit in
    const Rectangle<float> inner (4.0f, (float) kHeaderH + 2.0f, full.getWidth() - 8.0f, full.getHeight() - (float) kHeaderH - 8.0f);
    g.setGradientFill (ColourGradient (Colour (0xff151819), 0.0f, inner.getY(), Colour (0xff050606), 0.0f, inner.getBottom(), false));
    g.fillRoundedRectangle (inner, 8.0f);

    // header bar
    const Rectangle<float> header (4.0f, 6.0f, full.getWidth() - 8.0f, (float) kHeaderH - 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xff34393c), 0.0f, header.getY(),
                                       Colour (0xff1b1e20), 0.0f, header.getBottom(), false));
    g.fillRoundedRectangle (header, 8.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawLine (header.getX() + 6.0f, header.getY() + 1.0f, header.getRight() - 6.0f, header.getY() + 1.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawLine (header.getX() + 6.0f, header.getBottom(), header.getRight() - 6.0f, header.getBottom(), 1.2f);

    // traffic-light glyphs
    const float gy = header.getCentreY();
    g.setColour (Colour (0xffd9dcde));
    g.drawLine (30.0f, gy - 6.0f, 44.0f, gy + 6.0f, 1.6f);
    g.drawLine (30.0f, gy + 6.0f, 44.0f, gy - 6.0f, 1.6f);
    g.drawRoundedRectangle (Rectangle<float> (66.0f, gy - 1.5f, 14.0f, 3.0f), 1.5f, 1.0f);
    g.setColour (Colour (0xffd9dcde).withAlpha (0.8f));
    g.drawEllipse (Rectangle<float> (12.0f, 12.0f).withCentre ({ 100.0f, gy }), 1.2f);

    // title
    g.setFont (fnt (21.0f, false));
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawText ("MASTERING RACK", header.translated (0.0f, 1.0f), Justification::centred);
    g.setColour (Colour (0xffdfe2e3));
    g.drawText ("MASTERING RACK", header, Justification::centred);

    // master caption
    g.setFont (fnt (13.0f, false));
    g.setColour (Colour (0xffc9cdcf));
    g.drawText ("Master", Rectangle<float> (full.getWidth() - 220.0f, 0.0f, 62.0f, (float) kHeaderH),
                Justification::centredRight);

    // plate shadows so the faces read as raised hardware
    for (int s = 0; s < 4; ++s)
    {
        const auto r = slotBounds (s);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.toFloat().withTrimmedTop (kGripH).translated (0.0f, 4.0f), 6.0f);
    }
}

void RackStripEditor::paintOverChildren (Graphics& g)
{
    // outer bevel last so it sits above the plates
    const auto full = getLocalBounds().toFloat();
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (full.reduced (2.5f), 9.0f, 1.2f);
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (full.reduced (0.5f), 10.0f, 1.5f);
}
