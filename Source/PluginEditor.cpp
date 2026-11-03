#include "PluginEditor.h"
#include "MeterPanel.h"
#include "UiStyle.h"
#include <BinaryData.h>


using namespace juce;

// Version / channel label rendered after the header wordmark. Supplied by CMake
// (ZYCHANSTRIP_VERSION_LABEL); falls back to the bundle version if it is ever missing.
#ifndef ZYCHANSTRIP_VERSION_LABEL
 #define ZYCHANSTRIP_VERSION_LABEL "v" JucePlugin_VersionString
#endif

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
const Colour limTop         { 0xff3b4a63 };
const Colour limLow         { 0xff1b2333 };
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
    // JUCE 9 removed Drawable::createFromSVG (const XmlElement&); createFromSVGString
    // parses the markup directly and returns null when the document is invalid.
    static std::unique_ptr<Drawable> drawable = []() -> std::unique_ptr<Drawable> {
        return Drawable::createFromSVGString (kZyMarkSvg);
    }();
    if (drawable != nullptr)
        drawable->drawWithin (g, bounds, RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize, 1.0f);
}

/** Authentic API twin op-amp chevron logo */
void paintApiLogo (Graphics& g, Rectangle<float> area, Colour col)
{
    g.setColour (col);
    const float x = area.getX(), y = area.getY(), w = area.getWidth(), h = area.getHeight();
    // Top bar
    g.fillRect (x, y + h * 0.22f, w * 0.90f, 1.8f);
    // Bottom bar
    g.fillRect (x, y + h * 0.74f, w * 0.90f, 1.8f);
    // Left op-amp chevron
    Path tri1;
    tri1.startNewSubPath (x + w * 0.15f, y + h * 0.08f);
    tri1.lineTo (x + w * 0.52f, y + h * 0.48f);
    tri1.lineTo (x + w * 0.15f, y + h * 0.88f);
    tri1.closeSubPath();
    g.fillPath (tri1);
    // Right op-amp chevron
    Path tri2;
    tri2.startNewSubPath (x + w * 0.42f, y + h * 0.08f);
    tri2.lineTo (x + w * 0.79f, y + h * 0.48f);
    tri2.lineTo (x + w * 0.42f, y + h * 0.88f);
    tri2.closeSubPath();
    g.fillPath (tri2);
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

Point<float> knobDirection (float angle)
{
    return { std::sin (angle), -std::cos (angle) };
}

void paintScale (Graphics& g, Point<float> centre, float radius, const KnobScale& sc, Colour c,
                 float scale = 1.0f)
{
    const int total = sc.minorTicks ? (sc.majorTicks - 1) * 4 + 1 : sc.majorTicks;
    for (int i = 0; i < total; ++i)
    {
        const bool major = (i % 4) == 0;
        const float a = sc.a0 + (sc.a1 - sc.a0) * (float) i / (float) (total - 1);
        const Point<float> dir = knobDirection (a);
        const float len = (major ? 6.5f : 3.5f) * scale;
        const Point<float> shadowOffset (0.9f * scale, 1.25f * scale);

        // Cast tick shadow in the same light direction as the knob: lower-right.
        g.setColour (Colours::black.withAlpha (0.50f));
        g.drawLine (Line<float> (centre + dir * radius + shadowOffset,
                                 centre + dir * (radius + len) + shadowOffset),
                    (major ? 1.5f : 1.0f) * scale);

        // Crisp tick mark
        g.setColour (c.withAlpha (major ? 0.95f : 0.58f));
        g.drawLine (Line<float> (centre + dir * radius, centre + dir * (radius + len)),
                    (major ? 1.5f : 1.0f) * scale);
    }
}

/** Authentic ANSI C16.5 / IEEE 152 broadcast VU meter ballistic deflection.
    Returns normalized deflection (0.08 at -20 VU, 0.90 at 0 VU, 1.0 at +3 VU). */
inline float vuDeflectionForDb (float db)
{
    if (db >= 0.0f)
        return jlimit (0.0f, 1.0f, 0.90f + 0.10f * (db / 3.0f));
    const float v = std::pow (10.0f, db / 20.0f);
    return jlimit (0.05f, 1.0f, v * 0.90f);
}

/** Authentic large circular vintage VU meter (Teletronix LA-2A style).
    The scale arc is dropped and widened so it sweeps the whole upper half of the glass,
    the broadcast numerals ride just outside it and the three legends are spread from the
    top of the face down to the needle pivot, leaving no dead paper at the bottom. */
void paintOptoVuMeter (Graphics& g, Rectangle<float> r, float deflection)
{
    const auto c = r.getCentre();
    const float R_meter = r.getWidth() * 0.5f;
    const float H = r.getHeight();

    // 1. Heavy moulded black bakelite bezel with quadrant mounting screws
    const Rectangle<float> bezel = r.expanded (4.0f);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.fillEllipse (bezel.translated (0.0f, 3.5f));

    g.setGradientFill (ColourGradient (Colour (0xff42464a), bezel.getX(), bezel.getY(),
                                       Colour (0xff101214), bezel.getRight(), bezel.getBottom(), false));
    g.fillEllipse (bezel);

    // Bezel specular highlights
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawEllipse (bezel.reduced (0.8f), 1.2f);
    g.setColour (Colour (0xff0a0c0e));
    g.drawEllipse (bezel.reduced (3.0f), 1.2f);

    // Stepped inner brass retaining rim
    const Rectangle<float> brassRim = r.expanded (0.5f);
    g.setGradientFill (ColourGradient (Colour (0xffcda95e), brassRim.getX(), brassRim.getY(),
                                       Colour (0xff685222), brassRim.getRight(), brassRim.getBottom(), false));
    g.drawEllipse (brassRim, 2.0f);

    // Bezel mounting screws at 45, 135, 225, 315 degrees
    for (int i = 0; i < 4; ++i)
    {
        const float sa = MathConstants<float>::pi * (0.25f + 0.5f * (float) i);
        const Point<float> sPos = c + Point<float> (std::sin (sa), -std::cos (sa)) * (R_meter + 1.6f);
        const auto sRect = Rectangle<float> (6.5f, 6.5f).withCentre (sPos);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillEllipse (sRect.translated (0.0f, 0.8f));
        g.setGradientFill (ColourGradient (Colour (0xff9aa0a6), sRect.getX(), sRect.getY(),
                                           Colour (0xff32363a), sRect.getRight(), sRect.getBottom(), false));
        g.fillEllipse (sRect);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawEllipse (sRect, 0.6f);
        // Slotted screw head groove
        const float slotAng = sa + 0.5f;
        const Point<float> slotDir (std::sin (slotAng), -std::cos (slotAng));
        g.setColour (Colour (0xff121416));
        g.drawLine (Line<float> (sPos - slotDir * 2.2f, sPos + slotDir * 2.2f), 1.0f);
    }

    // 2. Recessed dial face paper with authentic incandescent backlighting
    g.saveState();
    Path clip;
    clip.addEllipse (r);
    g.reduceClipRegion (clip);

    // Warm aged ivory paper
    g.setGradientFill (ColourGradient (Colour (0xfffbf5e2), c.x, r.getY(),
                                       Colour (0xffedd6a6), c.x, r.getBottom(), false));
    g.fillEllipse (r);

    // Incandescent bulb glow from top/center
    g.setGradientFill (ColourGradient (Colour (0x62ffd97d), c.x, r.getY() + r.getHeight() * 0.28f,
                                       Colours::transparentWhite, c.x, r.getBottom(), true));
    g.fillEllipse (r);

    // Recessed bezel shadow cast onto paper
    g.setGradientFill (ColourGradient (Colour (0x55000000), c.x, r.getY(),
                                       Colours::transparentBlack, c.x, r.getY() + 22.0f, false));
    g.fillRect (r);
    g.setColour (Colour (0x30000000));
    g.drawEllipse (r.reduced (0.8f), 1.8f);

    // 3. Dial scale arcs & markings — dropped pivot, wider sweep
    const Point<float> pivot (c.x, r.getY() + H * 0.80f);
    const float R = H * 0.55f;
    const float a0 = -0.90f; // approx -51.6 deg
    const float a1 =  0.90f; // approx +51.6 deg
    auto angleForT = [&] (float t) { return a0 + (a1 - a0) * t; };
    auto dirForA = [] (float a) { return Point<float> (std::sin (a), -std::cos (a)); };

    // Broadcast numerals sit outside the arc; keep them inside the glass rim near the ends
    const float safeR = R_meter - 13.0f;
    auto numeralPos = [&] (float radius, const Point<float>& dir)
    {
        Point<float> p = pivot + dir * radius;
        const Point<float> fromC = p - c;
        const float distC = fromC.getDistanceFromOrigin();
        if (distC > safeR && distC > 0.0f) p = c + fromC * (safeR / distC);
        return p;
    };

    // Upper VU arc: black from -20 to 0 VU (t=0.08..0.90)
    Path blackArc;
    blackArc.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleForT (0.08f), angleForT (0.90f), true);
    g.setColour (pal::needleInk);
    g.strokePath (blackArc, PathStrokeType (1.3f));

    // Upper VU arc: RED overload section from 0 to +3 VU (t=0.90..1.00)
    Path redArc;
    redArc.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleForT (0.90f), angleForT (1.00f), true);
    g.setColour (Colour (0xffc82418));
    g.strokePath (redArc, PathStrokeType (3.0f));

    // Secondary modulation percentage arc below
    const float R_sec = R - 10.0f;
    Path secArc;
    secArc.addCentredArc (pivot.x, pivot.y, R_sec, R_sec, 0.0f, angleForT (0.08f), angleForT (0.90f), true);
    g.setColour (pal::needleInk.withAlpha (0.65f));
    g.strokePath (secArc, PathStrokeType (0.85f));

    // Major VU markings
    const struct { float db; const char* txt; } majorVu[] =
    {
        { -20.0f, "-20" }, { -10.0f, "-10" }, { -7.0f, "-7" }, { -5.0f, "-5" },
        { -3.0f, "-3" },   { -2.0f, "-2" },   { -1.0f, "-1" }, { 0.0f, "0" },
        { 1.0f, "+1" },    { 2.0f, "+2" },    { 3.0f, "+3" }
    };
    for (const auto& m : majorVu)
    {
        const float t = vuDeflectionForDb (m.db);
        const float a = angleForT (t);
        const auto d = dirForA (a);
        const bool isRed = (m.db > 0.0f);

        // Tick mark
        g.setColour (isRed ? Colour (0xffc82418) : pal::needleInk);
        g.drawLine (Line<float> (pivot + d * R, pivot + d * (R + (isRed ? 6.5f : 5.5f))), isRed ? 1.6f : 1.2f);

        // Numerals: black scale rides outside the arc, the overload group stacks inward
        // at the red end so it stays inside the glass instead of clipping on the rim.
        g.setFont (fnt (isRed ? 7.0f : 8.0f, true));
        const Point<float> labelPos = isRed
            ? pivot + d * (R - 8.0f - (m.db - 1.0f) * 9.0f)
            : numeralPos (R + 9.0f, d);
        g.drawText (m.txt, Rectangle<float> (22.0f, 10.0f).withCentre (labelPos), Justification::centred, false);
    }

    // Minor intermediate VU ticks
    const float minorVu[] = { -15.0f, -9.0f, -8.0f, -6.0f, -4.0f, -2.5f, -1.5f, -0.5f, 0.5f, 1.5f, 2.5f };
    for (float db : minorVu)
    {
        const float t = vuDeflectionForDb (db);
        const float a = angleForT (t);
        const auto d = dirForA (a);
        const bool isRed = (db > 0.0f);
        g.setColour (isRed ? Colour (0xffc82418) : pal::needleInk.withAlpha (0.70f));
        g.drawLine (Line<float> (pivot + d * R, pivot + d * (R + 3.0f)), 0.8f);
    }

    // Lower modulation percentage scale (0 to 100%)
    const int pcts[] = { 0, 20, 40, 60, 80, 100 };
    for (int p : pcts)
    {
        const float t = ((float) p / 100.0f) * 0.90f;
        const float a = angleForT (t);
        const auto d = dirForA (a);

        g.setColour (pal::needleInk.withAlpha (0.75f));
        g.drawLine (Line<float> (pivot + d * (R_sec - 3.5f), pivot + d * R_sec), 0.85f);

        g.setFont (fnt (7.0f, false));
        // the 100 % mark would sit under the red overload block — blank it, the needle covers it
        const Point<float> pPos = pivot + d * (R_sec - 10.0f);
        g.drawText (String (p), Rectangle<float> (18.0f, 8.0f).withCentre (pPos), Justification::centred, false);
    }

    // Typography: spread from the top of the face down to the pivot
    g.setFont (fnt (9.5f, true));
    g.setColour (pal::needleInk.withAlpha (0.78f));
    g.drawText ("TELETRONIX", Rectangle<float> (110.0f, 12.0f).withCentre ({ c.x, r.getY() + H * 0.13f }), Justification::centred, false);

    g.setFont (Font (FontOptions ("Georgia", 20.0f, Font::bold)));
    g.setColour (pal::needleInk.withAlpha (0.92f));
    g.drawText ("VU", Rectangle<float> (60.0f, 26.0f).withCentre ({ c.x, r.getY() + H * 0.55f }), Justification::centred, false);

    g.setFont (fnt (7.5f, true));
    g.setColour (pal::needleInk.withAlpha (0.80f));
    g.drawText ("dB", Rectangle<float> (18.0f, 10.0f).withCentre (numeralPos (R + 9.0f, dirForA (angleForT (-0.05f)))), Justification::centred, false);

    g.setFont (fnt (11.0f, true));
    g.setColour (pal::needleInk.withAlpha (0.88f));
    g.drawText ("REDUCTION", Rectangle<float> (120.0f, 14.0f).withCentre ({ c.x, r.getY() + H * 0.70f }), Justification::centred, false);

    // 4. Knife-edge needle with drop shadow and counter-weight tail
    const float needleAngle = a0 + (a1 - a0) * jlimit (0.02f, 1.0f, deflection);

    // Needle drop shadow
    const Point<float> sOff (2.2f, 2.5f);
    Path sNeedle;
    sNeedle.startNewSubPath (-1.2f, 0.0f);
    sNeedle.lineTo (-0.4f, -(R + 5.0f));
    sNeedle.lineTo ( 0.4f, -(R + 5.0f));
    sNeedle.lineTo ( 1.2f, 0.0f);
    sNeedle.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.24f));
    g.fillPath (sNeedle, AffineTransform::rotation (needleAngle).translated (pivot.x + sOff.x, pivot.y + sOff.y));

    // Needle blade
    Path needle;
    needle.startNewSubPath (-1.1f, 0.0f);
    needle.lineTo (-0.35f, -(R + 5.0f));
    needle.lineTo ( 0.35f, -(R + 5.0f));
    needle.lineTo ( 1.1f, 0.0f);
    needle.closeSubPath();
    g.setColour (pal::needleInk);
    g.fillPath (needle, AffineTransform::rotation (needleAngle).translated (pivot.x, pivot.y));

    // Counter-weight tail
    Path tail;
    tail.addEllipse (Rectangle<float> (5.5f, 5.5f).withCentre ({ 0.0f, 9.0f }));
    tail.addRectangle (-0.9f, 0.0f, 1.8f, 9.5f);
    g.fillPath (tail, AffineTransform::rotation (needleAngle).translated (pivot.x, pivot.y));

    // Pivot Hub
    g.setColour (Colours::black.withAlpha (0.75f));
    g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (pivot));
    g.setGradientFill (ColourGradient (Colour (0xffe2bf72), pivot.x - 3.0f, pivot.y - 3.0f,
                                       Colour (0xff825d18), pivot.x + 3.0f, pivot.y + 3.0f, false));
    g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (pivot));
    g.setColour (Colours::white.withAlpha (0.50f));
    g.drawEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (pivot), 0.8f);

    // 5. Curved glass dome reflection
    Path glare;
    glare.addCentredArc (c.x, c.y, R_meter * 0.90f, R_meter * 0.90f, 0.0f, -2.1f, -0.4f, false);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), r.getX() + r.getWidth() * 0.22f, r.getY() + 10.0f,
                                       Colours::transparentWhite, c.x, c.y, false));
    g.strokePath (glare, PathStrokeType (16.0f));

    g.setGradientFill (ColourGradient (Colours::transparentWhite, c.x, r.getY() + r.getHeight() * 0.40f,
                                       Colour (0x28000000), c.x, r.getBottom(), false));
    g.fillEllipse (r);

    g.restoreState();
}

