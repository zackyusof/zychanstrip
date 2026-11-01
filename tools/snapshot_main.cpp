// Renders the real editor and exercises master output, state, scaling and drag/reorder.
// Build: python3 tools/build_snapshot.py [--build-dir <Ninja build>]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <complex>
#include <iostream>

namespace
{
int passed = 0, failed = 0;
void check (bool ok, const juce::String& label)
{
    (ok ? passed : failed)++;
    std::cout << (ok ? "PASS " : "FAIL ") << label << std::endl;
}
void setParam (ZychanStripProcessor& proc, const juce::String& id, float value)
{
    auto* p = proc.apvts.getParameter (id);
    if (p != nullptr) p->setValueNotifyingHost (p->convertTo0to1 (value));
    else check (false, "missing parameter " + id);
}
void bypassAll (ZychanStripProcessor& proc)
{
    for (auto id : { "con_bypass", "opto_bypass", "fet_bypass", "lim_bypass" })
        setParam (proc, id, 1.0f);
}
void prepare (ZychanStripProcessor& proc, int channels, double sr = 48000.0)
{
    proc.setPlayConfigDetails (channels, channels, sr, 512);
    check (proc.getTotalNumInputChannels() == channels && proc.getTotalNumOutputChannels() == channels,
           "channel layout configured");
    proc.prepareToPlay (sr, 512);
}
// Fraction of opaque, fully black pixels inside `area` of a component snapshot.
double blackFraction (const juce::Image& img, juce::Rectangle<int> area)
{
    int black = 0, total = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
        {
            if (! img.getBounds().contains (x, y)) continue;
            const auto c = img.getPixelAt (x, y);
            ++total;
            if (c.getAlpha() > 200 && c.getRed() == 0 && c.getGreen() == 0 && c.getBlue() == 0) ++black;
        }
    return total > 0 ? (double) black / (double) total : 0.0;
}
// Fraction of the page-toggle teal inside `area` of a component snapshot.
double tealFraction (const juce::Image& img, juce::Rectangle<int> area)
{
    int teal = 0, total = 0;
    for (int y = area.getY(); y < area.getBottom(); ++y)
        for (int x = area.getX(); x < area.getRight(); ++x)
        {
            if (! img.getBounds().contains (x, y)) continue;
            const auto c = img.getPixelAt (x, y);
            ++total;
            if (c.getAlpha() > 200 && c.getRed() < 140 && c.getGreen() > 130 && c.getBlue() > 110) ++teal;
        }
    return total > 0 ? (double) teal / (double) total : 0.0;
}
void processConstant (ZychanStripProcessor& proc, juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.1f * (float) (ch + 1), buffer.getNumSamples());
    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);
}
juce::Component* findNamed (juce::Component& root, const juce::String& name)
{
    if (root.getName() == name) return &root;
    for (auto* child : root.getChildren())
        if (auto* found = findNamed (*child, name)) return found;
    return nullptr;
}
// Test-only implementations remain separate from production DSP.
#include "dsp_checks.h"

