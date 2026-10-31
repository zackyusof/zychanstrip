#include "PluginEditor.h"

using namespace juce;

//==============================================================================
//  Palette + photorealistic drawing helpers
//==============================================================================
namespace
{
constexpr int kMargin = 8;      // window chrome margin
constexpr int kHeaderH = 44;     // header bar height
constexpr int kGripH = 24;       // space above each plate for the drag grip
constexpr int kGap = 10;         // gap between plates
constexpr int kEditorW = 1080;
constexpr int kEditorH = 760;

// Authentic zynamfx ZY monogram SVG (exact brand mark from zynamfx Brand.cpp)
constexpr const char* kZyMarkSvg =
R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 44 20"><rect x="0.75" y="0.75" width="18.5" height="18.5" rx="3" fill="#35262b" stroke="#f06470" stroke-width="1.5"/><path d="M4.5 5h11l-11 10h11" fill="none" stroke="#f06470" stroke-width="2.25" stroke-linecap="square" stroke-linejoin="miter"/><rect x="24.75" y="0.75" width="18.5" height="18.5" rx="3" fill="#19322f" stroke="#57d4c2" stroke-width="1.5"/><path d="m28.5 5 5.5 5 5.5-5M34 10v5" fill="none" stroke="#57d4c2" stroke-width="2.25" stroke-linecap="square" stroke-linejoin="miter"/></svg>)svg";

Font fnt (float h, bool bold = true) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

namespace pal
{
const Colour chassisTop     { 0xff202428 };
const Colour chassisBottom  { 0xff090b0c };
const Colour chassisGrain   { 0xff14171a };
const Colour headerBarTop   { 0xff2c3135 };
const Colour headerBarLow   { 0xff15181a };
const Colour faceDark       { 0xff1e2124 };
const Colour faceDarkLow    { 0xff0a0c0d };
const Colour faceLightTop   { 0xffe2e4e1 };
const Colour faceLightLow   { 0xffafb2ae };
const Colour fetTop         { 0xff181a1c };
const Colour fetLow         { 0xff0b0d0e };
const Colour limTop         { 0xff415462 };
const Colour limLow         { 0xff1f2a33 };
const Colour cream          { 0xfffcf5dc };
const Colour creamLow       { 0xffd9c798 };
const Colour needleInk      { 0xff18120c };
const Colour engrave        { 0xffe6e8ea };
const Colour engraveDim     { 0xff989ea4 };
const Colour screwHi        { 0xffd4d7db };
const Colour screwLo        { 0xff282b2e };
const Colour redCap         { 0xffc82824 };
const Colour blueCap        { 0xff2b66b2 };
const Colour greenCap       { 0xff1e7a42 };
const Colour blackCap       { 0xff222428 };
const Colour silverCap      { 0xffc8ccd1 };
const Colour btnOff         { 0xff182028 };
const Colour btnOn          { 0xff296eb4 };
}

void drawZyLogo (Graphics& g, Rectangle<float> bounds)
{
    static std::unique_ptr<Drawable> drawable = []() {
        if (auto xml = XmlDocument::parse (kZyMarkSvg))
            return Drawable::createFromSVG (*xml);
        return std::unique_ptr<Drawable>();
    }();
    if (drawable != nullptr)
        drawable->drawWithin (g, bounds, RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize, 1.0f);
}

/** Realistic brushed metal faceplate with anisotropic lighting and chamfered bevels. */
void paintMetalPlate (Graphics& g, Rectangle<float> r, Colour top, Colour bottom, float corner, int seed)
{
    Path shape;
    shape.addRoundedRectangle (r, corner);

    // Deep ambient occlusion shadow behind plate
    g.setColour (Colours::black.withAlpha (0.70f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.5f), corner + 1.0f);
    g.setColour (Colours::black.withAlpha (0.40f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), corner);

    g.saveState();
    g.reduceClipRegion (shape);

    // Primary anodized metal gradient
    g.setGradientFill (ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
    g.fillRect (r);

    // Dense fine brushed metal grain
    Random rnd (seed);
    for (float y = r.getY(); y < r.getBottom(); y += 0.85f + rnd.nextFloat() * 1.4f)
    {
        const float alpha = rnd.nextFloat() * 0.038f;
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (alpha));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }

    // Overhead diffuse specular sheen
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.08f), r.getCentreX(), r.getY(),
                                       Colours::transparentWhite, r.getCentreX(), r.getY() + r.getHeight() * 0.45f, false));
    g.fillRect (r.withHeight (r.getHeight() * 0.45f));

    // Subtle bottom bounce highlight
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.04f), r.getCentreX(), r.getBottom(),
                                       Colours::transparentWhite, r.getCentreX(), r.getBottom() - r.getHeight() * 0.20f, false));
    g.fillRect (r.withTop (r.getBottom() - r.getHeight() * 0.20f));

    g.restoreState();

    // Machined chamfered outer bevel: bright top-left, shadowed bottom-right
    g.setColour (Colours::white.withAlpha (0.32f));
    g.drawLine (r.getX() + corner, r.getY() + 0.5f, r.getRight() - corner, r.getY() + 0.5f, 1.0f);
    g.drawLine (r.getX() + 0.5f, r.getY() + corner, r.getX() + 0.5f, r.getBottom() - corner, 1.0f);

    g.setColour (Colours::black.withAlpha (0.80f));
    g.drawLine (r.getX() + corner, r.getBottom() - 0.5f, r.getRight() - corner, r.getBottom() - 0.5f, 1.0f);
    g.drawLine (r.getRight() - 0.5f, r.getY() + corner, r.getRight() - 0.5f, r.getBottom() - corner, 1.0f);

    // Hairline inner rim
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

/** Precision countersunk machine screw with metallic sheen and 3D slot. */
void paintScrew (Graphics& g, Point<float> p, float rad, float slotAngle)
{
    // Countersunk bevel well in the panel
    const Rectangle<float> well (p.x - rad - 1.4f, p.y - rad - 1.4f, (rad + 1.4f) * 2.0f, (rad + 1.4f) * 2.0f);
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillEllipse (well);
    g.setColour (Colours::white.withAlpha (0.22f));
    g.drawEllipse (well.reduced (0.4f), 0.7f);

    // Screw head dome with metallic gradient
    const Rectangle<float> head (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);
    g.setGradientFill (ColourGradient (pal::screwHi, p.x - rad * 0.7f, p.y - rad * 0.7f,
                                       pal::screwLo, p.x + rad * 0.7f, p.y + rad * 0.7f, false));
    g.fillEllipse (head);

    // Concentric specular ring
    g.setColour (Colours::white.withAlpha (0.45f));
    g.drawEllipse (head.reduced (0.6f), 0.6f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (head, 0.8f);

    // 3D screw slot
    Path slot;
    slot.addRoundedRectangle (-rad * 0.74f, -1.05f, rad * 1.48f, 2.1f, 0.8f);
    AffineTransform xform = AffineTransform::rotation (slotAngle).translated (p.x, p.y);

    // Slot inner depth
    g.setColour (Colour (0xff08090a));
    g.fillPath (slot, xform);

    // Bright lower edge catching light
    AffineTransform hiXform = AffineTransform::rotation (slotAngle).translated (p.x, p.y + 0.75f);
    g.setColour (Colours::white.withAlpha (0.30f));
    g.strokePath (slot, PathStrokeType (0.6f), hiXform);
}

/** Machined aluminum module extraction handle / drag grip with knurling. */
void paintGrip (Graphics& g, Rectangle<float> r, bool hot)
{
    // Drop shadow onto chassis
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 5.0f);

    // Milled anodized block
    g.setGradientFill (ColourGradient (hot ? Colour (0xff484e54) : Colour (0xff32373c), 0.0f, r.getY(),
                                       hot ? Colour (0xff1f2226) : Colour (0xff141618), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);

    // Machined chamfer
    g.setColour (Colours::white.withAlpha (hot ? 0.35f : 0.18f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);

    // Knurled thumb grip texture (6 raised precision dots with 3D relief)
    const auto c = r.getCentre();
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 2; ++col)
        {
            const Point<float> p (c.x - 5.5f + (float) col * 11.0f, c.y - 8.0f + (float) row * 8.0f);
            g.setColour (Colours::black.withAlpha (0.75f));
            g.fillEllipse (Rectangle<float> (3.6f, 3.6f).withCentre (p.translated (0.0f, 0.8f)));

            g.setGradientFill (ColourGradient (Colours::white.withAlpha (hot ? 0.95f : 0.70f), p.x - 1.0f, p.y - 1.0f,
                                               Colour (0xff70767c), p.x + 1.2f, p.y + 1.2f, false));
            g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (p));
        }
    }
}

