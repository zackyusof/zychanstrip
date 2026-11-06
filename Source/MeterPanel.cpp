#include "MeterPanel.h"
#include "UiStyle.h"

using namespace juce;

namespace
{
// SSL Meter palette
const Colour bgDeep      { 0xff0d0d0d };
const Colour bgPanel     { 0xff141414 };
const Colour bgInset     { 0xff0a0a0a };
const Colour bronze      { 0xffa8865c };
const Colour bronzeDim   { 0xff6d5a42 };
const Colour inkWhite    { 0xffffffff };
const Colour inkDim      { 0xff9a9a9a };
const Colour meterGreen  { 0xff3ddc5f };
const Colour meterOrange { 0xffffa424 };
const Colour meterRed    { 0xffff3333 };
const Colour scopeGrid   { 0xff23301f };

Font sslSans (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

void sectionRule (Graphics& g, Rectangle<int> r, const String& title)
{
    g.setFont (sslSans (10.0f, true));
    g.setColour (bronze);
    g.drawText (title, r, Justification::centredLeft, false);
    GlyphArrangement ga; ga.addLineOfText (sslSans (10.0f, true), title, 0.0f, 0.0f);
    const auto tw = ga.getBoundingBox (0, -1, true).getWidth();
    g.setColour (bronzeDim);
    g.drawHorizontalLine (r.getCentreY(), (float) r.getX() + tw + 8.0f, (float) r.getRight());
}

/** Seven-segment style readout well. */
void digital (Graphics& g, Rectangle<int> r, const String& value, Colour ink, float size)
{
    g.setColour (bgInset);
    g.fillRoundedRectangle (r.toFloat(), 2.0f);
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r.toFloat(), 2.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (r.getBottom() - 1, (float) r.getX() + 2.0f, (float) r.getRight() - 2.0f);

    g.setFont (Font (FontOptions (size, Font::bold)));
    g.setColour (ink.withAlpha (0.25f));
    g.drawText (value, r, Justification::centred);
    g.setColour (ink);
    g.drawText (value, r.translated (0, -1), Justification::centred);
}
} // namespace

void MeterPanel::UpdateKnobLook::drawRotarySlider(Graphics& g, int x, int y, int w, int h,
                                                float position, float start, float end, Slider& slider)
{
    const Point<float> c((float)x+w*.5f, (float)y+h*.5f);
    const float r=jmin((float)w,(float)h)*.285f;
    const auto circle=Rectangle<float>(r*2,r*2).withCentre(c);
    for(int i=0;i<=20;++i) {
        const float a=start+(end-start)*i/20.f;
        Point<float> d(std::sin(a),-std::cos(a));
        g.setColour(Colour(0xff85877e));
        g.drawLine(Line<float>(c+d*(r+6),c+d*(r+9)),.7f);
    }
    for(int value : {16,33,50,100,200,500}) {
        const float a=start+(end-start)*(float)slider.valueToProportionOfLength(value);
        Point<float> d(std::sin(a),-std::cos(a));
        g.setColour(Colour(0xffe2e3d8));
        g.drawLine(Line<float>(c+d*(r+5),c+d*(r+10)),1.f);
        const auto p=c+d*(r+18);
        g.setFont(sslSans(8));
        g.drawText(String(value),Rectangle<float>(p.x-12,p.y-5,24,10),Justification::centred);
    }
    g.setColour(Colours::black.withAlpha(.75f));g.fillEllipse(circle.expanded(4).translated(1,3));
    g.setGradientFill(ColourGradient(Colour(0xff68615a),c.x-r,c.y-r,Colour(0xff151718),c.x+r,c.y+r,false));
    g.fillEllipse(circle.expanded(3));
    for(int i=0;i<48;++i) {
        const float a=MathConstants<float>::twoPi*i/48;
        Point<float> d(std::sin(a),-std::cos(a));
        g.setColour(Colours::black.withAlpha(.6f));g.drawLine(Line<float>(c+d*(r-1),c+d*(r+3)),.7f);
    }
    g.setGradientFill(ColourGradient(Colour(0xfff74924),c.x-r*.3f,c.y-r,Colour(0xffa91006),c.x+r*.4f,c.y+r,false));
    g.fillEllipse(circle);
    g.setColour(Colour(0xff5b130a));g.drawEllipse(circle,.8f);
    g.saveState();Path clip;clip.addEllipse(circle.reduced(1));g.reduceClipRegion(clip);
    g.setGradientFill(ColourGradient(Colours::white.withAlpha(.55f),c.x-r*.35f,c.y-r*.65f,
                                    Colours::transparentWhite,c.x+r*.4f,c.y+r*.25f,true));
    g.fillEllipse(circle);
    g.restoreState();
    const float angle=start+position*(end-start);
    const Point<float> d(std::sin(angle),-std::cos(angle));
    g.setColour(Colour(0xffffecd2));
    g.drawLine(Line<float>(c-d*r*.15f,c+d*r*.87f),3.f);
}

MeterPanel::MeterPanel (ZychanStripProcessor& p) : proc (p)
{
    setOpaque (true);
    updateInterval.setLookAndFeel(&updateKnobLook);
    updateInterval.setRotaryParameters(MathConstants<float>::pi*1.25f, MathConstants<float>::pi*2.75f, true);
    updateInterval.setName("Meter update interval");
    updateInterval.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
    updateInterval.setRange(16, 500, 1);
    updateInterval.setSkewFactorFromMidPoint(100);
    updateInterval.setValue(33, dontSendNotification);
    updateInterval.setTextBoxStyle(Slider::TextBoxBelow, false, 72, 18);
    updateInterval.setTextValueSuffix(" ms");
    updateInterval.setDoubleClickReturnValue(true, 33);
    updateInterval.setTooltip("GUI meter refresh interval. Lower values refresh faster; audio processing is unaffected. Double-click restores 33 ms.");
    updateInterval.onValueChange = [this] {
        if (onUpdateIntervalChanged) onUpdateIntervalChanged((int)updateInterval.getValue());
    };
    addAndMakeVisible(updateInterval);
    updateLabel.setText("UPDATE (ms)", dontSendNotification);
    updateLabel.setFont(sslSans(9, true));
    updateLabel.setColour(Label::textColourId, bronze);
    updateLabel.setJustificationType(Justification::centred);
    addAndMakeVisible(updateLabel);
}

const char* MeterPanel::scaleName (int index)
{
    switch (index)
    {
        case 1: return "K-20";
        case 2: return "K-14";
        case 3: return "K-12";
        default: return "SETTLE";
    }
}

void MeterPanel::setScale (int index)
{
    scale = jlimit (0, 3, index);
    repaint();
}

float MeterPanel::normForScale (float db) const
{
    // Linear is -60..+3 dBFS; K-scales shift the 0 dB reference (K-20 => -20 dBFS = 0).
    float top = 3.0f, bottom = -60.0f;
    switch (scale)
    {
        case 1: top = 20.0f - 20.0f; bottom = -60.0f; break;   // K-20: 0 mark at -20 dBFS
        case 2: top = 0.0f;  bottom = -60.0f; break;
        case 3: top = 0.0f;  bottom = -60.0f; break;
        default: break;
    }
    const float ref = (scale == 0) ? 0.0f : (scale == 1 ? 20.0f : 14.0f);
    const float v = db + (scale == 0 ? 0.0f : -ref + ref) ;   // dB below 0 for display
    const float shifted = db + ref * 0.0f;
    (void) v; (void) shifted;
    return jlimit (0.0f, 1.0f, (db - bottom) / (top - bottom));
}

void MeterPanel::tick()
{
    auto& m = proc.getMeters();
    // The analyser FFT runs HERE, on the message thread, never in the audio callback
    // (prior-art report F5). It consumes one published snapshot if the audio thread has one.
    m.runAnalysis();
    for (int c = 0; c < 2; ++c)
    {
        displayPeakDb[(size_t) c] = m.peakDb (c);
        displayHoldDb[(size_t) c] = m.holdDb (c);
        displayRmsDb[(size_t) c]  = m.rmsDb (c);
    }
    m.copyScope (scopePoints);
    const auto& bd = m.bands();
    const auto& bh = m.bandHolds();
    // Averaging control: previously the SETTLE/AVG indicator was painted but nothing read it,
    // so every position behaved identically. Apply one-pole smoothing to the analyser bands
    // here, on the GUI thread, at the panel's own tick rate — the DSP metering tap stays
    // untouched and read-only.
    static constexpr float avgCoef[] { 1.00f, 0.55f, 0.25f, 0.10f };   // SETTLE, FAST, MED, AVG
    const float a = 1.f - std::pow(1.f - avgCoef[(size_t) jlimit (0, 3, rtaAverage)], (float)updateInterval.getValue()/33.f);
    for (int i = 0; i < RackMeters::kBands && i < (int) bd.size(); ++i)
    {
        const float target = bd[(size_t) i];
        bandDb[(size_t) i] += (target - bandDb[(size_t) i]) * a;
        bandHoldDb[(size_t) i] = (i < (int) bh.size()) ? bh[(size_t) i] : -100.0f;
    }
    const float c = m.correlation();
    corrSmoothed += (c - corrSmoothed) * (1.f-std::pow(.6f,(float)updateInterval.getValue()/33.f));

    // Never repaint while the editor is being torn down. A Timer firing during app quit used to
    // post a message after the editor's handles were gone (crash in MessageQueue::post on a null
    // mutex). Skipping repaint while invisible or mid-destruction makes the timer harmless.
    if (isShowing() && ! isCurrentlyBlockedByAnotherModalComponent())
        repaint();
}

void MeterPanel::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    headerBar = r.removeFromTop (34);
    r.removeFromTop (10);