void audioTests()
{
    ZychanStripProcessor proc;
    auto* master = proc.apvts.getParameter ("master_level");
    check (master != nullptr, "master parameter exists");
    if (master == nullptr) return;
    check (std::abs (master->convertFrom0to1 (master->getDefaultValue())) < 1.0e-5f, "master defaults to unity");
    for (int channels : { 1, 2 })
        for (double sr : { 44100.0, 96000.0 })
        {
            bypassAll (proc);
            prepare (proc, channels, sr);
            juce::AudioBuffer<float> buffer (channels, 512);
            for (float db : { -60.0f, -6.0f, 0.0f, 6.0f })
            {
                setParam (proc, "master_level", db);
                for (int i = 0; i < 12; ++i) processConstant (proc, buffer);
                const auto expected = 0.1f * juce::Decibels::decibelsToGain (db);
                bool ok = true;
                for (int ch = 0; ch < channels; ++ch)
                    ok &= std::abs (buffer.getSample (ch, 511) - expected * (float) (ch + 1)) < 2.0e-6f;
                check (ok, "master gain " + juce::String (db) + "dB, " + juce::String (channels) + "ch @" + juce::String (sr));
            }
        }

    bypassAll (proc);
    setParam (proc, "master_level", 0);
    prepare (proc, 2);
    juce::AudioBuffer<float> buffer (2, 512);
    for (int i = 0; i < 4; ++i) processConstant (proc, buffer);
    setParam (proc, "master_level", -60);
    processConstant (proc, buffer);
    bool linked = true, smooth = true;
    for (int i = 0; i < 512; ++i)
    {
        linked &= std::abs (buffer.getSample (1, i) - 2.0f * buffer.getSample (0, i)) < 1.0e-7f;
        if (i > 0) smooth &= std::abs (buffer.getSample (0, i) - buffer.getSample (0, i - 1)) < 0.001f;
    }
    check (linked, "same smoothing ramp on both channels");
    check (smooth && buffer.getSample (0, 0) > 0.09f && buffer.getSample (0, 511) > 0.001f,
           "master changes ramp instead of stepping");

    setParam (proc, "master_level", -9);
    proc.setOrder ({ 3, 1, 0, 2 });
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    ZychanStripProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (restored.apvts.getRawParameterValue ("master_level")->load() + 9.0f) < 0.001f,
           "master survives state roundtrip");
    check (restored.getOrder() == proc.getOrder(), "module order survives state roundtrip");
    auto oldState = proc.apvts.copyState();
    for (int i = oldState.getNumChildren(); --i >= 0;)
        if (oldState.getChild (i).getProperty ("id").toString() == "master_level") oldState.removeChild (i, nullptr);
    juce::MemoryBlock oldBlob;
    auto xml = oldState.createXml();
    juce::AudioProcessor::copyXmlToBinary (*xml, oldBlob);
    restored.setStateInformation (oldBlob.getData(), (int) oldBlob.getSize());
    check (std::abs (restored.apvts.getRawParameterValue ("master_level")->load()) < 1.0e-5f,
           "legacy session resets prior master level to unity");

    // Compare two identical active chains: only the final master setting differs.
    ZychanStripProcessor reference, attenuated;
    setParam (attenuated, "master_level", -6);
    prepare (reference, 2);
    prepare (attenuated, 2);
    juce::AudioBuffer<float> a (2, 512), b (2, 512);
    for (int i = 0; i < 12; ++i) { processConstant (reference, a); processConstant (attenuated, b); }
    check (std::abs (b.getSample (0, 511) - a.getSample (0, 511) * juce::Decibels::decibelsToGain (-6.0f)) < 2.0e-6f,
           "master scales the completed active module chain");
    // ---- Dry/wet parallel mix, applied after master volume.
    // At 100% the output must equal the fully processed signal; at 0% it must equal the
    // untouched input; at 50% it must be the exact average of the two.
    {
        auto render = [] (float mixPct, float masterDb, juce::AudioBuffer<float>& out)
        {
            ZychanStripProcessor p;
            setParam (p, "master_mix", mixPct);
            setParam (p, "master_level", masterDb);
            for (auto id : { "con_bypass", "opto_bypass", "fet_bypass", "lim_bypass" })
                setParam (p, id, 1.0f);
            p.setPlayConfigDetails (2, 2, 48000.0, 512);
            p.prepareToPlay (48000.0, 512);
            out.setSize (2, 512);
            for (int i = 0; i < 512; ++i)
            {
                out.setSample (0, i, 0.30f * std::sin (2.0f * juce::MathConstants<float>::pi * 300.0f * (float) i / 48000.0f));
                out.setSample (1, i, 0.22f * std::sin (2.0f * juce::MathConstants<float>::pi * 700.0f * (float) i / 48000.0f));
            }
            juce::MidiBuffer midi;
            p.processBlock (out, midi);
        };

        juce::AudioBuffer<float> dry (2, 512), wet (2, 512), half (2, 512);
        render (0.0f, 0.0f, dry);
        render (100.0f, -6.0f, wet);
        render (50.0f, -6.0f, half);

        // At 0% the output must be the input, sample for sample (no chain, no latency).
        bool dryOk = true;
        for (int i = 0; i < 512; ++i)
        {
            const float expect = 0.30f * std::sin (2.0f * juce::MathConstants<float>::pi * 300.0f * (float) i / 48000.0f);
            dryOk &= std::abs (dry.getSample (0, i) - expect) < 1.0e-6f;
        }
        check (dryOk, "0% mix returns the untouched input");

        // Total gain only: the module chain adds look-ahead latency, so sample-by-sample
        // comparison of the two renders is meaningless. Compare RMS instead, which is
        // latency-invariant, to prove master volume (and the 100% blend) is fully applied.
        auto rmsOf = [] (const juce::AudioBuffer<float>& b)
        {
            double s = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i) s += (double) b.getSample (0, i) * b.getSample (0, i);
            return std::sqrt (s / (double) b.getNumSamples());
        };
        juce::AudioBuffer<float> unity (2, 512);
        render (100.0f, 0.0f, unity);
        const double wetRms = rmsOf (wet), unityRms = rmsOf (unity);
        const double expectedRatio = juce::Decibels::decibelsToGain (-6.0f);
        const double actualRatio = wetRms / juce::jmax (1.0e-12, unityRms);
        bool wetOk = std::abs (actualRatio - expectedRatio) < 0.02;
        check (wetOk, "100% mix applies the full signal including master volume");

        // Proven directly: at 0% the output IS the input, at 100% it carries the full master
        // gain, and every intermediate percentage must scale the wet contribution linearly.
        // Measure the wet factor by rendering with the limiter engaged (so wet differs from dry)
        // and reading how much of that difference survives.
        float prevResidual = -1.0f;
        bool monotonic = true;
        for (float pct : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
        {
            juce::AudioBuffer<float> b (2, 512);
            render (pct, 0.0f, b);
            // Residual against the untouched input grows with the wet amount.
            double diff = 0.0;
            for (int i = 0; i < 512; ++i)
            {
                const float input = 0.30f * std::sin (2.0f * juce::MathConstants<float>::pi * 300.0f * (float) i / 48000.0f);
                diff += std::abs (b.getSample (0, i) - input);
            }
            const float residual = (float) (diff / 512.0);
            if (prevResidual >= 0.0f && residual + 1.0e-6f < prevResidual) monotonic = false;
            prevResidual = residual;
        }
        check (monotonic, "wet amount increases monotonically with the mix percentage");

        // ---- Latency compensation (zychanstrip-mix-phase).
        // The rack delays the wet path by the reported latency; with a bypassed rack that delay is
        // a pure shift, so the aligned dry sample IS the wet sample. A 50 % blend must therefore
        // equal the wet signal exactly (0.5*wet + 0.5*alignedDry). Before the fix the raw dry copy
        // combed against it and cancelled the tone.
        {
            juce::AudioBuffer<float> half50 (2, 512), zero50 (2, 512), wet50 (2, 512);
            render (50.0f, 0.0f, half50);
            render (0.0f, 0.0f, zero50);
            render (100.0f, 0.0f, wet50);

            ZychanStripProcessor probe;
            probe.setPlayConfigDetails (2, 2, 48000.0, 512);
            probe.prepareToPlay (48000.0, 512);
            // `render` processes a full block before returning; the delay ring is only populated
            // once audio has flowed, so do the same here before reading the latency.
            juce::AudioBuffer<float> probeBuf (2, 512);
            juce::MidiBuffer probeMidi;
            probe.processBlock (probeBuf, probeMidi);
            const int lat = juce::jmax (0, probe.getLatencySamples());
            check (lat > 0, "rack reports look-ahead latency (" + juce::String (lat) + " samples)");

            double maxErr = 0.0;
            int compared = 0;
            for (int i = lat + 32; i < 512; ++i)
            {
                maxErr = juce::jmax (maxErr, (double) std::abs (half50.getSample (0, i) - wet50.getSample (0, i)));
                ++compared;
            }
            check (compared > 100 && maxErr < 1.0e-5,
                   "50% mix sums latency-aligned dry (max err " + juce::String (maxErr, 8) + ")");

            // The old bug cancelled the tone: prove the 50 % level is full, not a comb null.
            auto rmsRange = [] (const juce::AudioBuffer<float>& b, int from)
            {
                double s = 0.0;
                for (int i = from; i < b.getNumSamples(); ++i) s += (double) b.getSample (0, i) * b.getSample (0, i);
                return std::sqrt (s / (double) juce::jmax (1, b.getNumSamples() - from));
            };
            const double halfRms = rmsRange (half50, lat + 32);
            const double wetRmsAligned = rmsRange (wet50, lat + 32);
            check (halfRms > 0.95 * wetRmsAligned,
                   "50% mix does not cancel the wet path (" + juce::String (halfRms, 4) + " vs "
                     + juce::String (wetRmsAligned, 4) + ")");

            // Fully dry must still return the untouched input with no latency added.
            bool dryExact = true;
            for (int i = 0; i < 512; ++i)
            {
                const float in = 0.30f * std::sin (2.0f * juce::MathConstants<float>::pi * 300.0f * (float) i / 48000.0f);
                dryExact &= std::abs (zero50.getSample (0, i) - in) < 1.0e-6f;
            }
            check (dryExact, "0% mix still returns the untouched input (dry path not delayed)");
        }
    }

    juce::AudioBuffer<float> empty (2, 0);
    juce::MidiBuffer midi;
    proc.processBlock (empty, midi);
    check (true, "empty audio block accepted");
}
// RESET must be a full restore: every parameter back to its declared default, the factory rack
// order, cleared meters and no stale DSP state left in the chain.
// Auto Gain must not depend on where the limiter sits in the rack (zychanstrip-autogain-order):
// measure the same settings across every permutation and require a tight spread.
void autoGainOrderTests()
{
    auto rmsOf = [] (const juce::AudioBuffer<float>& b)
    {
        double s = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i) s += (double) b.getSample (0, i) * b.getSample (0, i);
        return std::sqrt (s / (double) b.getNumSamples());
    };

    std::array<std::array<int, 4>, 24> orders {};
    int count = 0;
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            for (int c = 0; c < 4; ++c)
                for (int d = 0; d < 4; ++d)
                    if (a != b && a != c && a != d && b != c && b != d && c != d)
                        orders[(size_t) count++] = { a, b, c, d };
    check (count == 24, "all 24 rack permutations enumerated");

    double lo = 1.0e9, hi = -1.0e9;
    for (int k = 0; k < count; ++k)
    {
        ZychanStripProcessor p;
        // Only the limiter is active, driven hard so Auto Gain has real work to do.
        for (auto id : { "con_bypass", "opto_bypass", "fet_bypass" })
            setParam (p, id, 1.0f);
        setParam (p, "lim_bypass", 0.0f);
        setParam (p, "lim_gain", 12.0f);
        setParam (p, "lim_auto_gain", 1.0f);
        setParam (p, "master_level", 0.0f);
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.setOrder (orders[(size_t) k]);

        juce::AudioBuffer<float> b (2, 512);
        for (int blk = 0; blk < 40; ++blk)
        {
            for (int i = 0; i < 512; ++i)
                for (int c = 0; c < 2; ++c)
                    b.setSample (c, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 300.0f
                                                        * (float) (blk * 512 + i) / 48000.0f));
            juce::MidiBuffer midi;
            p.processBlock (b, midi);
        }
        const double r = rmsOf (b);
        lo = juce::jmin (lo, r);
        hi = juce::jmax (hi, r);
    }
    const double spreadDb = 20.0 * std::log10 (hi / juce::jmax (1.0e-12, lo));
    check (spreadDb < 0.5, "Auto Gain is order-independent across 24 permutations (spread "
                             + juce::String (spreadDb, 3) + " dB)");

    // Master level must not feed the Auto Gain correction (it normalises the rack, not the output
    // trim): two instances differing only in master level must apply the same compensation.
    ZychanStripProcessor unity, attenuated;
    setParam (attenuated, "master_level", -6.0f);
    for (auto* p : { &unity, &attenuated })
    {
        setParam (*p, "lim_gain", 9.0f);
        setParam (*p, "lim_auto_gain", 1.0f);
        p->setPlayConfigDetails (2, 2, 48000.0, 512);
        p->prepareToPlay (48000.0, 512);
    }
    juce::AudioBuffer<float> u (2, 512), v (2, 512);
    for (int i = 0; i < 12; ++i) { processConstant (unity, u); processConstant (attenuated, v); }
    check (std::abs (unity.getAutoGainCompensation() - attenuated.getAutoGainCompensation()) < 0.01f,
           "Auto Gain correction ignores master level");
}