/** Authentic rectangular vintage Simpson 29T meter (UREI 1176 style).
    The illuminated dial window fills the moulded housing edge to edge — only the glass lip
    and the moulded apron remain dark — with the signature lower bakelite mask, the ZERO ADJ
    calibration screw re-centred on the expanded window and the long sweeping needle. */
void paintFetVuMeter (Graphics& g, Rectangle<float> r, float deflection)
{
    const auto c = r.getCentre();

    // 1. Heavy moulded rectangular bakelite housing with drop shadow
    g.setColour (Colours::black.withAlpha (0.75f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.5f), 5.0f);

    g.setGradientFill (ColourGradient (Colour (0xff3e4044), r.getX(), r.getY(),
                                       Colour (0xff111215), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);

    // 2. Illuminated dial window: the housing minus the glass lip only
    const Rectangle<float> inner = r.reduced (1.5f);

    g.saveState();
    Path winClip;
    winClip.addRoundedRectangle (inner, 3.5f);
    g.reduceClipRegion (winClip);

    // Warm aged buff dial paper
    g.setGradientFill (ColourGradient (Colour (0xfffaf4e1), c.x, inner.getY(),
                                       Colour (0xffead4a0), c.x, inner.getBottom(), false));
    g.fillRect (inner);

    // Incandescent bulb illumination from top
    g.setGradientFill (ColourGradient (Colour (0x65ffd872), c.x, inner.getY() + 14.0f,
                                       Colours::transparentWhite, c.x, inner.getBottom(), true));
    g.fillRect (inner);

    // Recessed lip shadow cast onto the paper (light source: top)
    g.setGradientFill (ColourGradient (Colour (0x60000000), c.x, inner.getY(),
                                       Colours::transparentBlack, c.x, inner.getY() + 18.0f, false));
    g.fillRect (inner);
    g.setColour (Colour (0x40000000));
    g.drawRoundedRectangle (inner, 3.5f, 1.2f);

    // 3. Dial scale arcs & markings
    const Point<float> pivot (c.x, inner.getBottom() + 4.0f);
    const float R = inner.getHeight() * 0.70f;
    const float a0 = -0.64f; // approx -36.7 deg
    const float a1 =  0.64f; // approx +36.7 deg
    auto angleForT = [&] (float t) { return a0 + (a1 - a0) * t; };
    auto dirForA = [] (float a) { return Point<float> (std::sin (a), -std::cos (a)); };

    const float tMin = vuDeflectionForDb (-20.0f);
    const float tZero = vuDeflectionForDb (0.0f);

    // Black curved arc scale starting at -20 dB and ending precisely at 0
    Path blackArc;
    blackArc.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleForT (tMin), angleForT (tZero), true);
    g.setColour (pal::needleInk);
    g.strokePath (blackArc, PathStrokeType (1.3f));

    // Major VU markings (-20 dB to 0 dB, no positive numbers or red zone)
    const struct { float db; const char* txt; } majorVu[] =
    {
        { -20.0f, "-20" }, { -10.0f, "-10" }, { -7.0f, "-7" }, { -5.0f, "-5" },
        { -3.0f, "-3" },   { -2.0f, "-2" },   { -1.0f, "-1" }, { 0.0f, "0" }
    };
    for (const auto& m : majorVu)
    {
        const float t = vuDeflectionForDb (m.db);
        const float a = angleForT (t);
        const auto d = dirForA (a);

        // Tick mark
        g.setColour (pal::needleInk);
        g.drawLine (Line<float> (pivot + d * R, pivot + d * (R + 5.0f)), 1.2f);

        // Numerals
        const float labelR = R + 10.5f;
        g.setFont (fnt (7.5f, true));
        const Point<float> labelPos = pivot + d * labelR;
        g.drawText (m.txt, Rectangle<float> (22.0f, 10.0f).withCentre (labelPos), Justification::centred, false);
    }

    // Minor intermediate VU ticks (-20 dB to 0 dB)
    const float minorVu[] = { -15.0f, -9.0f, -8.0f, -6.0f, -4.0f, -2.5f, -1.5f, -0.5f };
    for (float db : minorVu)
    {
        const float t = vuDeflectionForDb (db);
        const float a = angleForT (t);
        const auto d = dirForA (a);
        g.setColour (pal::needleInk.withAlpha (0.70f));
        g.drawLine (Line<float> (pivot + d * R, pivot + d * (R + 2.8f)), 0.8f);
    }

    // Typography: positioned clearly in the visible window above the mask
    g.setFont (Font (FontOptions ("Georgia", 16.0f, Font::bold)));
    g.setColour (pal::needleInk.withAlpha (0.92f));
    g.drawText ("VU", Rectangle<float> (46.0f, 18.0f).withCentre ({ c.x, inner.getY() + inner.getHeight() * 0.52f }), Justification::centred, false);

    g.setFont (fnt (7.5f, true));
    g.setColour (pal::needleInk.withAlpha (0.80f));
    g.drawText ("dB", Rectangle<float> (18.0f, 10.0f).withCentre (pivot + dirForA (angleForT (-0.045f)) * (R + 10.5f)), Justification::centred, false);

    g.setFont (fnt (8.0f, true));
    g.setColour (pal::needleInk.withAlpha (0.85f));
    g.drawText ("GR", Rectangle<float> (30.0f, 10.0f).withCentre ({ c.x, inner.getY() + inner.getHeight() * 0.62f }), Justification::centred, false);

    // 4. Needle with drop shadow emerging from behind lower mask
    const float needleAngle = a0 + (a1 - a0) * jlimit (0.02f, 1.0f, deflection);

    // Needle drop shadow
    const Point<float> sOff (2.2f, 2.5f);
    Path sNeedle;
    sNeedle.startNewSubPath (-1.1f, 0.0f);
    sNeedle.lineTo (-0.35f, -(R + 5.0f));
    sNeedle.lineTo ( 0.35f, -(R + 5.0f));
    sNeedle.lineTo ( 1.1f, 0.0f);
    sNeedle.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.24f));
    g.fillPath (sNeedle, AffineTransform::rotation (needleAngle).translated (pivot.x + sOff.x, pivot.y + sOff.y));

    // Needle blade
    Path needle;
    needle.startNewSubPath (-1.0f, 0.0f);
    needle.lineTo (-0.3f, -(R + 5.0f));
    needle.lineTo ( 0.3f, -(R + 5.0f));
    needle.lineTo ( 1.0f, 0.0f);
    needle.closeSubPath();
    g.setColour (pal::needleInk);
    g.fillPath (needle, AffineTransform::rotation (needleAngle).translated (pivot.x, pivot.y));

    // 5. Signature lower moulded black bakelite mask / apron covering the movement
    const float maskY = inner.getBottom() - inner.getHeight() * 0.25f;
    Path mask;
    mask.startNewSubPath (inner.getX() - 1.0f, inner.getBottom() + 1.0f);
    mask.lineTo (inner.getX() - 1.0f, maskY);
    mask.quadraticTo (c.x, maskY - 6.0f, inner.getRight() + 1.0f, maskY);
    mask.lineTo (inner.getRight() + 1.0f, inner.getBottom() + 1.0f);
    mask.closeSubPath();

    g.setGradientFill (ColourGradient (Colour (0xff26282b), c.x, maskY - 6.0f,
                                       Colour (0xff0e0f11), c.x, inner.getBottom(), false));
    g.fillPath (mask);

    Path maskEdge;
    maskEdge.startNewSubPath (inner.getX(), maskY);
    maskEdge.quadraticTo (c.x, maskY - 6.0f, inner.getRight(), maskY);
    g.setColour (Colours::white.withAlpha (0.28f));
    g.strokePath (maskEdge, PathStrokeType (1.0f));

    // Iconic ZERO ADJ mechanical calibration screw, centred on the expanded window
    const Point<float> screwPos (c.x, inner.getBottom() - 16.0f);
    g.setColour (Colours::black.withAlpha (0.65f));
    g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (screwPos.translated (0.0f, 0.5f)));

    g.setGradientFill (ColourGradient (Colour (0xffadb2b8), screwPos.x - 3.0f, screwPos.y - 3.0f,
                                       Colour (0xff44474c), screwPos.x + 3.0f, screwPos.y + 3.0f, false));
    g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (screwPos));
    g.setColour (Colours::white.withAlpha (0.45f));
    g.drawEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (screwPos), 0.8f);

    // Horizontal screwdriver slot
    g.setColour (Colour (0xff141618));
    g.fillRect (Rectangle<float> (8.0f, 1.4f).withCentre (screwPos));

    g.setFont (fnt (6.5f, true));
    g.setColour (Colour (0xff888e94));
    g.drawText ("ZERO ADJ", Rectangle<float> (46.0f, 8.0f).withCentre ({ c.x, inner.getBottom() - 6.0f }), Justification::centred, false);

    // 6. Flat polished glass reflection, top-down key light
    Path glare;
    glare.startNewSubPath (inner.getX() + 10.0f, inner.getY());
    glare.lineTo (inner.getX() + 50.0f, inner.getY());
    glare.lineTo (inner.getX() + 20.0f, inner.getBottom());
    glare.lineTo (inner.getX() - 10.0f, inner.getBottom());
    glare.closeSubPath();
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), inner.getX() + 30.0f, inner.getY(),
                                       Colours::transparentWhite, inner.getX() + 50.0f, inner.getY() + 40.0f, false));
    g.fillPath (glare);

    g.restoreState();

    // 7. Glass lip and polished bezel, stroked after the paper so the frame stays crisp
    g.setGradientFill (ColourGradient (Colour (0xffcbced2), r.getX(), r.getY(),
                                       Colour (0xff484a4e), r.getRight(), r.getBottom(), false));
    g.drawRoundedRectangle (r.reduced (0.7f), 4.6f, 1.4f);
    g.setColour (Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (0.2f), 5.0f, 1.0f);
    g.setColour (Colour (0xff0a0b0d));
    g.drawRoundedRectangle (r.reduced (2.2f), 4.0f, 0.9f);
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
    /** Daka-Ware LA-2A replacement knob: low round base, scalloped grip body, and the
        characteristic long raised pointer/wing. Geometry follows the product reference. */
    static void drawDavies1470 (Graphics& g, Rectangle<float> b, Point<float> c, Slider& s,
                                float knobScale)
    {
        static Image knobImage = []
        {
            const auto* bytes = BinaryData::dakawarela2afront_png;
            const int size = BinaryData::dakawarela2afront_pngSize;
            return ImageFileFormat::loadFrom (bytes, (size_t) size);
        }();
        static Image knobShadow = [] (const Image& source)
        {
            if (! source.isValid()) return Image();
            Image shadow (Image::ARGB, source.getWidth(), source.getHeight(), true);
            for (int y = 0; y < source.getHeight(); ++y)
                for (int x = 0; x < source.getWidth(); ++x)
                {
                    const auto alpha = source.getPixelAt (x, y).getAlpha();
                    shadow.setPixelAt (x, y, Colour (0xff090a0b).withAlpha ((uint8) (alpha * 0.58f)));
                }
            return shadow;
        } (knobImage);
        const float pos = jlimit (0.0f, 1.0f, (float) s.valueToProportionOfLength (s.getValue()));
        // UA's numbered faceplate runs clockwise from 0 at ~7:30 to 100 at ~4:30.
        constexpr float dialA0 = MathConstants<float>::pi * 1.25f;
        constexpr float dialA1 = MathConstants<float>::pi * 2.75f;
        const float outerR = b.getWidth() * 0.455f;
        const float angle = dialA0 + pos * (dialA1 - dialA0);
        const float tickR = outerR * 1.02f;
        const float labelR = outerR * 1.40f;
        const Point<float> shadowOffset (2.3f * knobScale, 3.0f * knobScale);

        const float side = outerR * 1.48f;
        const auto knobDest = Rectangle<float> (side, side).withCentre (c);

        // The shadow uses source alpha only; its black RGB is independent of the artwork.
        // Rotate the offset silhouette around the knob centre, then paint the source paddle.
        if (knobImage.isValid())
        {
            g.saveState();
            const auto shadowDest = knobDest.translated (shadowOffset.x, shadowOffset.y)
                                        .transformedBy (AffineTransform::rotation (
                                            angle - MathConstants<float>::pi, c.x, c.y));
            g.drawImage (knobShadow, shadowDest, RectanglePlacement::centred);
            g.restoreState();

            g.saveState();
            g.addTransform (AffineTransform::rotation (angle - MathConstants<float>::pi, c.x, c.y));
            g.drawImage (knobImage, knobDest, RectanglePlacement::centred);
            g.restoreState();
        }

        for (int value = 0; value <= 100; value += 10)
        {
            const float markAngle = dialA0 + (dialA1 - dialA0) * ((float) value / 100.0f);
            const Point<float> dir = knobDirection (markAngle);
            const float markLength = (value % 20 == 0 ? 5.2f : 3.4f) * knobScale;
            g.setColour (Colours::black.withAlpha (0.92f));
            g.drawLine (Line<float> (c + dir * tickR,
                                     c + dir * (tickR + markLength)),
                        (value % 20 == 0 ? 1.2f : 0.8f) * knobScale);
        }

        g.setFont (fnt (jmax (7.0f, 8.0f * knobScale), true));
        for (int value = 0; value <= 100; value += 10)
        {
            const float t = (float) value / 100.0f;
            const float labelAngle = dialA0 + (dialA1 - dialA0) * t;
            const Point<float> dir = knobDirection (labelAngle);
            const Point<float> labelCentre = c + dir * labelR;
            const auto labelBox = Rectangle<float> (20.0f, 12.0f).withCentre (labelCentre);
            g.setColour (Colours::black.withAlpha (0.78f));
            g.drawText (String (value), labelBox.translated (0.0f, 0.7f), Justification::centred);
            g.setColour (Colour (0xff171819));
            g.drawText (String (value), labelBox, Justification::centred);
        }
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                           float /*pos*/, float a0, float a1, Slider& s) override
    {
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float d = jmin (r.getWidth(), r.getHeight());
        // knobScale shrinks the drawn hardware about its own centre (the component bounds,
        // and therefore the centre axis, never move). Used by the 1176 ATTACK/RELEASE pair.
        const float knobScale = (float) s.getProperties().getWithDefault ("knobScale", 1.0f);
        auto b = r.withSizeKeepingCentre (d * knobScale, d * knobScale);
        const auto c = b.getCentre();
        const auto cap = s.findColour (Slider::rotarySliderFillColourId);

        // Davies Molding 1470 (LA-2A / UREI replacement "Daka-Ware" bakelite).
        // Opt-in per slider so the other modules keep their own hardware.
        if (s.getProperties().contains ("davies1470"))
        {
            drawDavies1470 (g, b, c, s, knobScale);
            return;
        }

        const float pos = (float) s.valueToProportionOfLength (s.getValue());
        const float angle = a0 + pos * (a1 - a0);

        const bool fetInputScale = s.getProperties().contains ("fetInputScale");
        const bool fetOutputScale = s.getProperties().contains ("fetOutputScale");
        const bool fetNumberScale = s.getProperties().contains ("fetNumberScale");

        // ---- Skirt: heavy machined ring with scale marks
        const float skirtR = d * knobScale * 0.50f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c).translated (0.0f, 2.0f));

        g.setGradientFill (ColourGradient (Colour (0xff383d42), c.x, c.y - skirtR,
                                           Colour (0xff0a0c0e), c.x, c.y + skirtR, false));
        g.fillEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.15f));
        g.drawEllipse (Rectangle<float> (skirtR * 2.0f, skirtR * 2.0f).withCentre (c), 0.8f);

        KnobScale sc; sc.a0 = a0; sc.a1 = a1;
        paintScale (g, c, skirtR * 0.78f, sc, Colours::white.withAlpha (0.85f), knobScale);

        if (fetInputScale || fetOutputScale || fetNumberScale)
        {
            g.setFont (fnt (jmax (7.0f, 8.0f * knobScale), true));
            for (int i = 0; i <= 8; ++i)
            {
                const float t = (float) i / 8.0f;
                const float a = a0 + (a1 - a0) * t;
                const Point<float> dir (std::sin (a), -std::cos (a));
                const float labelRadius = fetNumberScale ? skirtR * 0.88f : skirtR * 0.97f;
                const int value = fetNumberScale ? roundToInt (juce::jmap (t, 1.0f, 7.0f))
                                                  : roundToInt (juce::jmap (t, 0.0f, 48.0f));
                const auto p = c + dir * labelRadius;
                const auto text = Rectangle<float> (p.x - 10.0f, p.y - 7.0f, 20.0f, 12.0f);
                g.setColour (Colours::black.withAlpha (0.8f));
                g.drawText (String (value), text.translated (0.0f, 1.0f), Justification::centred);
                g.setColour (Colours::white.withAlpha (0.9f));
                g.drawText (String (value), text, Justification::centred);
            }
        }

        // ---- Knob Body
        const float bodyR = d * knobScale * 0.355f;
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

        // Very thin white rim along the black channel edge: keeps the fader slot readable
        // against the dark plate without thickening the line.
        g.setColour (Colours::white.withAlpha (0.62f));
        g.drawRoundedRectangle (track.expanded (0.8f), 4.8f, 0.7f);

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