    auto left = r.removeFromLeft (r.getWidth() * 3 / 5);
    footerArea = left.removeFromBottom (18);
    meterArea = left;
    r.removeFromLeft (12);
    // Right column: phase scope on top, correlation, then the 31-band analyser.
    scopeArea = r.removeFromTop (r.getHeight() * 55 / 100);
    r.removeFromTop (8);
    corrArea = r.removeFromTop (58);
    r.removeFromTop (6);
    rtaArea = r;

    const auto hb = headerBar;
    backBtn  = { hb.getX(), hb.getCentreY() - 11, 68, 22 };
    resetBtn = { hb.getCentreX() - 42, hb.getCentreY() - 11, 84, 22 };
    scaleBtn = { hb.getRight() - 104, hb.getCentreY() - 11, 104, 22 };
    // The right bar occupies the left half of its channel column. Centre the
    // update control in the remaining half, immediately before the phase scope.
    const int columnWidth=meterArea.getWidth()/2;
    const int updateX=meterArea.getX()+7*columnWidth/4;
    const int knobWidth=jmin(122,columnWidth/2-10);
    const int knobY=scopeArea.getCentreY()-60;
    updateInterval.setBounds(updateX-knobWidth/2,knobY,knobWidth,126);
    updateLabel.setBounds(updateX-knobWidth/2,knobY-22,knobWidth,18);
}