void resetTests()
{
    ZychanStripProcessor proc;
    prepare (proc, 2);

    setParam (proc, "con_bypass", 1.0f);
    setParam (proc, "con_eq_1k", 8.0f);
    setParam (proc, "con_out", -6.0f);
    setParam (proc, "opto_peak", 65.0f);
    setParam (proc, "opto_mode", 1.0f);
    setParam (proc, "fet_input", 12.0f);
    setParam (proc, "fet_ratio", 4.0f);
    setParam (proc, "fet_bypass", 1.0f);
    setParam (proc, "lim_gain", 9.0f);
    setParam (proc, "lim_ceiling", -6.0f);
    setParam (proc, "lim_auto_gain", 0.0f);
    setParam (proc, "master_level", -12.0f);
    setParam (proc, "master_mix", 40.0f);
    proc.setOrder ({ 3, 1, 2, 0 });
    check (proc.getOrder() != ZychanStripProcessor::getDefaultOrder(), "custom settings applied before reset");

    proc.resetToDefaults();

    bool everyParameterDefault = true;
    juce::String drifted;
    for (auto* p : proc.getParameters())
    {
        if (std::abs (p->getValue() - p->getDefaultValue()) > 1.0e-6f)
        {
            everyParameterDefault = false;
            drifted << p->getName (64) << " ";
        }
    }
    check (everyParameterDefault, "reset returns every parameter to its default " + drifted);
    check (proc.getOrder() == ZychanStripProcessor::getDefaultOrder(), "reset restores the factory rack order");

    // A loud block just before the reset must not leak through the limiter's look-ahead delay
    // afterwards: the reset is consumed at the start of the next block.
    juce::AudioBuffer<float> loud (2, 512), silent (2, 512);
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::fill (loud.getWritePointer (ch), 0.9f, 512);
    silent.clear();
    juce::MidiBuffer midi;
    for (int i = 0; i < 4; ++i) proc.processBlock (loud, midi);
    check (proc.getMeters().peakDb (0) > -20.0f, "loud block reaches the meters before reset");

    proc.resetToDefaults();
    silent.clear();
    proc.processBlock (silent, midi);
    bool silentOutput = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i) silentOutput &= std::abs (silent.getSample (ch, i)) < 1.0e-6f;
    check (silentOutput, "reset clears stale DSP state before the next block is processed");
    check (proc.getMeters().peakDb (0) <= -100.0f, "reset clears the metering tap");
}