/** Engraved studio typography with recessed shadow and sharp surface highlight. */
void engraveText (Graphics& g, const String& text, Rectangle<int> area, Justification j, float height,
                  Colour colour, bool bold = true)
{
    g.setFont (fnt (height, bold));
    // Crisp dark drop shadow underneath
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText (text, area.translated (0, 1), j);
    // Main sharp engraved legend
    g.setColour (colour);
    g.drawText (text, area, j);
}

/** Dial scale markings around knobs. */
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
        const float len = major ? 6.5f : 3.5f;

        // Shadow under tick
        g.setColour (Colours::black.withAlpha (0.50f));
        g.drawLine (Line<float> (centre + dir * radius + Point<float> (0.0f, 0.8f),
                                 centre + dir * (radius + len) + Point<float> (0.0f, 0.8f)), major ? 1.5f : 1.0f);

        // Crisp tick mark
        g.setColour (c.withAlpha (major ? 0.95f : 0.58f));
        g.drawLine (Line<float> (centre + dir * radius, centre + dir * (radius + len)), major ? 1.5f : 1.0f);
    }
}

/** Photorealistic vintage VU meter with authentic tungsten backlighting, convex glass reflection,
    and needle shadow cast onto the dial face. Works for both round (Opto) and rectangular (FET) meters. */
void paintVuDial (Graphics& g, Rectangle<float> r, float angle, float a0, float a1,
                  const String& leftCaption, const String& rightCaption, float labelSize,
                  const String& unitCaption = {}, bool isRectangular = false)
{
    const auto c = r.getCentre();

    // 1. Heavy moulded bakelite bezel
    const Rectangle<float> bezel = r.expanded (jmax (6.0f, r.getHeight() * 0.065f));
    g.setColour (Colours::black.withAlpha (0.80f));
    if (isRectangular)
        g.fillRoundedRectangle (bezel.translated (0.0f, 3.5f), 5.0f);
    else
        g.fillEllipse (bezel.translated (0.0f, 3.5f));

    g.setGradientFill (ColourGradient (Colour (0xff484c50), bezel.getX(), bezel.getY(),
                                       Colour (0xff101214), bezel.getRight(), bezel.getBottom(), false));
    if (isRectangular)
        g.fillRoundedRectangle (bezel, 4.0f);
    else
        g.fillEllipse (bezel);

    // Bezel inner stepped brass/chrome retaining rim
    g.setColour (Colours::white.withAlpha (0.35f));
    if (isRectangular)
        g.drawRoundedRectangle (bezel.reduced (0.8f), 4.0f, 1.2f);
    else
        g.drawEllipse (bezel.reduced (0.8f), 1.2f);

    g.setColour (Colour (0xffb89852).withAlpha (0.60f));
    if (isRectangular)
        g.drawRoundedRectangle (r.expanded (1.6f), 3.0f, 1.2f);
    else
        g.drawEllipse (r.expanded (1.8f), 1.2f);

    // 2. Warm backlit dial paper
    g.setGradientFill (ColourGradient (pal::cream, c.x, r.getY() - r.getHeight() * 0.15f,
                                       pal::creamLow, c.x, r.getBottom() + r.getHeight() * 0.10f, false));
    if (isRectangular)
        g.fillRoundedRectangle (r, 3.0f);
    else
        g.fillEllipse (r);

    // Warm incandescent bulb glow behind the glass
    g.setGradientFill (ColourGradient (Colour (0x55ffde82), c.x, r.getY() + r.getHeight() * 0.20f,
                                       Colours::transparentWhite, c.x, r.getBottom(), true));
    if (isRectangular)
        g.fillRoundedRectangle (r, 3.0f);
    else
        g.fillEllipse (r);

    g.setColour (Colour (0xff0a0907));
    if (isRectangular)
        g.drawRoundedRectangle (r, 3.0f, 1.4f);
    else
        g.drawEllipse (r, 1.6f);

    // 3. Dial scale arc & markings
    const float pivotY = r.getY() + r.getHeight() * 0.74f;
    const Point<float> pivot (c.x, pivotY);
    const float radius = r.getHeight() * 0.31f;
    auto dir = [] (float a) { return Point<float> (std::sin (a), -std::cos (a)); };

    // Scale arc
    Path arc; arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, a0, a1, true);
    g.setColour (pal::needleInk.withAlpha (0.85f));
    g.strokePath (arc, PathStrokeType (1.2f));

    // Dial ticks + numerals
    const int ticks = 21;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / (float) (ticks - 1);
        const auto d = dir (a);
        const bool major = (i % 4) == 0;
        g.setColour (pal::needleInk.withAlpha (0.92f));
        g.drawLine (Line<float> (pivot + d * radius, pivot + d * (radius + (major ? 7.0f : 4.0f))), major ? 1.4f : 0.85f);

        if (major && labelSize > 0.0f)
        {
            g.setFont (fnt (labelSize, false));
            const auto centre = pivot + d * (radius + 15.0f);
            g.drawText (String (i * 5), Rectangle<float> (20.0f, 12.0f).withCentre (centre), Justification::centred);
        }
    }

    // Dial text captions
    if (leftCaption.isNotEmpty())
    {
        g.setFont (fnt (jmax (8.0f, labelSize * 0.95f), false));
        g.setColour (pal::needleInk.withAlpha (0.85f));
        g.drawText (leftCaption, Rectangle<float> (pivot.x - radius * 0.95f, pivot.y - radius * 0.42f,
                                                   radius * 0.95f, 11.0f), Justification::centredRight);
    }
    if (unitCaption.isNotEmpty())
    {
        g.setFont (fnt (jmax (9.0f, labelSize * 1.05f), true));
        g.setColour (pal::needleInk.withAlpha (0.92f));
        g.drawText (unitCaption, Rectangle<float> (pivot.x - radius * 1.35f, pivot.y + 10.0f,
                                                   radius * 2.7f, 14.0f), Justification::centred);
    }
    if (rightCaption.isNotEmpty())
    {
        g.setFont (fnt (jmax (8.0f, labelSize * 0.95f), true));
        g.setColour (Colour (0xffb82618));
        g.drawText (rightCaption, Rectangle<float> (pivot.x + radius * 0.05f, pivot.y - radius * 0.42f,
                                                    radius, 11.0f), Justification::centredLeft);
        Path redArc;
        redArc.addCentredArc (pivot.x, pivot.y, radius + 3.0f, radius + 3.0f, 0.0f, a1 - 0.34f, a1, true);
        g.setColour (Colour (0xffb82618).withAlpha (0.92f));
        g.strokePath (redArc, PathStrokeType (3.2f));
    }

    // 4. Pointer needle with realistic drop shadow cast onto paper
    const float a = a0 + (a1 - a0) * jlimit (0.0f, 1.0f, angle);

    // Needle shadow (offset 2.2px down and right)
    Point<float> shadowOff (2.2f, 2.2f);
    Path shadowNeedle;
    shadowNeedle.addRoundedRectangle (-1.0f, -(radius + 1.0f), 2.0f, radius - 1.0f, 0.9f);
    g.setColour (Colours::black.withAlpha (0.28f));
    g.fillPath (shadowNeedle, AffineTransform::rotation (a).translated (pivot.x + shadowOff.x, pivot.y + shadowOff.y));

    // Needle blade
    Path needle;
    needle.addRoundedRectangle (-0.95f, -(radius + 1.0f), 1.9f, radius - 1.0f, 0.9f);
    g.setColour (pal::needleInk);
    g.fillPath (needle, AffineTransform::rotation (a).translated (pivot.x, pivot.y));

    // Needle pivot hub
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (pivot.translated (0.0f, 1.0f)));
    g.setGradientFill (ColourGradient (Colour (0xff2c2824), pivot.x - 3.0f, pivot.y - 3.0f,
                                       Colour (0xff0a0806), pivot.x + 3.0f, pivot.y + 3.0f, false));
    g.fillEllipse (Rectangle<float> (9.5f, 9.5f).withCentre (pivot));
    g.setColour (Colours::white.withAlpha (0.45f));
    g.drawEllipse (Rectangle<float> (9.5f, 9.5f).withCentre (pivot), 0.8f);

    // 5. Curved glass dome reflection
    g.saveState();
    Path clip;
    if (isRectangular)
        clip.addRoundedRectangle (r, 3.0f);
    else
        clip.addEllipse (r);
    g.reduceClipRegion (clip);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.45f), r.getX() + r.getWidth() * 0.16f, r.getY(),
                                       Colours::transparentWhite, r.getX() + r.getWidth() * 0.48f, r.getY() + r.getHeight() * 0.65f, false));
    g.fillRect (r);

    // Lower glass reflection
    g.setGradientFill (ColourGradient (Colours::transparentBlack, c.x, r.getY() + r.getHeight() * 0.50f,
                                       Colour (0x35000000), c.x, r.getBottom(), false));
    g.fillRect (r);
    g.restoreState();
}