void MeterPanel::mouseDown (const MouseEvent& e)
{
    if (backBtn.contains (e.getPosition()))       { pressed = Pressed::Back;  repaint(); return; }
    if (resetBtn.contains (e.getPosition()))      { pressed = Pressed::Reset; repaint(); return; }
    if (scaleBtn.contains (e.getPosition()))        setScale ((scale + 1) % 4);
}

void MeterPanel::mouseUp (const MouseEvent& e)
{
    const auto was = pressed;
    pressed = Pressed::None;
    repaint();

    // Momentary buttons fire on release, and only when released over the same button.
    if (was == Pressed::Back && backBtn.contains (e.getPosition()))
    {
        if (onBack) onBack();
    }
    else if (was == Pressed::Reset && resetBtn.contains (e.getPosition()))
    {
        if (onReset) onReset();
        else         proc.getMeters().reset();
    }
}

void MeterPanel::paint (Graphics& g)
{
    g.fillAll (bgDeep);
    g.setGradientFill (ColourGradient (bgPanel, 0.0f, 0.0f, bgDeep, 0.0f, (float) getHeight(), false));
    g.fillRect (getLocalBounds());

    paintHeader (g);

    const auto panel = getLocalBounds().reduced (14, 12).withTrimmedTop (44).toFloat();
    g.setColour (bgPanel);
    g.fillRoundedRectangle (panel, 4.0f);
    g.setColour (bronzeDim.withAlpha (0.5f));
    g.drawRoundedRectangle (panel, 4.0f, 1.0f);

    paintMeters (g);
    paintScope (g);
    paintCorrelation (g);
    paintRta (g);
}