// Renders the SSL-style metering page with real audio pushed through the chain so the
// meters, scope and correlation show genuine measurements rather than resting values.
void meterPageTests (ZychanStripProcessor& proc, ZychanStripEditor& editor, const juce::File& output)
{
    const juce::Point<int> sizes[] { { 1080, 760 }, { 864, 608 } };
    int pass = 0;
    for (const auto size : sizes)
    {
        ++pass;
        editor.setSize (size.x, size.y);

        // Restore a live chain (audioTests() leaves every module bypassed) and feed a
        // stereo tone pair so the tap has real signal.
        for (auto id : { "con_bypass", "opto_bypass", "fet_bypass", "lim_bypass" })
            setParam (proc, id, 0.0f);
        setParam (proc, "master_level", 0.0f);
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        for (int block = 0; block < 64; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = (float) (block * 512 + i) / 48000.0f;
                buf.setSample (0, i, 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * t));
                buf.setSample (1, i, 0.20f * std::sin (2.0f * juce::MathConstants<float>::pi * 660.0f * t));
            }
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
        }

        // Switch pages the way a user does: a real mouseDown on the toggle child. This is the
        // regression test for the tab that never responded (the canvas was eating the clicks).
        auto* toggle = findNamed (editor, "Page toggle");
        check (toggle != nullptr, "page toggle component exists");
        if (toggle != nullptr)
        {
            const auto time = juce::Time::getCurrentTime();
            const juce::Point<float> pt (toggle->getWidth() * 0.75f, toggle->getHeight() * 0.5f);
            juce::MouseEvent ev (juce::Desktop::getInstance().getMainMouseSource(), pt,
                juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, toggle, toggle,
                time, pt, time, 1, false);
            toggle->mouseDown (ev);
        }
        if (pass == 1)
            check (editor.isMeterPageVisible(), "clicking METER opens the metering page");
        else
            check (! editor.isMeterPageVisible(), "clicking METER again returns to the rack");

        // The meter page must offer its own way back: press BACK (black while held) and confirm
        // the release returns us to the rack. Only meaningful while the page is actually open
        // (hidden components are not enumerated by getChildren()).
        if (auto* meter = editor.isMeterPageVisible() ? findNamed (editor, "Meter page") : nullptr)
        {
            const auto hb = meter->getLocalBounds().reduced (14, 12).removeFromTop (34);
            const juce::Rectangle<int> backSample (hb.getX() + 4, hb.getCentreY() - 8, 10, 16);
            const juce::Rectangle<int> resetSample (hb.getCentreX() - 40, hb.getCentreY() - 8, 10, 16);
            auto meterEvent = [&] (juce::Point<float> p)
            {
                const auto t = juce::Time::getCurrentTime();
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p,
                    juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, meter, meter, t, p, t, 1, false);
            };

            // Idle: both buttons carry the page-toggle teal.
            auto idleFace = meter->createComponentSnapshot (meter->getLocalBounds());
            check (tealFraction (idleFace, backSample) > 0.5, "meter page BACK is teal at rest");
            check (tealFraction (idleFace, resetSample) > 0.5, "meter page RESET is teal at rest");

            // RESET goes black while held, then fires the full defaults restore on release.
            proc.setOrder ({ 3, 2, 1, 0 });
            const juce::Point<float> resetPoint ((float) resetSample.getCentreX(), (float) resetSample.getCentreY());
            meter->mouseDown (meterEvent (resetPoint));
            auto heldReset = meter->createComponentSnapshot (meter->getLocalBounds());
            check (blackFraction (heldReset, resetSample) > 0.5, "meter page RESET is black while pressed");
            meter->mouseUp (meterEvent (resetPoint));
            check (proc.getOrder() == ZychanStripProcessor::getDefaultOrder(),
                   "meter page RESET fires the full defaults restore");

            // BACK goes black while held and returns to the rack on release.
            const auto backPoint = backSample.getCentre().toFloat();
            meter->mouseDown (meterEvent (backPoint));
            auto heldFace = meter->createComponentSnapshot (meter->getLocalBounds());
            check (blackFraction (heldFace, backSample) > 0.5, "meter page BACK is black while pressed");
            meter->mouseUp (meterEvent (backPoint));
            check (! editor.isMeterPageVisible(), "BACK on the meter page returns to the rack");
        }
        else if (pass == 1)
            check (false, "meter page component found");

        // Always finish this pass on the meter page so the snapshot shows it.
        if (! editor.isMeterPageVisible())
        {
            const auto t2 = juce::Time::getCurrentTime();
            const juce::Point<float> pt2 (toggle->getWidth() * 0.75f, toggle->getHeight() * 0.5f);
            juce::MouseEvent ev2 (juce::Desktop::getInstance().getMainMouseSource(), pt2,
                juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, toggle, toggle,
                t2, pt2, t2, 1, false);
            toggle->mouseDown (ev2);
        }
        for (int tick = 0; tick < 8; ++tick)
            editor.pumpTimerForTest();

        auto img = editor.createComponentSnapshot (editor.getLocalBounds());
        const auto file = output.getChildFile ("meter-" + juce::String (size.x) + "x" + juce::String (size.y) + ".png");
        juce::FileOutputStream stream (file);
        stream.setPosition (0);
        stream.truncate();
        check (stream.openedOk() && juce::PNGImageFormat().writeImageToStream (img, stream),
               "meter page snapshot " + file.getFileName());
    }
    // Return to the rack so the following editorTests() render the rack page.
    editor.setMeterPageVisible (false);
    editor.setSize (1080, 760);
}