// Machined brushed-silver fader cap (SSL/P&G style)
        g.setGradientFill (ColourGradient (Colour (0xffe8edf2), cap.getX() + 2.0f, cap.getY(),
                                            Colour (0xff8a96a3), cap.getRight() - 2.0f, cap.getY(), false));
        g.fillRoundedRectangle (cap, 4.0f);

        // Ribbed grip ridges
        g.setColour (Colour (0xff5a6470).withAlpha (0.6f));
        for (float ry = cap.getY() + 6.0f; ry < cap.getBottom() - 6.0f; ry += 3.0f)
            g.drawHorizontalLine ((int) ry, cap.getX() + 3.0f, cap.getRight() - 3.0f);

        // Ergonomic center finger concave well
        const Rectangle<float> well (cap.getX() + 4.0f, cap.getY() + 6.0f, cap.getWidth() - 8.0f, cap.getHeight() - 12.0f);
        g.setGradientFill (ColourGradient (Colour (0xffb0bac6), well.getX(), well.getY(),
                                            Colour (0xff6a7480), well.getX(), well.getBottom(), false));
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
//  Hardware API 560 "IN" Button Look & Feel (Square white button + IN + red jewel LED)
//==============================================================================
struct InButtonLookAndFeel : public LookAndFeel_V4
{
    void drawToggleButton (Graphics& g, ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        // con_bypass == false means EQ is IN (active in signal chain)
        const bool active = ! b.getToggleState();

        // Square white pushbutton on left
        const Rectangle<float> sq (r.getX() + 4.0f, r.getCentreY() - 11.0f, 22.0f, 22.0f);
        g.setColour (Colours::black.withAlpha (active ? 0.85f : 0.50f));
        g.fillRoundedRectangle (sq.translated (0.0f, active ? 0.5f : 1.8f), 2.5f);

        g.setGradientFill (ColourGradient (active ? Colour (0xffdce0e3) : Colour (0xffffffff), sq.getX(), sq.getY(),
                                           active ? Colour (0xffb0b6bc) : Colour (0xffd8dee2), sq.getX(), sq.getBottom(), false));
        g.fillRoundedRectangle (sq, 2.0f);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (sq.reduced (0.5f), 2.0f, 0.8f);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (sq, 2.0f, 1.0f);

        // "IN" label in center
        g.setFont (Font (FontOptions (15.0f, Font::bold)));
        g.setColour (Colours::black.withAlpha (0.75f));
        g.drawText ("IN", Rectangle<float> (sq.getRight() + 10.0f, r.getY() + 1.0f, 28.0f, r.getHeight()), Justification::centredLeft);
        g.setColour (Colours::white);
        g.drawText ("IN", Rectangle<float> (sq.getRight() + 10.0f, r.getY(), 28.0f, r.getHeight()), Justification::centredLeft);

        // Red jewel LED indicator on right
        const Point<float> ledC (sq.getRight() + 52.0f, r.getCentreY());
        const float rad = 6.0f;

        g.setColour (Colour (0xff08090a));
        g.fillEllipse (Rectangle<float> ((rad + 2.5f) * 2.0f, (rad + 2.5f) * 2.0f).withCentre (ledC));
        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawEllipse (Rectangle<float> ((rad + 2.5f) * 2.0f, (rad + 2.5f) * 2.0f).withCentre (ledC), 0.8f);

        const Rectangle<float> ledRect (rad * 2.0f, rad * 2.0f);
        if (active)
        {
            // Saturated optical red glow bloom
            g.setGradientFill (ColourGradient (Colour (0x75ff2211), ledC.x, ledC.y,
                                               Colours::transparentWhite, ledC.x + 15.0f, ledC.y + 15.0f, true));
            g.fillEllipse (Rectangle<float> (30.0f, 30.0f).withCentre (ledC));

            g.setGradientFill (ColourGradient (Colour (0xffff6a55), ledC.x - 1.5f, ledC.y - 1.5f,
                                               Colour (0xffc4160e), ledC.x + 2.0f, ledC.y + 2.0f, false));
            g.fillEllipse (ledRect.withCentre (ledC));
            g.setColour (Colours::white.withAlpha (0.75f));
            g.fillEllipse (Rectangle<float> (2.5f, 2.0f).withCentre (ledC.translated (-1.2f, -1.2f)));
        }
        else
        {
            g.setGradientFill (ColourGradient (Colour (0xff48100c), ledC.x - 1.0f, ledC.y - 1.0f,
                                               Colour (0xff1a0402), ledC.x + 2.0f, ledC.y + 2.0f, false));
            g.fillEllipse (ledRect.withCentre (ledC));
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawEllipse (ledRect.withCentre (ledC), 0.7f);
        }
    }
};