void MeterPanel::setRtaAveraging (int index)
{
    rtaAverage = jlimit (0, 3, index);
    repaint();
}

void MeterPanel::paintRta (Graphics& g)
{
    auto area = rtaArea;
    if (area.getHeight() < 30) return;

    sectionRule (g, area.removeFromTop (14), "31-BAND ANALYSER");

    auto plot = area.reduced (2, 2);
    // Reserve a label strip at the bottom for frequency markings.
    auto labels = plot.removeFromBottom (10);
    plot.removeFromRight (2);

    // Inset well with dB grid
    g.setColour (bgInset);
    g.fillRect (plot);
    g.setColour (Colour (0xff242424));
    g.drawRect (plot, 1.0f);

    const float topDb = 0.0f, bottomDb = -72.0f;
    const int gridLines[] = { -6, -12, -24, -36, -48, -60 };
    g.setFont (sslSans (7.0f, true));
    for (int dbv : gridLines)
    {
        const float t = ((float) dbv - bottomDb) / (topDb - bottomDb);
        const float y = (float) plot.getBottom() - (float) plot.getHeight() * t;
        g.setColour (Colour (0xff1e1e1e));
        g.drawHorizontalLine ((int) y, (float) plot.getX(), (float) plot.getRight());
        g.setColour (inkDim.withAlpha (0.6f));
        g.drawText (String (dbv), Rectangle<float> ((float) plot.getX() + 1.0f, y - 5.0f, 18.0f, 9.0f),
                    Justification::centredLeft, false);
    }

    const int n = RackMeters::kBands;
    const float barW = (float) plot.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float db = bandDb[(size_t) i];
        const float t = jlimit (0.0f, 1.0f, (db - bottomDb) / (topDb - bottomDb));
        const float x = (float) plot.getX() + (float) i * barW;
        const float h = (float) plot.getHeight() * t;
        const Rectangle<float> bar (x + 0.6f, (float) plot.getBottom() - h, jmax (1.0f, barW - 1.2f), h);

        // Colour by band position: lows green, mids amber, highs orange
        const Colour c = db > -6.0f ? meterRed
                       : (db > -18.0f ? meterOrange : meterGreen);
        g.setColour (c.withAlpha (0.92f));
        g.fillRect (bar);

        // Peak-hold cap
        const float ht = jlimit (0.0f, 1.0f, (bandHoldDb[(size_t) i] - bottomDb) / (topDb - bottomDb));
        const float hy = (float) plot.getBottom() - (float) plot.getHeight() * ht;
        g.setColour (inkWhite.withAlpha (0.85f));
        g.fillRect (x + 0.6f, hy - 1.0f, jmax (1.0f, barW - 1.2f), 1.4f);
    }

    // Frequency marks at octave centres
    const struct { const char* txt; int band; } marks[]
        { { "20", 0 }, { "100", 7 }, { "1k", 19 }, { "10k", 29 } };
    g.setFont (sslSans (7.0f, true));
    g.setColour (inkDim);
    for (auto& mk : marks)
    {
        const float x = (float) plot.getX() + ((float) mk.band + 0.5f) * barW;
        g.drawText (mk.txt, Rectangle<float> (x - 12.0f, (float) labels.getY(), 24.0f, 9.0f),
                    Justification::centred, false);
    }

    // Averaging indicator on the right of the section rule
    g.setFont (sslSans (7.5f, true));
    g.setColour (bronze);
    const char* avgName[] { "SETTLE", "FAST", "MED", "AVG" };
    g.drawText (avgName[jlimit (0, 3, rtaAverage)],
                Rectangle<float> ((float) area.getRight() - 70.0f, (float) rtaArea.getY(), 42.0f, 12.0f),
                Justification::centredRight, false);
}