/** Engraved metallic equipment badge. */
void paintBadge (Graphics& g, Rectangle<float> r, const String& text)
{
    // Outer drop shadow
    g.setColour (Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 3.0f);

    // Brushed metal badge plate
    g.setGradientFill (ColourGradient (Colour (0xff24282c), r.getX(), r.getY(),
                                       Colour (0xff101214), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);

    // Crisp metallic border
    g.setColour (Colours::white.withAlpha (0.28f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

    // Engraved lettering
    g.setFont (fnt (r.getHeight() * 0.62f));
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawText (text, r.translated (0.0f, 1.0f), Justification::centred);
    g.setColour (Colour (0xfff0e8d2));
    g.drawText (text, r, Justification::centred);
}

/** Photorealistic segmented LED ladder with smoked glass window and optical glow. */
void paintLadder (Graphics& g, Rectangle<float> r, float lit01, Colour low, Colour high, int segments)
{
    // Smoked acrylic bezel housing
    g.setColour (Colour (0xff080a0b));
    g.fillRoundedRectangle (r.expanded (2.5f), 3.0f);
    g.setColour (Colours::black.withAlpha (0.95f));
    g.drawRoundedRectangle (r.expanded (1.0f), 2.5f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (r.expanded (2.5f), 3.0f, 0.7f);

    const float h = r.getHeight() / (float) segments;
    const int lit = (int) std::round (jlimit (0.0f, 1.0f, lit01) * (float) segments);

    for (int i = 0; i < segments; ++i)
    {
        const float t = (float) i / (float) (segments - 1);
        const auto c = low.interpolatedWith (high, t);
        const Rectangle<float> seg (r.getX() + 1.2f, r.getBottom() - (float) (i + 1) * h + 0.8f, r.getWidth() - 2.4f, h - 1.6f);
        const bool on = (segments - i) <= lit;

        if (on)
        {
            // Saturated LED optical glow
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (seg.expanded (2.2f, 1.0f), 2.0f);

            // LED core body
            g.setGradientFill (ColourGradient (c.brighter (0.45f), seg.getX(), seg.getY(),
                                               c, seg.getX(), seg.getBottom(), false));
            g.fillRoundedRectangle (seg, 1.0f);

            // Bright center filament line
            g.setColour (Colours::white.withAlpha (0.65f));
            g.fillRect (seg.reduced (seg.getWidth() * 0.25f, seg.getHeight() * 0.35f));
        }
        else
        {
            // Unlit dark phosphor bar
            g.setColour (c.withMultipliedBrightness (0.15f).withAlpha (0.30f));
            g.fillRoundedRectangle (seg, 1.0f);
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

        const float pos = (float) s.valueToProportionOfLength (s.getValue());
        const float angle = a0 + pos * (a1 - a0);

        // ---- Skirt: heavy machined ring with scale marks
        const float skirtR = d * 0.50f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c).translated (0.0f, 2.0f));

        g.setGradientFill (ColourGradient (Colour (0xff383d42), c.x, c.y - skirtR,
                                           Colour (0xff0a0c0e), c.x, c.y + skirtR, false));
        g.fillEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.15f));
        g.drawEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c), 0.8f);

        KnobScale sc; sc.a0 = a0; sc.a1 = a1;
        paintScale (g, c, skirtR * 0.78f, sc, Colours::white.withAlpha (0.85f));

        // ---- Knob Body
        const float bodyR = d * 0.355f;
        auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);

        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillEllipse (body.expanded (3.0f).translated (0.0f, 3.0f));

        // Machined chrome collar catching top-left key light
        g.setGradientFill (ColourGradient (Colour (0xff82878d), body.getX(), body.getY(),
                                           Colour (0xff0e1012), body.getRight(), body.getBottom(), false));
        g.fillEllipse (body.expanded (2.6f));

        // Knob Face Cap
        auto capBody = body.reduced (bodyR * 0.05f);
        g.setGradientFill (ColourGradient (cap.brighter (0.45f), capBody.getX() + capBody.getWidth() * 0.30f,
                                           capBody.getY() + capBody.getHeight() * 0.10f,
                                           cap.darker (0.75f), capBody.getRight(), capBody.getBottom(), true));
        g.fillEllipse (capBody);

        g.saveState();
        Path clip; clip.addEllipse (capBody); g.reduceClipRegion (clip);

        const bool isSilver = (cap == pal::silverCap);
        if (isSilver)
        {
            // 1176 style concentric turned aluminum disc
            g.setColour (Colours::black.withAlpha (0.12f));
            for (float rad = capBody.getWidth() * 0.12f; rad < capBody.getWidth() * 0.48f; rad += 2.2f)
                g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), 0.7f);

            // Anisotropic cross reflections for spun metal
            Path fan;
            fan.addPieSegment (capBody, -0.6f, 0.6f, 0.0f);
            fan.addPieSegment (capBody, MathConstants<float>::pi - 0.6f, MathConstants<float>::pi + 0.6f, 0.0f);
            g.setColour (Colours::white.withAlpha (0.35f));
            g.fillPath (fan);
        }
        else
        {
            // Bakelite / collet cap specular sheen
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.48f), capBody.getX(), capBody.getY(),
                                               Colours::transparentWhite, capBody.getX(), capBody.getY() + capBody.getHeight() * 0.65f, false));
            g.fillRect (capBody.withHeight (capBody.getHeight() * 0.65f));

            // Hotspot reflection
            const Point<float> hi (capBody.getX() + capBody.getWidth() * 0.34f, capBody.getY() + capBody.getHeight() * 0.26f);
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.60f), hi.x, hi.y,
                                               Colours::transparentWhite, hi.x + capBody.getWidth() * 0.35f,
                                               hi.y + capBody.getHeight() * 0.35f, true));
            g.fillEllipse (Rectangle<float> (capBody.getWidth() * 0.75f, capBody.getHeight() * 0.75f).withCentre (hi));
        }

        // Bottom bounce light
        g.setGradientFill (ColourGradient (Colours::transparentWhite, capBody.getCentreX(), capBody.getBottom() - capBody.getHeight() * 0.34f,
                                           Colours::white.withAlpha (0.18f), capBody.getCentreX(), capBody.getBottom(), false));
        g.fillRect (capBody.withTop (capBody.getBottom() - capBody.getHeight() * 0.34f));

        // Realistic knurling grooves around the skirt
        g.setColour (Colours::black.withAlpha (0.18f));
        for (int i = 0; i < 56; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 56.0f;
            const Point<float> dir (std::sin (a), -std::cos (a));
            g.drawLine (Line<float> (c + dir * (capBody.getWidth() * 0.455f), c + dir * (capBody.getWidth() * 0.50f)), 0.65f);
        }
        g.restoreState();

        g.setColour (Colours::black.withAlpha (0.75f));
        g.drawEllipse (capBody, 1.0f);
        g.setColour (Colours::white.withAlpha (0.30f));
        g.drawEllipse (capBody.reduced (0.9f), 0.8f);

        // ---- Enamel-inlaid pointer line
        const float indLen = capBody.getHeight() * 0.36f;
        Path ind;
        ind.addRoundedRectangle (-1.5f, -capBody.getHeight() * 0.44f, 3.0f, indLen, 1.5f);

        // Pointer groove shadow
        g.setColour (Colours::black.withAlpha (0.50f));
        g.fillPath (ind, AffineTransform::rotation (angle).translated (c.x, c.y + 1.2f));

        // Filled enamel line
        g.setColour (s.findColour (Slider::thumbColourId).withAlpha (0.98f));
        g.fillPath (ind, AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    //-------------------------------------------------------------------- faders
    void drawLinearSlider (Graphics& g, int x, int y, int w, int h,
                           float pos, float minPos, float maxPos,
                           Slider::SliderStyle style, Slider& s) override
    {
        if (style == Slider::LinearHorizontal && (bool) s.getProperties()["eq560"])
            drawGraphicEqFader (g, x, y, w, h, pos, s);
        else if (style == Slider::LinearVertical) drawChromeFader (g, x, y, w, h, pos, minPos, maxPos, s);
        else if (style == Slider::LinearHorizontal) drawMasterBar (g, x, y, w, h, pos, s);
        else LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
    }

    static void drawGraphicEqFader (Graphics& g, int x, int y, int w, int h, float pos, Slider& s)
    {
        const float cy = (float) y + (float) h * 0.5f;
        const Rectangle<float> slot ((float) x - 9.0f, cy - 5.0f, (float) w + 18.0f, 10.0f);

        // Recessed milled channel
        g.setColour (Colour (0xff050a09));
        g.fillRoundedRectangle (slot, 5.0f);
        g.setColour (Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (slot, 5.0f, 1.0f);

        // Center felt dust strip
        g.setColour (Colour (0xff000000));
        g.drawLine (slot.getX() + 6.0f, cy, slot.getRight() - 6.0f, cy, 1.5f);

        // Metallic bottom lip reflection
        g.setColour (Colour (0xff70847e).withAlpha (0.40f));
        g.drawLine (slot.getX() + 4.0f, slot.getBottom() + 0.5f, slot.getRight() - 4.0f, slot.getBottom() + 0.5f, 0.8f);

        // Center detent alignment notch
        const float midX = slot.getCentreX();
        g.setColour (Colour (0xff36b8cf).withAlpha (0.75f));
        g.drawLine (midX, slot.getY() - 3.0f, midX, slot.getY(), 1.0f);
        g.drawLine (midX, slot.getBottom(), midX, slot.getBottom() + 3.0f, 1.0f);

        // 3D Slider Cap
        const auto cap = s.findColour (Slider::thumbColourId);
        const Rectangle<float> thumb (pos - 12.0f, cy - 6.5f, 24.0f, 13.0f);

        // Drop shadow into track
        g.setColour (Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 1.8f), 3.0f);

        // Main ribbed slider cap body
        g.setGradientFill (ColourGradient (cap.brighter (0.35f), thumb.getX(), thumb.getY(),
                                           cap.darker (0.45f), thumb.getX(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 2.5f);

        // Grip ribs across cap
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawHorizontalLine ((int) (thumb.getY() + 3.0f), thumb.getX() + 2.0f, thumb.getRight() - 2.0f);
        g.drawHorizontalLine ((int) (thumb.getBottom() - 3.0f), thumb.getX() + 2.0f, thumb.getRight() - 2.0f);

        // Center white index stripe
        g.setColour (Colours::white.withAlpha (0.95f));
        g.fillRect (thumb.getCentreX() - 1.0f, thumb.getY() + 1.0f, 2.0f, thumb.getHeight() - 2.0f);

        // Bevel highlight
        g.setColour (Colours::white.withAlpha (0.50f));
        g.drawRoundedRectangle (thumb.reduced (0.5f), 2.5f, 0.8f);
    }

    static void drawChromeFader (Graphics& g, int x, int y, int w, int h,
                                 float pos, float minPos, float maxPos, Slider& s)
    {
        const float cx = (float) x + (float) w * 0.5f;
        const Rectangle<float> track (cx - 4.5f, (float) y, 9.0f, (float) h);

        // Recessed milled channel
        g.setGradientFill (ColourGradient (Colour (0xff07080a), track.getX(), 0.0f, Colour (0xff1a1d20), track.getRight(), 0.0f, false));
        g.fillRoundedRectangle (track, 4.0f);
        g.setColour (Colours::black.withAlpha (0.95f));
        g.drawRoundedRectangle (track, 4.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.15f));
        g.drawLine (track.getX() + 0.5f, track.getY(), track.getX() + 0.5f, track.getBottom(), 1.0f);

        // Felt center travel guide
        g.setColour (Colour (0xff000000));
        g.fillRect (Rectangle<float> (cx - 1.0f, (float) y + 4.0f, 2.0f, (float) h - 8.0f));

        // Precision tick ladder
        g.setColour (Colours::white.withAlpha (0.40f));
        for (int i = 0; i <= 20; ++i)
        {
            const float ty = (float) y + (float) h * (float) i / 20.0f;
            const float len = (i % 5 == 0) ? 7.5f : 4.0f;
            g.drawLine (track.getX() - 2.0f, ty, track.getX() - 2.0f - len, ty, 1.0f);
            g.drawLine (track.getRight() + 2.0f, ty, track.getRight() + 2.0f + len, ty, 1.0f);
        }

        // Photorealistic sculpted console fader knob (P&G / SSL style)
        const float capH = 46.0f, capW = 30.0f;
        const Rectangle<float> cap (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);

        // Deep drop shadow onto channel
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (cap.translated (2.0f, 3.5f), 4.0f);

        // Machined chrome body
        g.setGradientFill (ColourGradient (Colour (0xfff6fdff), cap.getX() + 2.0f, cap.getY(),
                                           Colour (0xff708896), cap.getRight() - 2.0f, cap.getY(), false));
        g.fillRoundedRectangle (cap, 4.0f);

        // Ergonomic center finger concave well
        const Rectangle<float> well (cap.getX() + 4.0f, cap.getY() + 6.0f, cap.getWidth() - 8.0f, cap.getHeight() - 12.0f);
        g.setGradientFill (ColourGradient (Colour (0xff8ea4b0), well.getX(), well.getY(),
                                           Colour (0xff485a66), well.getX(), well.getBottom(), false));
        g.fillRoundedRectangle (well, 3.0f);

        // Bright white central index line
        g.setColour (Colours::white);
        g.fillRect (Rectangle<float> (cap.getCentreX() - 1.0f, cap.getY() + 2.0f, 2.0f, cap.getHeight() - 4.0f));

        // Knurl grooves on sides
        g.setColour (Colour (0xff20303a).withAlpha (0.50f));
        for (float gy = cap.getY() + 5.0f; gy < cap.getBottom() - 4.0f; gy += 4.0f)
            g.drawHorizontalLine ((int) gy, cap.getX() + 2.0f, cap.getX() + 4.0f);

        // Outer specular rim
        g.setColour (Colours::white.withAlpha (0.95f));
        g.drawRoundedRectangle (cap, 4.0f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (cap.expanded (0.8f), 5.0f, 1.2f);
        (void) s; (void) minPos; (void) maxPos;
    }

    static void drawMasterBar (Graphics& g, int x, int y, int w, int h, float pos, Slider&)
    {
        const Rectangle<float> track ((float) x, (float) y + (float) h * 0.5f - 4.0f, (float) w, 8.0f);
        g.setColour (Colour (0xff080a0b));
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (track, 3.0f, 1.0f);

        // Illuminated trim position track
        g.setGradientFill (ColourGradient (Colour (0xff3ba4c7), track.getX(), track.getY(),
                                           Colour (0xff65d2f0), pos, track.getY(), false));
        g.fillRoundedRectangle (track.withRight (jlimit (track.getX(), track.getRight(), pos)), 3.0f);

        // Machined metal master knob
        const Rectangle<float> thumb (pos - 9.0f, track.getCentreY() - 11.0f, 18.0f, 22.0f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (thumb.translated (1.0f, 2.0f), 3.0f);

        g.setGradientFill (ColourGradient (Colour (0xfff6feff), thumb.getX(), thumb.getY(),
                                           Colour (0xff7e96a4), thumb.getRight(), thumb.getY(), false));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (Colours::white.withAlpha (0.90f));
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);

        // Center line
        g.setColour (Colours::black.withAlpha (0.45f));
        g.drawLine (thumb.getCentreX(), thumb.getY() + 3.0f, thumb.getCentreX(), thumb.getBottom() - 3.0f, 1.5f);
        g.setColour (Colours::white);
        g.drawLine (thumb.getCentreX(), thumb.getY() + 4.0f, thumb.getCentreX(), thumb.getBottom() - 4.0f, 1.0f);
    }

    //-------------------------------------------------------------------- toggles / jewel lamps
    void drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        const auto c = b.findColour (ToggleButton::tickColourId);

        // Realistic jewel indicator lamp with chrome bezel
        const float ledR = jmin (7.0f, r.getHeight() * 0.30f);
        const bool ledFirst = b.getProperties().getWithDefault ("ledFirst", true);
        const Point<float> ledC (ledFirst ? r.getX() + ledR + 5.0f
                                          : r.getRight() - ledR - 5.0f, r.getCentreY());
        const Rectangle<float> cup (ledC.x - ledR - 3.5f, ledC.y - ledR - 3.5f,
                                    (ledR + 3.5f) * 2.0f, (ledR + 3.5f) * 2.0f);

        // Threaded chrome bezel
        g.setColour (Colour (0xff08090a));
        g.fillEllipse (cup);
        g.setColour (Colours::white.withAlpha (0.28f));
        g.drawEllipse (cup, 1.0f);

        const Rectangle<float> led (ledC.x - ledR, ledC.y - ledR, ledR * 2.0f, ledR * 2.0f);
        if (on)
        {
            // Radiant optical bloom
            const Rectangle<float> glow (ledC.x - ledR * 3.8f, ledC.y - ledR * 3.8f, ledR * 7.6f, ledR * 7.6f);
            g.setGradientFill (ColourGradient (c.withAlpha (0.60f), ledC.x, ledC.y,
                                               c.withAlpha (0.0f), ledC.x, ledC.y + ledR * 3.7f, true));
            g.fillEllipse (glow);
        }

        // Faceted lens
        g.setGradientFill (ColourGradient (on ? c.brighter (0.75f) : Colour (0xff42464a), led.getX() + ledR * 0.5f, led.getY() + ledR * 0.4f,
                                           on ? c.darker (0.35f) : Colour (0xff121415), led.getX() + ledR * 0.9f, led.getY() + ledR * 1.8f, true));
        g.fillEllipse (led);
        g.setColour (Colours::white.withAlpha (on ? 0.75f : 0.25f));
        g.drawEllipse (led, 0.8f);

        auto textArea = ledFirst ? r.withTrimmedLeft (ledC.x + ledR + 8.0f)
                                 : r.withTrimmedRight (r.getRight() - ledC.x + ledR + 8.0f);
        g.setFont (fnt (12.0f));
        g.setColour (Colours::black.withAlpha (0.75f));
        g.drawText (b.getButtonText(), textArea.translated (0, 1), Justification::centredLeft);
        g.setColour (b.findColour (ToggleButton::textColourId).withAlpha (highlighted ? 1.0f : 0.92f));
        g.drawText (b.getButtonText(), textArea, Justification::centredLeft);
    }

    //-------------------------------------------------------------------- buttons (1176 Shadow switch bank)
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        auto base = on ? b.findColour (TextButton::buttonOnColourId) : b.findColour (TextButton::buttonColourId);
        if (highlighted) base = base.brighter (0.12f);

        // Recessed switch well
        g.setColour (Colours::black.withAlpha (on ? 0.85f : 0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, on ? 0.5f : 1.8f), 3.0f);

        // Button body
        g.setGradientFill (ColourGradient (base.brighter (on ? 0.65f : 0.35f), 0.0f, r.getY(),
                                           base.darker (on ? 0.25f : 0.55f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);

        if (on)
        {
            // Backlit inner glow
            g.setColour (base.brighter (0.85f).withAlpha (0.60f));
            g.fillRoundedRectangle (r.reduced (1.2f).withHeight (r.getHeight() * 0.45f), 2.0f);
            g.setColour (base.brighter (0.95f).withAlpha (0.45f));
            g.drawRoundedRectangle (r.expanded (1.8f), 4.0f, 2.0f);
        }

        // Metallic bevel rim
        g.setColour (Colours::white.withAlpha (on ? 0.55f : 0.22f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setFont (fnt (14.0f));
        const bool on = b.getToggleState();
        g.setColour (on ? Colours::white : Colour (0xffa0b8cc));
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
        setName (StringArray { "Console", "Opto", "FET", "Limiter" }[id]);
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
        bypass.setBounds (content.getRight() - 84, plateBounds().getY() + 6, 80, 22);
        layoutContent (content);

        if (getWidth() > 0 && getHeight() > 0)
        {
            faceImage = Image (Image::ARGB, getWidth(), getHeight(), true);
            Graphics fg (faceImage);
            paintFace (fg);

            Image caps (Image::ARGB, getWidth(), getHeight(), true);
            Graphics cg (caps);
            paintFaceExtra (cg);
            captionLayer.image = caps;
            captionLayer.setBounds (0, 0, getWidth(), getHeight());
            captionLayer.setVisible (true);
            captionLayer.toBack();
        }
    }

    void cancelDrag() { dragging = false; }

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
    virtual void paintFaceBase (Graphics&) {}
    virtual void paintFaceExtra (Graphics&) {}
    virtual void paintMeter (Graphics&) {}

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
        const float angles[] { 0.45f, -0.85f, 1.15f, -0.35f };
        for (int i = 0; i < 4; ++i) paintScrew (g, pts[i], rad, angles[i] + (float) moduleId * 0.4f);
    }

    void paintFace (Graphics& g) { paintFaceBase (g); }

    Image faceImage;

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
//  Module 1 — ten-band 560 graphic EQ + console dynamics
//==============================================================================
class ConsolePanel : public ModulePanel
{
public:
    explicit ConsolePanel (RackStripProcessor& p)
        : ModulePanel (p, RackStripProcessor::Console, "con_",
                       Colour (0xff283732), Colour (0xff121d19), pal::engrave)
    {
        const char* ids[] { "eq_16k", "eq_8k", "eq_4k", "eq_2k", "eq_1k", "eq_500", "eq_250", "eq_125", "eq_63", "eq_31" };
        for (size_t i = 0; i < bands.size(); ++i)
        {
            auto& band = bands[i];
            band.slider.setSliderStyle (Slider::LinearHorizontal);
            band.slider.setName (prefix + ids[i]);
            band.slider.getProperties().set ("eq560", true);
            band.slider.setColour (Slider::thumbColourId, i == 5 ? Colour (0xffd82a2e) : Colour (0xffe4e8dc));
            band.slider.setTextValueSuffix (" dB");
            band.slider.setPopupDisplayEnabled (true, true, nullptr);
            band.slider.setDoubleClickReturnValue (true, 0.0);
            band.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, prefix + ids[i], band.slider);
            addAndMakeVisible (band.slider);
        }
        setupKnob (thr, "thresh", pal::blackCap);
        setupKnob (rat, "ratio", pal::blackCap);
        setupKnob (atk, "attack", pal::blackCap);
        setupKnob (rel, "release", pal::blackCap);
        setupKnob (out, "out", pal::blackCap);
        setupKnob (hpf, "hpf", pal::blackCap);

        for (auto* b : { &compLed, &gateLed })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (9001);
            b->setColour (ToggleButton::tickColourId, Colours::white);
            b->setColour (ToggleButton::textColourId, labelColour);
            b->getProperties().set ("ledFirst", false);
            addAndMakeVisible (b);
        }
        compLed.setToggleState (true, dontSendNotification);
    }

protected:
    void layoutContent (Rectangle<int> c) override
    {
        for (size_t i = 0; i < bands.size(); ++i)
            bands[i].slider.setBounds (c.getX() + 36, c.getY() + 44 + (int) i * 29, c.getWidth() - 36, 22);

        dynTitle = { c.getX(), c.getY() + 350, c.getWidth(), 20 };
        const int ledY = dynTitle.getBottom() + 4;
        compLed.setBounds (c.getCentreX() - 88, ledY, 66, 22);
        gateLed.setBounds (c.getCentreX() + 22, ledY, 66, 22);
        constexpr int knob = 54;
        const int col = c.getWidth() / 3;
        const int y = c.getY() + 400;
        place (thr, { c.getX() + col / 2 - knob / 2, y, knob, knob });
        place (rat, { c.getX() + col + col / 2 - knob / 2, y, knob, knob });
        place (atk, { c.getX() + col * 2 + col / 2 - knob / 2, y, knob, knob });
        place (rel, { c.getX() + col / 2 - knob / 2, y + 82, knob, knob });
        place (out, { c.getX() + col + col / 2 - knob / 2, y + 82, knob, knob });
        place (hpf, { c.getX() + col * 2 + col / 2 - knob / 2, y + 82, knob, knob });
        badge = { c.getX() + 4, c.getBottom() - 32, 74, 28 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "GRAPHIC EQ", { content.getX(), content.getY(), content.getWidth(), 18 }, 13.0f);
        auto& first = bands.front().slider;
        auto sliderBounds = first.getLookAndFeel().getSliderLayout (first).sliderBounds.translated (first.getX(), first.getY());
        const float x0 = (float) sliderBounds.getX();
        const float travel = (float) sliderBounds.getWidth();
        const float y0 = (float) first.getBounds().getCentreY();
        const float y1 = (float) bands.back().slider.getBounds().getCentreY();
        for (int i = 0; i <= 4; ++i)
        {
            const float x = x0 + travel * (float) i * 0.25f;
            g.setColour (Colour (0xff36b8cf).withAlpha (i == 2 ? 0.9f : 0.55f));
            g.drawLine (x, y0 - 3.0f, x, y1 + 3.0f, i == 2 ? 1.5f : 1.0f);
        }
        for (int i = 0; i < 3; ++i)
        {
            const char* scale[] { "-12", "0", "+12" };
            caption (g, scale[i], { roundToInt (x0 + travel * (float) i * 0.5f) - 18,
                                      content.getY() + 21, 36, 16 }, 12.0f);
        }
        const char* frequencies[] { "16K", "8K", "4K", "2K", "1K", "500", "250", "125", "63", "31" };
        for (size_t i = 0; i < bands.size(); ++i)
            caption (g, frequencies[i], { content.getX() - 5, bands[i].slider.getY() + 2, 34, 18 }, 12.0f, Justification::centredRight);

        g.setColour (Colours::white.withAlpha (0.18f));
        g.fillRect (content.getX(), dynTitle.getY() - 10, content.getWidth(), 1);
        caption (g, "Dynamics", dynTitle, 15.0f);
        caption (g, "Comp", { compLed.getX(), compLed.getY(), 40, 22 }, 12.0f);
        caption (g, "Gate", { gateLed.getX(), gateLed.getY(), 40, 22 }, 12.0f);
        knobCaption (g, thr, "Threshold");
        knobCaption (g, rat, "Ratio");
        knobCaption (g, atk, "Attack");
        knobCaption (g, rel, "Release");
        knobCaption (g, out, "Output");
        knobCaption (g, hpf, "HPF");
        paintBadge (g, badge.toFloat(), "560");
    }

private:
    void knobCaption (Graphics& g, const Knob& k, const String& text)
    {
        const auto b = k.slider.getBounds();
        caption (g, text, { b.getX() - 10, b.getBottom() + 4, b.getWidth() + 20, 16 }, 10.0f);
    }

    std::array<Knob, 10> bands;
    Knob hpf, out, thr, rat, atk, rel;
    ToggleButton compLed { "" }, gateLed { "" };
    Rectangle<int> dynTitle, badge;
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

        onBtn.setButtonText ("ON  (LIMIT)");
        onBtn.setClickingTogglesState (true);
        onBtn.setColour (ToggleButton::tickColourId, Colour (0xffffb000));
        onBtn.setColour (ToggleButton::textColourId, labelColour);
        onAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "opto_mode", onBtn);
        addAndMakeVisible (onBtn);

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
        const int vw = jmin (c.getWidth() - 26, 210);
        vu = { c.getCentreX() - vw / 2, c.getY() + 10, vw, vw * 3 / 4 };

        const int kw = 84;
        const int ky = vu.getBottom() + 54;
        place (gain, { c.getX() + 4,                 ky, kw, kw });
        place (peak, { c.getRight() - kw - 4,        ky, kw, kw });

        const int ty = ky + kw + 34;
        meterBtn.setBounds (c.getCentreX() - 46, ty, 92, 26);
        onBtn.setBounds (c.getCentreX() - 46, ty + 40, 92, 26);
    }

    void paintFaceBase (Graphics& g) override
    {
        Random rnd (99);
        for (int i = 0; i < 3000; ++i)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (rnd.nextFloat() * 0.07f));
            g.fillRect (rnd.nextFloat() * (float) getWidth(),
                        (float) kGripH + rnd.nextFloat() * (float) (getHeight() - kGripH), 1.3f, 1.3f);
        }
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "INPUT",  { gain.slider.getBounds().getCentreX() - 60, gain.slider.getY() - 44, 120, 18 }, 14.0f);
        caption (g, "GAIN",   { gain.slider.getBounds().getCentreX() - 60, gain.slider.getY() - 26, 120, 16 }, 14.0f);
        caption (g, "PEAK",   { peak.slider.getBounds().getCentreX() - 60, peak.slider.getY() - 44, 120, 18 }, 14.0f);
        caption (g, "REDUCTION", { peak.slider.getBounds().getCentreX() - 60, peak.slider.getY() - 26, 120, 16 }, 14.0f);

        paintBadge (g, Rectangle<float> ((float) content.getX() + 4.0f, (float) content.getBottom() - 34.0f, 78.0f, 30.0f), "LA-2A");
    }

    void paintMeter (Graphics& g) override
    {
        const auto r = vu.toFloat();
        // Warm glow bloom around round VU housing
        g.setGradientFill (ColourGradient (Colour (0x38ffd27a), r.getCentreX(), r.getCentreY(),
                                           Colour (0x00ffd27a), r.getCentreX(), r.getCentreY() - r.getHeight(), true));
        g.fillEllipse (r.expanded (20.0f));

        const float f = jlimit (0.0f, 1.0f, gr / 20.0f);
        paintVuDial (g, r, f, -0.95f, 0.95f, "", "", 10.0f, "REDUCTION", false);
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
        const int ky = c.getY() + 34;
        place (input,  { c.getX() + 2,               ky, 88, 88 });
        place (output, { c.getRight() - 90,         ky, 88, 88 });

        ratioLabel = { c.getX(), ky + 104, c.getWidth(), 20 };
        const int bw = (c.getWidth() - 8 - 4 * 5) / 5;
        for (int i = 0; i < 5; ++i)
            ratioBtns[(size_t) i].setBounds (c.getX() + 4 + i * (bw + 5), ratioLabel.getBottom() + 2, bw, 26);

        meter = { c.getX() + 10, ratioBtns[0].getBounds().getBottom() + 16, c.getWidth() - 20, 104 };

        const int ay = meter.getBottom() + 34;
        place (atk, { c.getX() + 8,        ay, 78, 78 });
        place (rel, { c.getRight() - 86,  ay, 78, 78 });
        grLabel = { c.getX() + c.getWidth() / 2 - 40, ay + 44, 80, 18 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "INPUT",  { input.slider.getBounds().getCentreX() - 50, input.slider.getY() - 20, 100, 18 }, 13.0f);
        caption (g, "OUTPUT", { output.slider.getBounds().getCentreX() - 50, output.slider.getY() - 20, 100, 18 }, 13.0f);
        caption (g, "RATIO",  ratioLabel, 14.0f);
        caption (g, "ATTACK", { atk.slider.getBounds().getCentreX() - 50, atk.slider.getY() - 16, 100, 18 }, 13.0f);
        caption (g, "RELEASE",{ rel.slider.getBounds().getCentreX() - 50, rel.slider.getY() - 16, 100, 18 }, 13.0f);

        // Vintage UREI-style wordmark in the top centre
        engraveText (g, "urei", { content.getX(), content.getY() - 6, content.getWidth(), 22 },
                     Justification::centred, 21.0f, Colour (0xffe8e8e8), false);

        paintBadge (g, { (float) content.getCentreX() - 44.0f, (float) content.getBottom() - 30.0f, 88.0f, 26.0f }, "1176");
    }

    void paintMeter (Graphics& g) override
    {
        const auto r = meter.toFloat();
        // Rectangular Simpson-style meter with warm backlighting
        g.setColour (Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (r.expanded (4.0f), 4.0f);
        g.setColour (Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (r.expanded (4.0f), 4.0f, 1.0f);

        const float f = jlimit (0.0f, 1.0f, gr / 20.0f);
        paintVuDial (g, r, f, -0.95f, 0.95f, "dB", "GR", 9.0f, {}, true);
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
        autoGain.setName ("Auto Gain");
        autoGain.setButtonText ("AUTO GAIN");
        autoGain.setColour (ToggleButton::tickColourId, Colour (0xffffb000));
        autoGain.setColour (ToggleButton::textColourId, labelColour);
        autoGainAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "lim_auto_gain", autoGain);
        addAndMakeVisible (autoGain);
    }

protected:
    void layoutContent (Rectangle<int> c) override
    {
        const int kh = 70;
        const int kw = c.getWidth() / 3;
        const int ky = c.getY() + 26;
        place (gain,    { c.getX(),                ky, kw, kh });
        place (ceiling, { c.getX() + kw,           ky, kw, kh });
        place (release, { c.getX() + kw * 2,       ky, kw, kh });

        autoGain.setBounds (c.getCentreX() - 58, ky + kh + 10, 116, 24);
        const int fy = ky + kh + 52;
        const int fh = c.getBottom() - fy - 52;
        ladderA = { c.getX() + 24, fy + 22, 26, fh };
        ladderB = { c.getRight() - 50, fy + 22, 26, fh };
        fader.setBounds (c.getCentreX() - 26, fy, 52, fh + 22);
        scaleRect = { c.getCentreX() - 62, fy + 22, 124, fh };
        enhanceLabel = { c.getX(), fader.getBottom() + 2, c.getWidth(), 20 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "THRESHOLD", { gain.slider.getBounds().getX(), gain.slider.getY() - 19, gain.slider.getBounds().getWidth(), 17 }, 9.5f);
        caption (g, "CEILING",   { ceiling.slider.getBounds().getX(), ceiling.slider.getY() - 19, ceiling.slider.getBounds().getWidth(), 17 }, 9.5f);
        caption (g, "RELEASE",   { release.slider.getBounds().getX(), release.slider.getY() - 19, release.slider.getBounds().getWidth(), 17 }, 9.5f);
        caption (g, "ENHANCE", enhanceLabel, 15.0f);

        // dB scale beside fader
        g.setFont (fnt (10.0f, false));
        for (int i = 0; i <= 6; ++i)
        {
            const float t = (float) i / 6.0f;
            const float y = (float) scaleRect.getY() + (float) scaleRect.getHeight() * t;
            const int db = (int) std::round (12.0f - 24.0f * t);
            g.setColour (Colours::black.withAlpha (0.75f));
            g.drawText (String (db), Rectangle<float> ((float) scaleRect.getX(), y - 7.0f, 26.0f, 14.0f), Justification::centredRight);
            g.setColour (Colours::white.withAlpha (0.6f));
            g.drawText (String (db), Rectangle<float> ((float) scaleRect.getX(), y - 8.0f, 26.0f, 14.0f), Justification::centredRight);
        }
        for (int i = 0; i <= 12; ++i)
        {
            const float y = (float) scaleRect.getY() + (float) scaleRect.getHeight() * (float) i / 12.0f;
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawLine ((float) scaleRect.getCentreX() + 14.0f, y, (float) scaleRect.getCentreX() + 22.0f, y, 1.0f);
            g.drawLine ((float) scaleRect.getCentreX() - 22.0f, y, (float) scaleRect.getCentreX() - 14.0f, y, 1.0f);
        }

        // Slots beside ladders
        for (auto s : { Rectangle<int> (ladderA.getX() - 14, ladderA.getY() - 6, 4, ladderA.getHeight() + 12),
                        Rectangle<int> (ladderB.getRight() + 10, ladderB.getY() - 6, 4, ladderB.getHeight() + 12) })
        {
            g.setColour (Colour (0xff080a0b));
            g.fillRoundedRectangle (s.toFloat(), 2.0f);
            g.setColour (Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (s.toFloat(), 2.0f, 1.0f);
        }
    }

    void paintMeter (Graphics& g) override
    {
        const float red = jlimit (0.0f, 1.0f, gr / 14.0f);
        paintLadder (g, ladderA.toFloat(), red * 0.85f + 0.06f, Colour (0xffc06a10), Colour (0xffffd23a), 28);
        paintLadder (g, ladderB.toFloat(), juce::jlimit (0.0f, 1.0f, 0.30f + red * 0.55f),
                     Colour (0xff17b04a), Colour (0xff5fe1f5), 28);

        engraveText (g, "-" + String (gr, 1) + " dB", { ladderA.getX() - 18, ladderA.getBottom() + 8, ladderA.getWidth() + 36, 16 },
                     Justification::centred, 11.0f, labelColour, false);
    }

private:
    Knob gain, ceiling, release;
    Slider fader;
    ToggleButton autoGain;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt;
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
    addAndMakeVisible (canvas);
    canvas.setBounds (0, 0, kEditorW, kEditorH);

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
        canvas.addAndMakeVisible (*panel);
    }

    // Host-automatable output trim after the complete module chain
    masterFader.setName ("Master level");
    masterFader.setTextValueSuffix (" dB");
    masterFader.setPopupDisplayEnabled (true, true, this);
    masterFader.setDoubleClickReturnValue (true, 0.0);
    masterFader.setColour (Slider::thumbColourId, Colours::white);
    canvas.addAndMakeVisible (masterFader);
    masterAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master_level", masterFader);

    setResizable (true, true);
    setResizeLimits (864, 608, 1620, 1140);
    getConstrainer()->setFixedAspectRatio ((double) kEditorW / kEditorH);
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
    const int pw = (kEditorW - kMargin * 2 - kGap * 3) / 4;
    const int top = kHeaderH + 4;
    return { kMargin + slot * (pw + kGap), top, pw, kEditorH - top - kMargin };
}