//==============================================================================
//  Module plate base
//==============================================================================
class ModulePanel : public Component
{
public:
    ModulePanel (ZychanStripProcessor& p, int id, const String& prefixText,
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

    virtual void tick()
    {
        const float target = proc.getGainReduction (moduleId);
        gr += (target - gr) * 0.35f;
        if (std::abs (target - gr) < 0.002f) gr = target;
        if (gr < 0.05f) gr = 0.0f;
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
        if (moduleId != ZychanStripProcessor::Console)
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

    void setupKnob (Knob& k, const String& id, Colour cap, Colour indicator = Colours::white,
                    float scale = 1.0f, bool davies1470 = false)
    {
        const float rotaryStart = davies1470 ? MathConstants<float>::pi * 1.25f
                                              : MathConstants<float>::pi * 1.22f;
        const float rotaryEnd = davies1470 ? MathConstants<float>::pi * 2.75f
                                            : MathConstants<float>::pi * 2.78f;
        k.slider.setRotaryParameters (rotaryStart, rotaryEnd, true);
        k.slider.setColour (Slider::rotarySliderFillColourId, cap);
        k.slider.setColour (Slider::thumbColourId, indicator);
        k.slider.getProperties().set ("knobScale", scale);
        if (davies1470) k.slider.getProperties().set ("davies1470", true);
        k.slider.setNumDecimalPlacesToDisplay (1);
        k.slider.setPopupDisplayEnabled (true, true, nullptr);
        k.slider.setDoubleClickReturnValue (true, k.slider.getDoubleClickReturnValue());
        k.slider.setName (id);
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

    ZychanStripProcessor& proc;
    String prefix;
    Colour plateTop, plateLow, labelColour;
    Rectangle<int> content;
    float gr = 0.0f;
    ToggleButton bypass { "BYPASS" };

private:
    void paintChassisGrip (Graphics& g) { paintGrip (g, gripBounds().toFloat(), gripHot); }

    void paintScrewsAndBadge (Graphics& g, Rectangle<float> plate)
    {
        if (moduleId == ZychanStripProcessor::Console)
            return; // Console panel has custom screws & holes matching the real API 560 unit

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
//  Module 1 — API 560 ten-band graphic EQ (authentic 500-series hardware layout)
//==============================================================================
class ConsolePanel : public ModulePanel
{
public:
    explicit ConsolePanel (ZychanStripProcessor& p)
        : ModulePanel (p, ZychanStripProcessor::Console, "con_",
                       Colour (0xff141517), Colour (0xff08090a), pal::engrave)
    {
        const char* ids[] { "eq_16k", "eq_8k", "eq_4k", "eq_2k", "eq_1k", "eq_500", "eq_250", "eq_125", "eq_63", "eq_31" };
        for (size_t i = 0; i < bands.size(); ++i)
        {
            auto& band = bands[i];
            band.slider.setSliderStyle (Slider::LinearHorizontal);
            band.slider.setName (prefix + ids[i]);
            band.slider.getProperties().set ("eq560", true);
            // In the API 560 photo: 1K (index 4) has the iconic red cap!
            band.slider.setColour (Slider::thumbColourId, i == 4 ? Colour (0xffd8262a) : Colour (0xffe6e9e4));
            band.slider.setTextValueSuffix (" dB");
            band.slider.setNumDecimalPlacesToDisplay (1);
            band.slider.setPopupDisplayEnabled (true, true, nullptr);
            band.slider.setDoubleClickReturnValue (true, 0.0);
            band.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, prefix + ids[i], band.slider);
            addAndMakeVisible (band.slider);
        }

        bypass.setLookAndFeel (&inLaf);
    }

    ~ConsolePanel() override
    {
        bypass.setLookAndFeel (nullptr);
    }

protected:
    void layoutContent (Rectangle<int> c) override
    {
        // Spanning the full height of the module
        const int startY = c.getY() + 72;
        const int totalH = c.getBottom() - 72 - startY;
        const int rowSpacing = totalH / 9;

        for (size_t i = 0; i < bands.size(); ++i)
            bands[i].slider.setBounds (c.getX() + 38, startY + (int) i * rowSpacing, c.getWidth() - 44, 24);

        // Hardware IN switch at bottom
        const int inY = c.getBottom() - 52;
        bypass.setBounds (c.getCentreX() - 46, inY, 92, 26);
    }

    void paintFaceExtra (Graphics& g) override
    {
        // 1. Top center mounting screw hole cutout
        const Point<float> topHole ((float) content.getCentreX(), (float) content.getY() - 1.0f);
        g.setColour (Colour (0xff050607));
        g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (topHole));
        g.setColour (Colours::white.withAlpha (0.45f));
        g.drawEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (topHole), 1.0f);
        g.setColour (Colour (0xff202428));
        g.drawEllipse (Rectangle<float> (13.6f, 13.6f).withCentre (topHole), 0.8f);

        // 2. API emblem in top left (light blue)
        paintApiLogo (g, Rectangle<float> ((float) content.getX() + 4.0f, (float) content.getY() - 4.0f, 32.0f, 24.0f), Colour (0xff8bb5eb));

        // 3. "560" in top right (light blue)
        g.setFont (fnt (26.0f, false));
        g.setColour (Colour (0xff8bb5eb));
        g.drawText ("560", Rectangle<float> ((float) content.getRight() - 56.0f, (float) content.getY() - 6.0f, 52.0f, 26.0f), Justification::centredRight);

        // 4. Two black machine screws below top header
        const float topScrewY = (float) content.getY() + 26.0f;
        paintScrew (g, { (float) content.getX() + 6.0f, topScrewY }, 4.6f, 0.45f);
        paintScrew (g, { (float) content.getRight() - 6.0f, topScrewY }, 4.6f, -0.65f);

        // 5. Header scale: -12   0   +12
        auto& first = bands.front().slider;
        auto sliderBounds = first.getLookAndFeel().getSliderLayout (first).sliderBounds.translated (first.getX(), first.getY());
        const float x0 = (float) sliderBounds.getX();
        const float travel = (float) sliderBounds.getWidth();
        const float scaleY = (float) content.getY() + 44.0f;

        g.setFont (fnt (12.5f, true));
        g.setColour (Colours::white);
        g.drawText ("-12", Rectangle<float> (x0 - 18.0f, scaleY, 36.0f, 16.0f), Justification::centred);
        g.drawText ("0", Rectangle<float> (x0 + travel * 0.5f - 18.0f, scaleY, 36.0f, 16.0f), Justification::centred);
        g.drawText ("+12", Rectangle<float> (x0 + travel - 18.0f, scaleY, 36.0f, 16.0f), Justification::centred);

        // 6. Five continuous light blue guide lines from top scale down through all 10 slots
        const float yTop = scaleY + 18.0f;
        const float yBtm = (float) bands.back().slider.getBounds().getBottom() + 1.0f;
        for (int i = 0; i <= 4; ++i)
        {
            const float x = x0 + travel * (float) i * 0.25f;
            g.setColour (Colour (0xff8bb5eb).withAlpha (0.90f));
            g.drawLine (x, yTop, x, yBtm, 1.2f);
        }

        // 7. Frequency labels on left
        const char* frequencies[] { "16K", "8K", "4K", "2K", "1K", "500", "250", "125", "63", "31" };
        g.setFont (fnt (13.0f, true));
        g.setColour (Colours::white);
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto sb = bands[i].slider.getBounds();
            g.drawText (frequencies[i], Rectangle<float> ((float) content.getX() - 6.0f, (float) sb.getCentreY() - 9.0f, 38.0f, 18.0f),
                        Justification::centredRight);
        }

        // 8. Two black machine screws at the bottom
        const float btmScrewY = (float) content.getBottom() - 20.0f;
        paintScrew (g, { (float) content.getX() + 6.0f, btmScrewY }, 4.6f, 0.85f);
        paintScrew (g, { (float) content.getRight() - 6.0f, btmScrewY }, 4.6f, -0.35f);

        // 9. Bottom center mounting screw hole cutout
        const Point<float> btmHole ((float) content.getCentreX(), (float) content.getBottom() - 4.0f);
        g.setColour (Colour (0xff050607));
        g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (btmHole));
        g.setColour (Colours::white.withAlpha (0.45f));
        g.drawEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (btmHole), 1.0f);
        g.setColour (Colour (0xff202428));
        g.drawEllipse (Rectangle<float> (13.6f, 13.6f).withCentre (btmHole), 0.8f);
    }