void MeterPanel::paintHeader (Graphics& g)
{
    // Brand marking: uppercase serif, wide tracking
    g.setColour (bronze);
    g.fillRect (headerBar.getX(), headerBar.getBottom() - 1, headerBar.getWidth(), 1);

    // Return-to-rack button: same teal/black momentary pill as the rack page controls.
    rackui::paintPill (g, backBtn.toFloat(), pressed == Pressed::Back, "<  RACK", 9.0f);

    g.setFont (Font (FontOptions (15.0f, Font::bold)));
    const String brand = "S O L I D   S T A T E   L O G I C";
    g.setColour (inkWhite);
    g.drawText (brand, backBtn.getRight() + 10, headerBar.getY(),
                headerBar.getWidth() / 2, headerBar.getHeight(),
                Justification::centredLeft, false);

    g.setFont (sslSans (9.0f, true));
    g.setColour (inkDim);
    g.drawText ("METER", backBtn.getRight() + 18, headerBar.getBottom() - 14, 100, 12, Justification::centredLeft, false);

    // Scale selector
    g.setColour (bgInset);
    g.fillRoundedRectangle (scaleBtn.toFloat(), 2.0f);
    g.setColour (bronze.withAlpha (0.45f));
    g.drawRoundedRectangle (scaleBtn.toFloat(), 2.0f, 1.0f);
    g.setFont (sslSans (9.0f, true));
    g.setColour (bronze);
    g.drawText (String ("<  ") + scaleName (scale) + "  >", scaleBtn, Justification::centred, false);

    // RESET — same teal/black momentary pill as the rack header
    rackui::paintPill (g, resetBtn.toFloat(), pressed == Pressed::Reset, "RESET", 9.0f);
}

void MeterPanel::paintMeters (Graphics& g)
{
    auto area = meterArea;
    sectionRule (g, area.removeFromTop (16), "TRUE PEAK / RMS");

    const int chW = area.getWidth() / 2;
    for (int c = 0; c < 2; ++c)
    {
        auto col = area.withX (area.getX() + c * chW).withWidth (chW).reduced (8, 0);

        // Readouts directly above the bar (bar occupies the left half of this column)
        auto ro = col.removeFromTop (20);
        auto barRo = ro.removeFromLeft (col.getWidth() / 2 - 6);
        digital (g, barRo.removeFromRight (barRo.getWidth() / 2).reduced (1, 0),
                 String (displayHoldDb[(size_t) c], 1), meterGreen, 9.5f);
        digital (g, barRo.reduced (1, 0), String (displayPeakDb[(size_t) c], 1), inkWhite, 9.5f);

        col.removeFromTop (6);
        auto labelRow = col.removeFromTop (12);
        g.setFont (sslSans (8.0f, true));
        g.setColour (inkDim);
        g.drawText (c == 0 ? "L  MAX / PEAK" : "R  MAX / PEAK", labelRow, Justification::centred, false);

        col.removeFromTop (4);

        // Bar
        const auto bar = col.withTrimmedRight (col.getWidth() / 2).reduced (6, 0).toFloat();
        g.setColour (bgInset);
        g.fillRect (bar);
        g.setColour (Colours::black.withAlpha (0.9f));
        g.drawRect (bar, 1.0f);

        const int segs = 40;
        const float segH = bar.getHeight() / (float) segs;
        const float peakN = normForScale (displayPeakDb[(size_t) c]);
        const float rmsN  = normForScale (displayRmsDb[(size_t) c]);

        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) i / (float) (segs - 1);       // 0 bottom .. 1 top
            const Colour c2 = t > 0.93f ? meterRed : (t > 0.80f ? meterOrange : meterGreen);
            const Rectangle<float> seg (bar.getX() + 2.0f, bar.getBottom() - (float) (i + 1) * segH + 0.6f,
                                        bar.getWidth() - 4.0f, segH - 1.2f);
            if (t <= peakN)
            {
                g.setColour (c2);
                g.fillRect (seg);
            }
            else
            {
                g.setColour (c2.withAlpha (0.10f));
                g.fillRect (seg);
            }
            if (i == (int) std::round (rmsN * (float) (segs - 1)))
            {
                g.setColour (inkWhite.withAlpha (0.85f));
                g.fillRect (seg.withHeight (1.5f));
            }
        }

        // Max-hold marker
        const auto holdY = bar.getBottom() - bar.getHeight() * normForScale (displayHoldDb[(size_t) c]);
        g.setColour (displayHoldDb[(size_t) c] > -0.1f ? meterRed : bronze);
        g.fillRect (bar.getX() - 3.0f, holdY - 1.0f, bar.getWidth() + 6.0f, 2.0f);
    }

    // Short-term loudness readout, in the reserved footer row
    const auto lufs = proc.getMeters().shortTermLufs();
    g.setFont (sslSans (9.0f, true));
    g.setColour (inkDim);
    g.drawText (String ("SHORT-TERM  ") + (lufs > -99.0f ? String (lufs, 1) + " LUFS" : String ("--")),
                footerArea, Justification::centredLeft, false);
}