int RackStripEditor::slotForCentreX (int x) const
{
    const int pw = (kEditorW - kMargin * 2 - kGap * 3) / 4;
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
    draggingId = -1;
    previewOrder = proc.getOrder();
    for (auto& panel : panels) panel->cancelDrag();
    layoutFor (previewOrder, -1, false);
    masterFader.setBounds (kEditorW - kMargin - 160, kHeaderH / 2 - 12, 148, 24);

    const float scale = jmax (0.01f, jmin ((float) getWidth() / kEditorW, (float) getHeight() / kEditorH));
    canvasTransform = AffineTransform::scale (scale).translated (
        ((float) getWidth() - kEditorW * scale) * 0.5f,
        ((float) getHeight() - kEditorH * scale) * 0.5f);
    canvas.setTransform (canvasTransform);
    repaint();
}

void RackStripEditor::timerCallback()
{
    for (auto& p : panels) p->tick();
}

void RackStripEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff060708));
    g.saveState();
    g.addTransform (canvasTransform);
    const Rectangle<float> full (0.0f, 0.0f, (float) kEditorW, (float) kEditorH);

    // 1. Heavy outer 19" rack chassis with powder-coated texture
    g.setGradientFill (ColourGradient (pal::chassisTop, 0.0f, 0.0f, pal::chassisBottom, 0.0f, full.getBottom(), false));
    g.fillRoundedRectangle (full.reduced (2.0f), 8.0f);

    // Subtle rack stippled texture
    Random rnd (42);
    for (int i = 0; i < 4000; ++i)
    {
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (rnd.nextFloat() * 0.025f));
        g.fillRect (rnd.nextFloat() * full.getWidth(), rnd.nextFloat() * full.getHeight(), 1.2f, 1.2f);
    }

    // 2. Inner recessed rack bay holding the modules
    const Rectangle<float> inner (4.0f, (float) kHeaderH + 2.0f, full.getWidth() - 8.0f, full.getHeight() - (float) kHeaderH - 8.0f);
    g.setGradientFill (ColourGradient (Colour (0xff121517), 0.0f, inner.getY(), Colour (0xff040506), 0.0f, inner.getBottom(), false));
    g.fillRoundedRectangle (inner, 6.0f);

    // 3. Machined brushed aluminium header bar
    const Rectangle<float> header (4.0f, 6.0f, full.getWidth() - 8.0f, (float) kHeaderH - 6.0f);
    g.setGradientFill (ColourGradient (pal::headerBarTop, 0.0f, header.getY(),
                                       pal::headerBarLow, 0.0f, header.getBottom(), false));
    g.fillRoundedRectangle (header, 6.0f);

    // Header brushed horizontal streaks
    for (float y = header.getY() + 1.0f; y < header.getBottom(); y += 1.5f)
    {
        g.setColour (Colours::white.withAlpha (0.015f));
        g.drawHorizontalLine ((int) y, header.getX() + 6.0f, header.getRight() - 6.0f);
    }

    // Header machined bevels
    g.setColour (Colours::white.withAlpha (0.16f));
    g.drawLine (header.getX() + 6.0f, header.getY() + 1.0f, header.getRight() - 6.0f, header.getY() + 1.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawLine (header.getX() + 6.0f, header.getBottom(), header.getRight() - 6.0f, header.getBottom(), 1.2f);

    // 4. Branding on Header: "zychanstrip" + zynamfx-style ZY monogram
    const float logoX = header.getX() + 16.0f;
    const float logoY = header.getY() + 7.0f;

    // "zychanstrip" wordmark (matching zynamfx typographic style)
    g.setFont (fnt (18.0f, true));
    // Engraved drop shadow
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText ("zychanstrip", Rectangle<float> (logoX, logoY + 1.0f, 120.0f, 22.0f), Justification::centredLeft);
    // Crisp white lettering
    g.setColour (Colours::white);
    g.drawText ("zychanstrip", Rectangle<float> (logoX, logoY, 120.0f, 22.0f), Justification::centredLeft);

    // Iconic ZY monogram logo (resembling the logo on zynamfx)
    const Rectangle<float> zyMarkBox (logoX + 118.0f, logoY + 1.0f, 44.0f, 20.0f);
    drawZyLogo (g, zyMarkBox);

    // Subtitle badge
    g.setFont (fnt (9.0f, false));
    g.setColour (pal::engraveDim);
    g.drawText ("MASTERING & TRACKING STRIP", Rectangle<float> (zyMarkBox.getRight() + 12.0f, logoY + 3.0f, 180.0f, 18.0f), Justification::centredLeft);

    // Power jewel LED on header
    const Point<float> pwrC (full.getWidth() * 0.5f, header.getCentreY());
    g.setColour (Colour (0xff08090a));
    g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (pwrC));
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (pwrC), 0.8f);
    // Glowing cyan jewel
    g.setGradientFill (ColourGradient (Colour (0x6057d4c2), pwrC.x, pwrC.y, Colour (0x0057d4c2), pwrC.x + 10.0f, pwrC.y + 10.0f, true));
    g.fillEllipse (Rectangle<float> (18.0f, 18.0f).withCentre (pwrC));
    g.setGradientFill (ColourGradient (Colour (0xffc4fff7), pwrC.x - 1.0f, pwrC.y - 1.0f, Colour (0xff28a896), pwrC.x + 2.0f, pwrC.y + 2.0f, false));
    g.fillEllipse (Rectangle<float> (6.5f, 6.5f).withCentre (pwrC));

    // Master level title + calibrated scale ticks
    g.setFont (fnt (12.0f, true));
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText ("MASTER", Rectangle<float> (full.getWidth() - 230.0f, header.getY() + 1.0f, 62.0f, header.getHeight()), Justification::centredRight);
    g.setColour (Colour (0xffc8ccd0));
    g.drawText ("MASTER", Rectangle<float> (full.getWidth() - 230.0f, header.getY(), 62.0f, header.getHeight()), Justification::centredRight);

    // 5. Plate shadows so the faceplates read as physically raised hardware
    for (int s = 0; s < 4; ++s)
    {
        const auto r = slotBounds (s);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (r.toFloat().withTrimmedTop (kGripH).translated (0.0f, 4.0f), 6.0f);
    }
    g.restoreState();
}

void RackStripEditor::paintOverChildren (Graphics& g)
{
    g.saveState();
    g.addTransform (canvasTransform);
    const Rectangle<float> full (0.0f, 0.0f, (float) kEditorW, (float) kEditorH);

    // Outer 19" rack ears bevel
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (full.reduced (2.0f), 8.0f, 1.2f);
    g.setColour (Colours::black.withAlpha (0.95f));
    g.drawRoundedRectangle (full.reduced (0.5f), 9.0f, 1.5f);
    g.restoreState();
}