private:
    std::array<Knob, 10> bands;
    InButtonLookAndFeel inLaf;
};

//==============================================================================
//  Module 2 — opto leveller (reference: LA-2A)
//==============================================================================
class OptoPanel : public ModulePanel
{
public:
    explicit OptoPanel (ZychanStripProcessor& p)
        : ModulePanel (p, ZychanStripProcessor::Opto, "opto_",
                       pal::faceLightTop, pal::faceLightLow, Colour (0xff141414))
    {
        labelColour = Colour (0xff141414);
        bypass.setColour (ToggleButton::textColourId, labelColour);

        // LA-2A used the Davies Molding 1470 fluted bakelite knob on both controls.
        setupKnob (gain, "gain", pal::blackCap, Colours::white, 0.78f, true);
        setupKnob (peak, "peak", pal::blackCap, Colours::white, 0.78f, true);

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
        // Authentic large round circular VU meter, housing at 85 % of its old diameter about
        // the same centre point on the plate.
        const int vw = jmin (c.getWidth() - 8, 214);
        const Rectangle<int> fullVu { c.getCentreX() - vw / 2, c.getY() + 4, vw, vw };
        const int nvw = roundToInt ((float) vw * 0.85f);
        vu = fullVu.withSizeKeepingCentre (nvw, nvw);

        const int kw = 84;
        const int ky = 320;
        place (gain, { c.getX() + 8,                 ky, kw, kw });
        place (peak, { c.getRight() - kw - 8,        ky, kw, kw });

        const int ty = 430;
        meterBtn.setBounds (c.getCentreX() - 48, ty, 96, 26);
        onBtn.setBounds (c.getCentreX() - 48, ty + 38, 96, 26);
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
        caption (g, "INPUT GAIN",      { gain.slider.getBounds().getCentreX() - 60, gain.slider.getY() - 30, 120, 24 }, 13.0f);
        caption (g, "PEAK REDUCTION", { peak.slider.getBounds().getCentreX() - 65, peak.slider.getY() - 30, 130, 24 }, 13.0f);

        // Lower vintage sub-plate filling empty negative space with authentic nomenclature
        const Rectangle<float> subPlate ((float) content.getX() + 8.0f, 516.0f, (float) content.getWidth() - 16.0f, 96.0f);
        g.setColour (Colours::black.withAlpha (0.12f));
        g.drawRoundedRectangle (subPlate, 3.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (subPlate.translated (0.0f, 1.0f), 3.0f, 0.8f);

        caption (g, "TELETRONIX", { (int) subPlate.getX(), (int) subPlate.getY() + 14, (int) subPlate.getWidth(), 18 }, 14.0f);
        caption (g, "AUTOMATIC LEVELING AMPLIFIER", { (int) subPlate.getX(), (int) subPlate.getY() + 34, (int) subPlate.getWidth(), 14 }, 8.5f, Justification::centred, false);
        caption (g, "PATENT NO. 3,258,696", { (int) subPlate.getX(), (int) subPlate.getY() + 52, (int) subPlate.getWidth(), 14 }, 7.5f, Justification::centred, false);

        paintBadge (g, Rectangle<float> ((float) content.getCentreX() - 44.0f, (float) content.getBottom() - 34.0f, 88.0f, 28.0f), "LA-2A");
    }

    void paintMeter (Graphics& g) override
    {
        const auto r = vu.toFloat();
        // Warm glow bloom around round VU housing
        g.setGradientFill (ColourGradient (Colour (0x38ffd27a), r.getCentreX(), r.getCentreY(),
                                           Colour (0x00ffd27a), r.getCentreX(), r.getCentreY() - r.getHeight(), true));
        g.fillEllipse (r.expanded (18.0f));

        const float db = -jlimit (0.0f, 20.0f, gr);
        paintOptoVuMeter (g, r, vuDeflectionForDb (db));
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
    explicit FetPanel (ZychanStripProcessor& p)
        : ModulePanel (p, ZychanStripProcessor::Fet, "fet_",
                       pal::fetTop, pal::fetLow, pal::engrave)
    {
        setupKnob (input,  "input",  pal::silverCap, Colour (0xff202020));
        setupKnob (output, "output", pal::silverCap, Colour (0xff202020));
        // ATTACK / RELEASE sit at 80 % of the other knobs; the component bounds (and so the
        // centre axis) stay exactly where they were, only the drawn hardware shrinks.
        setupKnob (atk,    "attack", pal::silverCap, Colour (0xff202020), 0.8f);
        setupKnob (rel,    "release",pal::silverCap, Colour (0xff202020), 0.8f);
        input.slider.getProperties().set ("fetInputScale", true);
        output.slider.getProperties().set ("fetOutputScale", true);
        atk.slider.getProperties().set ("fetNumberScale", true);
        rel.slider.getProperties().set ("fetNumberScale", true);

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
        const int kw = 86;
        const int ky = c.getY() + 26;
        place (input,  { c.getX() + 6,               ky, kw, kw });
        place (output, { c.getRight() - kw - 6,      ky, kw, kw });

        ratioLabel = { c.getX(), 170, c.getWidth(), 18 };
        const int bw = (c.getWidth() - 8 - 4 * 4) / 5;
        for (int i = 0; i < 5; ++i)
            ratioBtns[(size_t) i].setBounds (c.getX() + 4 + i * (bw + 4), 192, bw, 26);

        // Span the faceplate edge to edge, independent of the control-area inset.
        meter = { plateBounds().getX(), 230, plateBounds().getWidth(), 154 };

        const int ay = meter.getBottom() + 32;
        const int akw = 80;
        place (atk, { c.getX() + 10,       ay, akw, akw });
        place (rel, { c.getRight() - akw - 10, ay, akw, akw });
        grLabel = { c.getX() + c.getWidth() / 2 - 40, ay + 44, 80, 18 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        caption (g, "INPUT",  { input.slider.getBounds().getCentreX() - 50, input.slider.getY() - 20, 100, 18 }, 13.0f);
        caption (g, "OUTPUT", { output.slider.getBounds().getCentreX() - 50, output.slider.getY() - 20, 100, 18 }, 13.0f);
        caption (g, "RATIO",  ratioLabel, 14.0f);
        // ATTACK / RELEASE captions scale with their 80 % knobs and stay on the knob axis
        const int atkCx = atk.slider.getBounds().getCentreX();
        const int relCx = rel.slider.getBounds().getCentreX();
        const int atkCy = atk.slider.getBounds().getCentreY();
        const int relCy = rel.slider.getBounds().getCentreY();
        caption (g, "ATTACK",  { atkCx - 40, atkCy - 44, 80, 15 }, 10.4f);
        caption (g, "RELEASE", { relCx - 40, relCy - 44, 80, 15 }, 10.4f);

        // Vintage UREI-style wordmark in the top centre
        engraveText (g, "urei", { content.getX(), content.getY() - 6, content.getWidth(), 22 },
                     Justification::centred, 21.0f, Colour (0xffe8e8e8), false);

        // Lower vintage sub-plate filling empty negative space with authentic nomenclature
        const Rectangle<float> subPlate ((float) content.getX() + 8.0f, 516.0f, (float) content.getWidth() - 16.0f, 96.0f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (subPlate, 3.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (subPlate, 3.0f, 1.0f);

        engraveText (g, "LIMITING AMPLIFIER", { (int) subPlate.getX(), (int) subPlate.getY() + 14, (int) subPlate.getWidth(), 16 },
                     Justification::centred, 11.5f, pal::engrave, true);
        engraveText (g, "MODEL 1176 LN", { (int) subPlate.getX(), (int) subPlate.getY() + 34, (int) subPlate.getWidth(), 18 },
                     Justification::centred, 13.5f, Colour (0xffe6c878), true);
        engraveText (g, "SOLID STATE PEAK LIMITER", { (int) subPlate.getX(), (int) subPlate.getY() + 56, (int) subPlate.getWidth(), 14 },
                     Justification::centred, 8.5f, pal::engraveDim, false);

        paintBadge (g, { (float) content.getCentreX() - 44.0f, (float) content.getBottom() - 34.0f, 88.0f, 28.0f }, "1176");
    }

    void paintMeter (Graphics& g) override
    {
        const auto r = meter.toFloat();
        const float db = -jlimit (0.0f, 20.0f, gr);
        paintFetVuMeter (g, r, vuDeflectionForDb (db));
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
    explicit LimiterPanel (ZychanStripProcessor& p)
        : ModulePanel (p, ZychanStripProcessor::Limiter, "lim_",
                       pal::limTop, pal::limLow, pal::engrave)
    {
        // All four controls are vertical sliders so they read as one bank, matching ENHANCE.
        labelColour = Colour (0xffe6e8ea);   // light engraving on the dark blue plate
        bypass.setColour (ToggleButton::textColourId, labelColour);
        setupFader (gain,    "gain");
        setupFader (ceiling, "ceiling");
        setupFader (release, "release");
        setupFader (fader,   "enhance");

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
        // Two zones: the control area (left) holds a 2x2 fader grid, the metering area (right)
        // keeps both vertical ladders running the full height beside it.
        const int meterZone = 66;                      // reserves room for the two ladders
        const int bankW = c.getWidth() - meterZone;

        const int top = c.getY() + 24;                 // room for the captions above
        autoGain.setBounds (c.getX() + 2, top, bankW - 8, 24);

        // Control area below the AUTO GAIN switch, split into two equal rows.
        const int gridTop = top + 34;
        const int bottomReserve = 26;
        const int gridH = c.getBottom() - gridTop - bottomReserve;

        // Leave room for the per-row captions above each fader cell.
        const int captionH = 16;
        const int rowH = gridH / 2;
        const int cellW = bankW / 2;

        Slider* all[4] { &gain, &ceiling, &release, &fader };
        for (int i = 0; i < 4; ++i)
        {
            const int row = i / 2;                     // 0 = THRESHOLD / CEILING, 1 = RELEASE / ENHANCE
            const int col = i % 2;
            const int cellX = c.getX() + cellW * col;
            const int cellY = gridTop + rowH * row;
            const int faderH = juce::jmax (24, rowH - captionH - 4);

            // Remember the cell so captions can span it (a caption clipped to the 26 px fader
            // would truncate "THRESHOLD").
            gridCell ((size_t) col) = { cellX, cellY, cellW, rowH };

            // Centre the narrow fader inside its cell.
            all[i]->setBounds (cellX + cellW / 2 - 13, cellY + captionH, 26, faderH);
        }

        // Both meters together on the right edge, GR then output, spanning both grid rows.
        const int mx = c.getRight() - meterZone + 8;
        ladderA = { mx,      gridTop + 22, 22, gridH - 22 };
        ladderB = { mx + 34, gridTop + 22, 22, gridH - 22 };
        scaleRect = { mx - 2, gridTop + 22, meterZone - 6, gridH - 22 };
        enhanceLabel = { c.getX(), c.getBottom() - 20, c.getWidth(), 18 };
    }

    void paintFaceExtra (Graphics& g) override
    {
        const char* names[4] { "THRESHOLD", "CEILING", "RELEASE", "ENHANCE" };
        Slider* all[4] { &gain, &ceiling, &release, &fader };
        for (int i = 0; i < 4; ++i)
        {
            // Span the whole grid cell, not just the narrow fader, so long captions are not clipped.
            const int col = i % 2;
            const auto& cell = gridCell ((size_t) col);
            const auto b = all[i]->getBounds();
            caption (g, names[i], Rectangle<int> (cell.getX(), b.getY() - 18, cell.getWidth(), 16), 8.5f);
        }

        // dB scale beside the meters
        g.setFont (fnt (9.0f, false));
        for (int i = 0; i <= 6; ++i)
        {
            const float t = (float) i / 6.0f;
            const float y = (float) ladderA.getY() + (float) ladderA.getHeight() * t;
            const int db = (int) std::round (12.0f - 24.0f * t);
            g.setColour (Colours::black.withAlpha (0.75f));
            g.drawText (String (db), Rectangle<float> ((float) ladderA.getX() - 26.0f, y - 6.0f, 24.0f, 13.0f), Justification::centredRight);
            g.setColour (Colours::white.withAlpha (0.65f));
            g.drawText (String (db), Rectangle<float> ((float) ladderA.getX() - 26.0f, y - 7.0f, 24.0f, 13.0f), Justification::centredRight);
        }

    }

    // The metre surround is painted into the face image so the ladders draw on top of it.
    void paintFaceBase (Graphics& g) override
    {
        if (ladderA.isEmpty()) return;

        const Rectangle<int> frame (ladderA.getX() - 10, ladderA.getY() - 16,
                                    (ladderB.getRight() - ladderA.getX()) + 20, ladderA.getHeight() + 26);
        g.setColour (Colour (0xff23282e));
        g.fillRoundedRectangle (frame.toFloat(), 3.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), 0.0f, (float) frame.getY(),
                                           Colours::transparentWhite, 0.0f, (float) frame.getCentreY(), false));
        g.fillRoundedRectangle (frame.toFloat(), 3.0f);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.drawRoundedRectangle (frame.toFloat(), 3.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (frame.reduced (1.0f).toFloat(), 2.5f, 0.8f);

        g.setFont (fnt (7.5f, true));
        g.setColour (Colour (0xffd4dae0));
        g.drawText ("GR",  Rectangle<int> (ladderA.getX() - 4, ladderA.getY() - 12, ladderA.getWidth() + 8, 10), Justification::centred, false);
        g.drawText ("OUT", Rectangle<int> (ladderB.getX() - 4, ladderB.getY() - 12, ladderB.getWidth() + 8, 10), Justification::centred, false);
    }

    void tick() override
    {
        ModulePanel::tick();
        const float targetOut = proc.getLimiterOutput();
        outLevel += (targetOut - outLevel) * 0.35f;
        if (std::abs (targetOut - outLevel) < 0.001f) outLevel = targetOut;
        if (outLevel < 0.001f) outLevel = 0.0f;
    }

    void paintMeter (Graphics& g) override
    {
        // Enhance module meter fix: display nothing when not processing
        const float red = gr > 0.05f ? jlimit (0.0f, 1.0f, gr / 14.0f) : 0.0f;
        const float outDb = outLevel > 1.0e-5f ? Decibels::gainToDecibels (outLevel) : -100.0f;
        const float outNorm = outDb > -48.0f ? jlimit (0.0f, 1.0f, (outDb + 48.0f) / 48.0f) : 0.0f;

        paintLadder (g, ladderA.toFloat(), red, Colour (0xffc06a10), Colour (0xffffd23a), 28);
        paintLadder (g, ladderB.toFloat(), outNorm, Colour (0xff17b04a), Colour (0xff5fe1f5), 28);

        if (gr >= 0.1f)
        {
            engraveText (g, "-" + String (gr, 1) + " dB", { ladderA.getX() - 18, ladderA.getBottom() + 8, ladderA.getWidth() + 36, 16 },
                         Justification::centred, 11.0f, labelColour, false);
        }
    }

private:
    void setupFader (Slider& s, const String& id)
    {
        s.setSliderStyle (Slider::LinearVertical);
        s.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        s.setNumDecimalPlacesToDisplay (1);
        s.setPopupDisplayEnabled (true, true, nullptr);
        s.setName ("limiter " + id);
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, prefix + id, s));
        addAndMakeVisible (s);
    }

    // Declaration order matters: members are destroyed in reverse, so the sliders must be
    // declared BEFORE the attachments that reference them (otherwise the attachment's
    // destructor calls removeListener() on an already-freed Slider).
    Slider gain, ceiling, release, fader;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> attachments;
    ToggleButton autoGain;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt;
    Rectangle<int> ladderA, ladderB, scaleRect, enhanceLabel;
    std::array<Rectangle<int>, 2> gridCells {};     // column cells, shared by layout and captions
    Rectangle<int>& gridCell (size_t col) { return gridCells[col < 2 ? col : 0]; }
    float outLevel = 0.0f;
};

//==============================================================================
//  Editor
//==============================================================================
ZychanStripEditor::ZychanStripEditor (ZychanStripProcessor& p)
    : AudioProcessorEditor (p), proc (p), laf (std::make_unique<RackLookAndFeel>())
{
    setLookAndFeel (laf.get());
    meterPanel = std::make_unique<MeterPanel> (proc);
    // The meter page carries its own way back to the rack, mirroring the RACK/METER switch.
    meterPanel->onBack = [this] { showPage (false); };
    // RESET means the same thing on both pages: modules back to defaults, meters cleared.
    meterPanel->onReset = [this] { resetRackToDefaults(); };
    meterPanel->setName ("Meter page");
    addChildComponent (*meterPanel);
    addAndMakeVisible (canvas);
    canvas.setBounds (0, 0, kEditorW, kEditorH);

    panels[ZychanStripProcessor::Console] = std::make_unique<ConsolePanel> (proc);
    panels[ZychanStripProcessor::Opto]    = std::make_unique<OptoPanel> (proc);
    panels[ZychanStripProcessor::Fet]     = std::make_unique<FetPanel> (proc);
    panels[ZychanStripProcessor::Limiter] = std::make_unique<LimiterPanel> (proc);

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
    masterFader.setNumDecimalPlacesToDisplay (1);
    masterFader.setPopupDisplayEnabled (true, true, this);
    masterFader.setDoubleClickReturnValue (true, 0.0);
    masterFader.setColour (Slider::thumbColourId, Colours::white);
    canvas.addAndMakeVisible (masterFader);

    pageToggle.setName ("Page toggle");
    pageToggle.setBounds (kMargin + 232, 10, 84, 30);
    pageToggle.onChanged = [this] (bool showMeters) { showPage (showMeters); };
    canvas.addAndMakeVisible (pageToggle);

    resetButton.setName ("Reset");
    resetButton.onReset = [this] { resetRackToDefaults(); };
    canvas.addAndMakeVisible (resetButton);
    unityButton = std::make_unique<UnityButton> (proc);
    unityButton->setName ("UNITY");
    unityButton->setMouseCursor (MouseCursor::PointingHandCursor);
    unityButton->setWantsKeyboardFocus (true);
    unityButton->onChanged = [this] (bool enabled) { setUnityEnabled (enabled); };
    canvas.addAndMakeVisible (*unityButton);
    masterAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master_level", masterFader);

    // Dry/wet mix, sits after the master volume in the signal path
    mixFader.setName ("Dry/Wet mix");
    mixFader.setTextValueSuffix (" %");
    mixFader.setNumDecimalPlacesToDisplay (0);
    mixFader.setPopupDisplayEnabled (true, true, this);
    mixFader.setDoubleClickReturnValue (true, 100.0);
    mixFader.setColour (Slider::thumbColourId, Colour (0xff57d4c2));
    canvas.addAndMakeVisible (mixFader);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master_mix", mixFader);

    setResizable (true, true);
    setResizeLimits (864, 608, 1620, 1140);
    getConstrainer()->setFixedAspectRatio ((double) kEditorW / kEditorH);
    setSize (864, 608);
    startTimerHz (30);
}

juce::Component* ZychanStripEditor::getModulePanelForTest (int id)
{
    return id >= 0 && id < (int) panels.size() ? static_cast<juce::Component*> (panels[(size_t) id].get())
                                               : nullptr;
}

ZychanStripEditor::~ZychanStripEditor()
{
    // Order matters: the timer is the last thing that can reach into the editor, so it goes
    // first; the animations hold component pointers; then the LookAndFeel is unhung before the
    // LookAndFeel object itself is destroyed. The meter page is a member declared AFTER the
    // destructor body runs, so any callback it still holds (onBack/onReset capture `this`) would
    // fire against a half-destroyed editor — clear them before it is released.
    stopTimer();
    animator.cancelAllAnimations (false);
    if (meterPanel != nullptr)
    {
        meterPanel->onBack = nullptr;
        meterPanel->onReset = nullptr;
    }
    pageToggle.onChanged = nullptr;
    resetButton.onReset = nullptr;
    if (unityButton != nullptr) unityButton->onChanged = nullptr;
    unityButton.reset();
    for (auto& panel : panels)
        if (panel != nullptr)
        {
            panel->onDragStart = nullptr;
            panel->onDragMove  = nullptr;
            panel->onDragEnd   = nullptr;
        }
    setLookAndFeel (nullptr);
}

Rectangle<int> ZychanStripEditor::slotBounds (int slot) const
{
    const int pw = (kEditorW - kMargin * 2 - kGap * 3) / 4;
    const int top = kHeaderH + 4;
    return { kMargin + slot * (pw + kGap), top, pw, kEditorH - top - kMargin };
}

int ZychanStripEditor::slotForCentreX (int x) const
{
    const int pw = (kEditorW - kMargin * 2 - kGap * 3) / 4;
    return jlimit (0, 3, (x - kMargin) / (pw + kGap));
}

std::array<int, 4> ZychanStripEditor::orderWithDragged (int id, int slot) const
{
    std::vector<int> v;
    for (int m : proc.getOrder()) if (m != id) v.push_back (m);
    v.insert (v.begin() + jlimit (0, (int) v.size(), slot), id);
    return { v[0], v[1], v[2], v[3] };
}

void ZychanStripEditor::layoutFor (const std::array<int, 4>& order, int skipId, bool animate)
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

void ZychanStripEditor::resized()
{
    animator.cancelAllAnimations (false);
    draggingId = -1;
    previewOrder = proc.getOrder();
    for (auto& panel : panels) panel->cancelDrag();
    layoutFor (previewOrder, -1, false);
    // Master + dry/wet share the header right side; mix sits below the master level.
    masterFader.setBounds (kEditorW - kMargin - 160, 8, 148, 16);
    mixFader.setBounds (kEditorW - kMargin - 160, 26, 148, 14);
    // Reset button sits on the header, left of the master fader.
    resetButton.setBounds (kEditorW - kMargin - 160 - 150, 10, 84, 30);
    unityButton->setBounds (kEditorW - kMargin - 160 - 58, 10, 58, 30);
    if (meterPanel != nullptr && metersShown)
        meterPanel->setBounds (getLocalBounds());

    const float scale = jmax (0.01f, jmin ((float) getWidth() / kEditorW, (float) getHeight() / kEditorH));
    canvasTransform = AffineTransform::scale (scale).translated (
        ((float) getWidth() - kEditorW * scale) * 0.5f,
        ((float) getHeight() - kEditorH * scale) * 0.5f);
    canvas.setTransform (canvasTransform);
    repaint();
}

void ZychanStripEditor::timerCallback()
{
    if (metersShown)
    {
        if (meterPanel != nullptr) meterPanel->tick();
        return;
    }
    for (auto& p : panels) p->tick();
}

void ZychanStripEditor::showPage (bool meters)
{
    metersShown = meters;
    pageToggle.setMeters (meters);
    if (meterPanel != nullptr) meterPanel->setVisible (meters);
    canvas.setVisible (! meters);
    resized();
    repaint();
}

void ZychanStripEditor::resetRackToDefaults()
{
    proc.resetToDefaults();

    // The rack order may have moved back to the factory slots, so re-place every face plate.
    draggingId = -1;
    previewOrder = proc.getOrder();
    for (auto& panel : panels) panel->cancelDrag();
    layoutFor (previewOrder, -1, true);

    // Parameter attachments refresh the sliders/buttons; repaint the hand-drawn value readouts.
    for (auto& panel : panels) panel->repaint();
    if (meterPanel != nullptr) meterPanel->repaint();
    repaint();
}

void ZychanStripEditor::setUnityEnabled (bool enabled)
{
    if (auto* parameter = proc.apvts.getParameter ("unity"); parameter != nullptr)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (enabled ? 1.0f : 0.0f);
        parameter->endChangeGesture();
    }
    if (unityButton != nullptr) unityButton->repaint();
}

//==============================================================================
//  Page toggle (child of the canvas: the canvas consumes editor-level clicks)
//==============================================================================
void ZychanStripEditor::PageToggle::paint (Graphics& g)
{
    auto t = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0d0e));
    g.fillRoundedRectangle (t, 3.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (t, 3.0f, 1.0f);

    const auto half = t.getWidth() * 0.5f;
    const Rectangle<float> rackBtn (t.getX(), t.getY(), half, t.getHeight());
    const Rectangle<float> meterBtn (t.getX() + half, t.getY(), half, t.getHeight());

    g.setGradientFill (ColourGradient (meters ? Colour (0xff15181a) : Colour (0xff57d4c2),
                                       rackBtn.getX(), rackBtn.getY(),
                                       meters ? Colour (0xff0a0c0d) : Colour (0xff2f9d8f),
                                       rackBtn.getX(), rackBtn.getBottom(), false));
    g.fillRoundedRectangle (rackBtn.reduced (2.0f), 2.0f);
    g.setGradientFill (ColourGradient (meters ? Colour (0xff57d4c2) : Colour (0xff15181a),
                                       meterBtn.getX(), meterBtn.getY(),
                                       meters ? Colour (0xff2f9d8f) : Colour (0xff0a0c0d),
                                       meterBtn.getX(), meterBtn.getBottom(), false));
    g.fillRoundedRectangle (meterBtn.reduced (2.0f), 2.0f);

    g.setFont (fnt (10.0f, true));
    g.setColour (meters ? Colour (0xff9aa0a4) : Colour (0xff06201d));
    g.drawText ("RACK", rackBtn, Justification::centred, false);
    g.setColour (meters ? Colour (0xff06201d) : Colour (0xff9aa0a4));
    g.drawText ("METER", meterBtn, Justification::centred, false);
}

void ZychanStripEditor::PageToggle::mouseDown (const MouseEvent&)
{
    meters = ! meters;
    repaint();
    if (onChanged) onChanged (meters);
}

void ZychanStripEditor::ResetButton::paint (Graphics& g)
{
    // Teal at rest like RACK/METER, matte black while held.
    rackui::paintPill (g, getLocalBounds().toFloat(), armed, "RESET");
}

void ZychanStripEditor::ResetButton::mouseDown (const MouseEvent&)
{
    armed = true;
    repaint();
}

void ZychanStripEditor::ResetButton::mouseUp (const MouseEvent& e)
{
    if (armed && getLocalBounds().contains (e.getPosition()))
    {
        armed = false;
        repaint();
        if (onReset) onReset();
        return;
    }
    armed = false;
    repaint();
}

void ZychanStripEditor::UnityButton::paint (Graphics& g)
{
    const bool enabled = proc.isUnityEnabled();
    const auto bounds = getLocalBounds().toFloat();
    rackui::paintPill (g, bounds, armed, enabled ? "UNITY ON" : "UNITY");
    if (! enabled)
    {
        g.setColour (Colour (0xff57d4c2).withAlpha (0.28f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }
    if (enabled)
    {
        g.setColour (Colour (0xff57d4c2).withAlpha (0.95f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.2f);
    g.setFont (fnt (8.0f, true));
        g.drawText (String (proc.getUnityGainDb(), 1) + " dB", bounds.withY (bounds.getY() + 15).withHeight (12), Justification::centred);
    }
}


void ZychanStripEditor::UnityButton::mouseDown (const MouseEvent&)
{
    armed = true;
    repaint();
}

void ZychanStripEditor::UnityButton::mouseExit (const MouseEvent&)
{
    if (armed) { armed = false; repaint(); }
}

void ZychanStripEditor::UnityButton::mouseUp (const MouseEvent& e)
{
    if (armed && getLocalBounds().contains (e.getPosition()))
        if (onChanged) onChanged (! proc.isUnityEnabled());
    armed = false;
    repaint();
}

void ZychanStripEditor::paint (Graphics& g)
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
    const Font wordmarkFont = fnt (18.0f, true);
    g.setFont (wordmarkFont);
    // Engraved drop shadow
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText ("zychanstrip", Rectangle<float> (logoX, logoY + 1.0f, 120.0f, 22.0f), Justification::centredLeft);
    // Crisp white lettering
    g.setColour (Colours::white);
    g.drawText ("zychanstrip", Rectangle<float> (logoX, logoY, 120.0f, 22.0f), Justification::centredLeft);

    // Version / channel label, immediately after the wordmark.
    // Convention (CMakeLists ZYCHANSTRIP_BETA): stable = "v1.0.1", prerelease = "v1.0.2-beta.1".
    // A prerelease renders amber so a beta build can never be mistaken for a release in a screenshot.
    const String versionLabel { ZYCHANSTRIP_VERSION_LABEL };
    const bool isBetaBuild = versionLabel.containsIgnoreCase ("beta");
    const Font versionFont = fnt (9.5f, true);

    GlyphArrangement wordmarkGa;
    wordmarkGa.addLineOfText (wordmarkFont, "zychanstrip", 0.0f, 0.0f);
    const float wordmarkW = wordmarkGa.getBoundingBox (0, -1, true).getRight();

    GlyphArrangement versionGa;
    versionGa.addLineOfText (versionFont, versionLabel, 0.0f, 0.0f);
    const float versionW = versionGa.getBoundingBox (0, -1, true).getWidth();

    const Rectangle<float> versionBox (logoX + wordmarkW + 7.0f, logoY + 6.5f, versionW + 11.0f, 14.0f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (versionBox, 3.0f);
    g.setColour ((isBetaBuild ? Colour (0xffe8a33d) : pal::engraveDim).withAlpha (0.55f));
    g.drawRoundedRectangle (versionBox, 3.0f, 0.8f);
    g.setFont (versionFont);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawText (versionLabel, versionBox.translated (0.0f, 0.6f), Justification::centred, false);
    g.setColour (isBetaBuild ? Colour (0xffffc45c) : Colour (0xffd8dde3));
    g.drawText (versionLabel, versionBox, Justification::centred, false);

    // Iconic ZY monogram logo (resembling the logo on zynamfx), placed clear of the version pill
    const Rectangle<float> zyMarkBox (versionBox.getRight() + 9.0f, logoY + 1.0f, 44.0f, 20.0f);
    drawZyLogo (g, zyMarkBox);

    // Subtitle badge — placed clear of the page toggle. It used to start right after the
    // monogram and ran underneath the RACK/METER toggle, so only "MAST" and "RIP" showed.
    const int subtitleX = pageToggle.getRight() + 14;
    const int subtitleRight = (int) (full.getWidth() * 0.5f) - 14;   // stop before the power jewel
    if (subtitleRight - subtitleX > 80)
    {
        g.setFont (fnt (9.0f, false));
        g.setColour (pal::engraveDim);
        g.drawText ("MASTERING & TRACKING STRIP",
                    Rectangle<int> (subtitleX, (int) (logoY + 3.0f), subtitleRight - subtitleX, 18),
                    Justification::centredLeft, true);
    }

    // Power jewel LED on header
    const Point<float> pwrC (full.getWidth() * 0.5f, header.getCentreY());    g.setColour (Colour (0xff08090a));
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
    g.drawText ("MASTER", Rectangle<float> (full.getWidth() - 230.0f, 10.0f, 64.0f, 12.0f), Justification::centredRight);
    g.setColour (Colour (0xffc8ccd0));
    g.drawText ("MASTER", Rectangle<float> (full.getWidth() - 230.0f, 9.0f, 64.0f, 12.0f), Justification::centredRight);
    // Dry/wet label beside the second bar
    g.setFont (fnt (10.0f, true));
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText ("DRY/WET", Rectangle<float> (full.getWidth() - 230.0f, 27.0f, 64.0f, 12.0f), Justification::centredRight);
    g.setColour (Colour (0xff9fe8dd));
    g.drawText ("DRY/WET", Rectangle<float> (full.getWidth() - 230.0f, 26.0f, 64.0f, 12.0f), Justification::centredRight);

    // 7. Plate shadows so the faceplates read as physically raised hardware
    for (int s = 0; s < 4; ++s)
    {
        const auto r = slotBounds (s);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillRoundedRectangle (r.toFloat().withTrimmedTop (kGripH).translated (0.0f, 4.0f), 6.0f);
    }
    g.restoreState();
}

void ZychanStripEditor::paintOverChildren (Graphics& g)
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