void MeterPanel::paintScope (Graphics& g)
{
    auto area = scopeArea;
    sectionRule (g, area.removeFromTop (16), "PHASE SCOPE");

    auto sq = area.reduced (4);
    const int side = jmin (sq.getWidth(), sq.getHeight());
    const Rectangle<float> circle ((float) sq.getCentreX() - (float) side * 0.5f,
                                   (float) sq.getCentreY() - (float) side * 0.5f, (float) side, (float) side);

    g.setColour (bgInset);
    g.fillRect (circle);
    g.setColour (scopeGrid);
    g.drawRect (circle, 1.0f);
    g.drawEllipse (circle.reduced (1.0f), 1.0f);
    g.drawLine (circle.getCentreX(), circle.getY(), circle.getCentreX(), circle.getBottom(), 0.8f);
    g.drawLine (circle.getX(), circle.getCentreY(), circle.getRight(), circle.getCentreY(), 0.8f);

    g.setColour (meterOrange.withAlpha (0.85f));
    for (auto& p : scopePoints)
    {
        const float x = circle.getCentreX() + p.x * circle.getWidth() * 0.46f;
        const float y = circle.getCentreY() - p.y * circle.getHeight() * 0.46f;
        g.fillRect (Rectangle<float> (x, y, 1.6f, 1.6f));
    }
}

void MeterPanel::paintCorrelation (Graphics& g)
{
    auto area = corrArea;
    sectionRule (g, area.removeFromTop (16), "CORRELATION");

    auto bar = area.reduced (10, 0).withHeight (10).withY (area.getY() + 2);
    g.setColour (bgInset);
    g.fillRoundedRectangle (bar.toFloat(), 2.0f);
    g.setColour (Colour (0xff2a2a2a));
    g.drawRoundedRectangle (bar.toFloat(), 2.0f, 1.0f);

    // -1 == out of phase (red) .. +1 == mono (green)
    const float t = (corrSmoothed + 1.0f) * 0.5f;
    const auto fill = bar.toFloat().withWidth ((float) bar.getWidth() * t);
    g.setGradientFill (ColourGradient (meterRed, (float) bar.getX(), 0.0f,
                                       meterGreen, (float) bar.getRight(), 0.0f, false));
    g.fillRoundedRectangle (fill, 2.0f);

    g.setColour (inkWhite);
    g.fillRect ((float) bar.getCentreX() - 0.5f, (float) bar.getY() - 2.0f, 1.0f, (float) bar.getHeight() + 4.0f);

    g.setFont (sslSans (8.0f, true));
    g.setColour (inkDim);
    const float tickY = (float) bar.getBottom() + 2.0f;
    g.drawText ("-1", Rectangle<float> ((float) bar.getX(), tickY, 20.0f, 10.0f), Justification::centredLeft, false);
    g.drawText ("0", Rectangle<float> ((float) bar.getCentreX() - 10.0f, tickY, 20.0f, 10.0f), Justification::centred, false);
    g.drawText ("+1", Rectangle<float> ((float) bar.getRight() - 20.0f, tickY, 20.0f, 10.0f), Justification::centredRight, false);

    g.setColour (corrSmoothed < 0.0f ? meterRed : meterGreen);
    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    g.drawText (String (corrSmoothed, 2), Rectangle<float> ((float) bar.getRight() - 60.0f, tickY, 60.0f, 12.0f), Justification::centredRight, false);
}