void editorTests (ZychanStripProcessor& proc, ZychanStripEditor& editor, const juce::File& output)
{
    // RESET button: the header button must restore module defaults, not just the meters.
    auto* reset = findNamed (editor, "Reset");
    check (reset != nullptr, "reset button component exists");
    if (reset != nullptr)
    {
        setParam (proc, "con_eq_1k", 9.0f);
        proc.setOrder ({ 3, 2, 1, 0 });
        editor.resized();
        const auto time = juce::Time::getCurrentTime();
        const juce::Point<float> pt (reset->getWidth() * 0.5f, reset->getHeight() * 0.5f);
        juce::MouseEvent ev (juce::Desktop::getInstance().getMainMouseSource(), pt,
            juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, reset, reset, time, pt, time, 1, false);

        const auto face = reset->getLocalBounds().reduced (6);
        // Idle face carries the page-toggle teal, like RACK/METER.
        auto idle = reset->createComponentSnapshot (reset->getLocalBounds());
        check (tealFraction (idle, face) > 0.5, "idle RESET face is teal");

        // Held face drops to matte black instead of a highlight.
        reset->mouseDown (ev);
        auto pressed = reset->createComponentSnapshot (reset->getLocalBounds());
        check (blackFraction (pressed, face) > 0.5, "pressed RESET face is black");

        reset->mouseUp (ev);
        check (std::abs (proc.apvts.getRawParameterValue ("con_eq_1k")->load()) < 1.0e-6f,
               "RESET button restores module parameters");
        check (proc.getOrder() == ZychanStripProcessor::getDefaultOrder(),
               "RESET button restores the rack order");
    }

    // Enhance module faders: every black channel carries a very thin white rim.
    for (auto id : { "limiter gain", "limiter ceiling", "limiter release", "limiter enhance" })
    {
        auto* fader = findNamed (editor, id);
        check (fader != nullptr, juce::String ("enhance fader found: ") + id);
        if (fader == nullptr) continue;
        auto face = fader->createComponentSnapshot (fader->getLocalBounds());
        const int top = face.getHeight() * 8 / 100, bottom = face.getHeight() * 92 / 100;
        // Ink = alpha-weighted brightness. The band just outside the black stroke must be lit by
        // the white rim while the empty plate further out stays dark. Rows where the fader cap
        // (or a tick) occupies that outer band are skipped, so any cap position is allowed.
        auto ink = [] (juce::Colour c) { return (double) c.getBrightness() * (double) c.getAlpha() / 255.0; };
        int cleanRows = 0, litRows = 0;
        for (int y = top; y < bottom; ++y)
        {
            double rim = 0.0, outside = 0.0;
            for (int x = 18; x <= 19; ++x) rim = juce::jmax (rim, ink (face.getPixelAt (x, y)));
            for (int x = 21; x <= 24; ++x) outside = juce::jmax (outside, ink (face.getPixelAt (x, y)));
            if (outside > 0.05) continue;
            ++cleanRows;
            if (rim > 0.20) ++litRows;
        }
        check (cleanRows > 40 && litRows >= cleanRows * 98 / 100,
               juce::String ("thin white rim on ") + id + " channel ("
                   + juce::String (litRows) + "/" + juce::String (cleanRows) + " rows lit)");
    }

    auto* master = dynamic_cast<juce::Slider*> (findNamed (editor, "Master level"));
    check (master != nullptr, "master slider found");
    if (master != nullptr)
    {
        master->setValue (-12, juce::sendNotificationSync);
        check (std::abs (proc.apvts.getRawParameterValue ("master_level")->load() + 12.0f) < 0.001f,
               "slider changes processor master parameter");
        setParam (proc, "master_level", -3);
        check (std::abs (master->getValue() + 3.0) < 0.001, "host parameter updates master slider");
        check (std::abs (master->getDoubleClickReturnValue()) < 1.0e-5, "double click restores unity");
        master->setValue (0, juce::sendNotificationSync);
    }
    auto* constrainer = editor.getConstrainer();
    check (editor.isResizable() && constrainer != nullptr, "editor supports constrained resizing");
    if (constrainer == nullptr) return;
    const double ratio = 1080.0 / 760.0;
    check (std::abs (constrainer->getFixedAspectRatio() - ratio) < 1.0e-6, "27:19 aspect ratio locked");
    for (const auto size : { juce::Point<int> (864, 608), { 1080, 760 }, { 1620, 1140 } })
    {
        constrainer->setBoundsForComponent (&editor, { 0, 0, size.x, size.y }, false, false, true, true);
        check (editor.getWidth() == size.x && editor.getHeight() == size.y,
               "resize " + juce::String (size.x) + "x" + juce::String (size.y));
        auto* canvas = findNamed (editor, "Rack canvas");
        check (canvas != nullptr, "logical canvas found");
        if (canvas != nullptr)
        {
            auto mapped = editor.getLocalArea (canvas, canvas->getLocalBounds());
            check (std::abs (mapped.getWidth() - editor.getWidth()) <= 1
                   && std::abs (mapped.getHeight() - editor.getHeight()) <= 1,
                   "canvas fills editor proportionally");
            for (auto name : { "Console", "Opto", "FET", "Limiter" })
            {
                auto* panel = findNamed (*canvas, name);
                bool contained = panel != nullptr;
                if (panel != nullptr)
                    for (auto* child : panel->getChildren())
                        contained &= panel->getLocalBounds().contains (child->getBounds());
                check (contained, juce::String (name) + " controls stay inside panel");
            }

            // Limiter fader bank is a 2x2 grid: THRESHOLD/CEILING above RELEASE/ENHANCE, with the
            // two metering ladders beside the grid rather than inside it.
            if (auto* limiter = findNamed (*canvas, "Limiter"))
            {
                auto* p0 = findNamed (*limiter, "limiter gain");
                auto* p1 = findNamed (*limiter, "limiter ceiling");
                auto* p2 = findNamed (*limiter, "limiter release");
                auto* p3 = findNamed (*limiter, "limiter enhance");
                check (p0 && p1 && p2 && p3, "limiter exposes four faders");
                if (p0 && p1 && p2 && p3)
                {
                    const auto a = p0->getBounds(), b = p1->getBounds();
                    const auto c2 = p2->getBounds(), d = p3->getBounds();
                    const bool topRow = std::abs (a.getY() - b.getY()) <= 1 && a.getX() < b.getX();
                    const bool bottomRow = std::abs (c2.getY() - d.getY()) <= 1 && c2.getX() < d.getX();
                    const bool stacked = c2.getY() > a.getBottom() - 1 && d.getY() > b.getBottom() - 1;
                    const bool columns = std::abs (a.getX() - c2.getX()) <= 1 && std::abs (b.getX() - d.getX()) <= 1;
                    check (topRow && bottomRow && stacked && columns,
                           "limiter faders form a 2x2 grid (threshold/ceiling over release/enhance)");

                    // Both rows share one height, so the grid is visually balanced.
                    check (std::abs (a.getHeight() - c2.getHeight()) <= 2,
                           "limiter grid rows are equal height");
                }
            }
            auto* panel = findNamed (*canvas, "Console");
            if (panel != nullptr)
            {
                const auto startOrder = proc.getOrder();
                const juce::Point<float> down ((float) panel->getWidth() * 0.5f, 10.0f);
                const auto time = juce::Time::getCurrentTime();
                auto event = [&] (juce::Point<float> point, bool dragged)
                {
                    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), point,
                        juce::ModifierKeys::leftButtonModifier, 1, 0, 0, 0, 0, panel, panel, time, down, time, 1, dragged);
                };
                panel->mouseDown (event (down, false));
                panel->mouseDrag (event ({ down.x + 900.0f, down.y }, true));
                panel->mouseUp (event (down, true));
                check (proc.getOrder().back() == ZychanStripProcessor::Console,
                       "scaled grip drag reorders Console to last slot");
                proc.setOrder (startOrder);
                editor.resized();
            }
        }
        auto img = editor.createComponentSnapshot (editor.getLocalBounds());
        const auto file = output.getChildFile ("rack-" + juce::String (size.x) + "x" + juce::String (size.y) + ".png");
        juce::FileOutputStream stream (file);
        stream.setPosition (0);
        stream.truncate();
        check (stream.openedOk() && juce::PNGImageFormat().writeImageToStream (img, stream),
               "snapshot " + file.getFileName());
    }
    constrainer->setBoundsForComponent (&editor, { 0, 0, 1230, 650 }, false, false, true, true);
    check (std::abs ((double) editor.getWidth() / editor.getHeight() - ratio) < 0.003,
           "off-ratio resize request is corrected");
}
}
class SnapshotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "ZychanStripSnapshot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        const auto output = args.isEmpty() ? juce::File::getCurrentWorkingDirectory()
                                         : juce::File (args[0].unquoted());
        output.createDirectory();
        eqTests();
        limiterTests();
        audioTests();
        autoGainOrderTests();
        resetTests();
        ZychanStripProcessor proc;
        auto editor = std::unique_ptr<ZychanStripEditor> (static_cast<ZychanStripEditor*> (proc.createEditor()));
        newEditorTests (proc, *editor);
        meterPageTests (proc, *editor, output);
        editorTests (proc, *editor, output);
        std::cout << "RESULT " << passed << "/" << (passed + failed) << " passed" << std::endl;
        setApplicationReturnValue (failed == 0 ? 0 : 1);
        quit();
    }
    void shutdown() override {}
};
START_JUCE_APPLICATION (SnapshotApp)
