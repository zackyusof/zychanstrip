// Independent response and level-matching checks for the EQ560 and limiter.
// Reference: pinned chkhld/jsfx Equalizer/eq_560.jsfx (see THIRD_PARTY_NOTICES.md).

const std::array<const char*, 10> eqIds { "con_eq_16k", "con_eq_8k", "con_eq_4k", "con_eq_2k", "con_eq_1k",
                                         "con_eq_500", "con_eq_250", "con_eq_125", "con_eq_63", "con_eq_31" };
const std::array<double, 10> eqFrequencies { 16000, 8000, 4000, 2000, 1000, 500, 250, 125, 63, 31 };
using Complex = std::complex<double>;

// Analytic z-domain response, independently derived from the upstream recurrence.
// The second high-rate image is necessary when a 2x signal is decimated again.
Complex referenceSvf (double sr, double frequency, double centre, double db)
{
    const auto a = std::pow (10.0, db * 0.025);
    const auto q = 0.126984 + (5.763566 - 0.126984) * std::pow (std::abs (db) / 12.0, 2.0);
    const auto k = 1.0 / (q * a);
    const Complex s (0, std::tan (juce::MathConstants<double>::pi * frequency / sr)
                           / std::tan (juce::MathConstants<double>::pi * centre / sr));
    return (s * s + k * a * a * s + 1.0) / (s * s + k * s + 1.0);
}
Complex referenceResampler (double sr, double frequency)
{
    // Analytic Fourier sum of a 63-point Blackman-windowed halfband sinc.
    // This replaces the historical IIR oversampler reference; the 2x SVF
    // recurrence and proportional-Q law are unchanged.
    Complex response {};
    double dc = 0;
    for (int n = 0; n < 63; ++n)
    {
        const double t = n - 31;
        const double window = 0.42 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * n / 62)
                                  + 0.08 * std::cos (4 * juce::MathConstants<double>::pi * n / 62);
        const double sinc = t == 0 ? 0.5 : std::sin (0.5 * juce::MathConstants<double>::pi * t)
                                                  / (juce::MathConstants<double>::pi * t);
        const double coefficient = sinc * window;
        dc += coefficient;
        response += coefficient * std::exp (Complex (0, -juce::MathConstants<double>::pi * frequency * n / sr));
    }
    return response / dc;
}
double referenceEqMagnitude (double sr, double frequency, const std::array<float, 10>& gains)
{
    Complex result (0, 0);
    for (double imageFrequency : { frequency, frequency + sr })
    {
        auto response = referenceResampler (sr, imageFrequency);
        response *= response;
        for (size_t band = 0; band < gains.size(); ++band)
            if (gains[band] != 0.0f)
                response *= referenceSvf (2.0 * sr, imageFrequency, eqFrequencies[band], gains[band]);
        result += response;
    }
    return std::abs (result);
}

// Run the production DSP, using sine/cosine projection rather than assuming integer cycle windows.
double measureEq (double sr, double frequency, const std::array<float, 10>& gains)
{
    rack::Eq560 eq;
    eq.setBandGains (gains);
    eq.prepare (sr);
    double sinProjection = 0, cosProjection = 0;
    const int settle = (int) sr;
    const int count = (int) sr;
    for (int i = 0; i < settle + count; ++i)
    {
        const double phase = 2.0 * juce::MathConstants<double>::pi * frequency * i / sr;
        eq.beginSample();
        const auto y = eq.processSample (0, (float) (0.01 * std::sin (phase)));
        if (i >= settle)
        {
            sinProjection += y * std::sin (phase);
            cosProjection += y * std::cos (phase);
        }
    }
    return 2.0 * std::hypot (sinProjection, cosProjection) / (0.01 * count);
}
void eqTests()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        for (size_t band = 0; band < eqIds.size(); ++band)
        {
            // Full ten-band sweep at48k; low/high edge bands exercise the other rates.
            if (sr != 48000.0 && band != 0 && band != 9) continue;
            const auto neutral = measureEq (sr, eqFrequencies[band], {});
            const auto referenceNeutral = referenceEqMagnitude (sr, eqFrequencies[band], {});
            check (std::abs (20.0 * std::log10 (neutral / referenceNeutral)) < 0.02,
                   "EQ560 neutral resampler reference " + juce::String (eqFrequencies[band]) + "Hz @" + juce::String (sr));
            for (float db : { -12.0f, 12.0f })
            {
                std::array<float, 10> gains {};
                gains[band] = db;
                const auto measured = measureEq (sr, eqFrequencies[band], gains);
                const auto expected = referenceEqMagnitude (sr, eqFrequencies[band], gains);
                const auto error = 20.0 * std::log10 (measured / expected);
                check (std::isfinite (error) && std::abs (error) < 0.025,
                       "EQ560 2x SVF / linear-phase response " + juce::String (eqFrequencies[band]) + "Hz "
                           + juce::String (db) + "dB @" + juce::String (sr) + " error=" + juce::String (error, 4) + "dB");
            }
        }
    }
    std::array<float, 10> low {}, high {};
    low[4] = 6; high[4] = 12;
    const auto at6 = 20.0 * std::log10 (measureEq (48000, 1000, low));
    const auto at12 = 20.0 * std::log10 (measureEq (48000, 1000, high));
    const auto side6 = 20.0 * std::log10 (measureEq (48000, 700, low));
    const auto side12 = 20.0 * std::log10 (measureEq (48000, 700, high));
    check (side12 / at12 < side6 / at6, "EQ560 proportional-Q response narrows as boost increases");

    for (double sr : { 32000.0, 44100.0, 96000.0, 192000.0 })
    {
        rack::Eq560 eq;
        eq.prepare (sr);
        bool stable = true, linked = true;
        for (int i = 0; i < (int) sr; ++i)
        {
            if (i % 97 == 0)
            {
                std::array<float, 10> gains {};
                for (size_t band = 0; band < gains.size(); ++band)
                    gains[band] = ((i / 97 + (int) band) % 2 == 0) ? 12.0f : -12.0f;
                eq.setBandGains (gains);
            }
            const auto input = (float) (0.001 * (std::sin (0.13 * i) + std::sin (0.379 * i)));
            eq.beginSample();
            const auto l = eq.processSample (0, input);
            const auto r = eq.processSample (1, input * 0.5f);
            stable &= std::isfinite (l) && std::isfinite (r) && std::abs (l) < 100.0f;
            linked &= std::abs (r - l * 0.5f) < 1.0e-6f;
        }
        check (stable, "EQ560 extreme automated gains stable @" + juce::String (sr));
        check (linked, "EQ560 channels share coefficients with independent matched state @" + juce::String (sr));
    }
}

struct LimiterMeasurement { double inputRms = 0, outputRms = 0, peak = 0, ratioError = 0, transientRatioError = 0; bool finite = true; };
LimiterMeasurement measureLimiter (rack::LimiterModule& limiter, double sr, int channels,
                                    double amplitude, double seconds = 6.0)
{
    juce::AudioBuffer<float> buffer (channels, 257);
    LimiterMeasurement m;
    double inputEnergy = 0, outputEnergy = 0;
    int measured = 0;
    const int count = (int) (sr * seconds);
    const int startMeasure = (int) (sr * (seconds - 1.0));
    for (int base = 0; base < count; base += buffer.getNumSamples())
    {
        const int n = std::min (buffer.getNumSamples(), count - base);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto t = (base + i) / sr;
            const auto x = amplitude * (0.6 * std::sin (2.0 * juce::MathConstants<double>::pi * 211.0 * t)
                                     + 0.25 * std::sin (2.0 * juce::MathConstants<double>::pi * 997.0 * t)
                                     + 0.15 * std::sin (2.0 * juce::MathConstants<double>::pi * 3011.0 * t));
            for (int ch = 0; ch < channels; ++ch) buffer.setSample (ch, i, (float) (x * (ch == 0 ? 1.0 : 0.5)));
            if (base + i >= startMeasure && i < n)
                for (int ch = 0; ch < channels; ++ch)
                {
                    const auto channelInput = x * (ch == 0 ? 1.0 : 0.5);
                    inputEnergy += channelInput * channelInput;
                    ++measured;
                }
        }
        limiter.run (buffer);
        for (int i = 0; i < n; ++i)
            for (int ch = 0; ch < channels; ++ch)
            {
                const auto y = buffer.getSample (ch, i);
                m.finite &= std::isfinite (y);
                m.peak = std::max (m.peak, (double) std::abs (y));
                if (base + i >= startMeasure) outputEnergy += y * y;
                if (channels == 2)
                {
                    const auto error = std::abs ((double) buffer.getSample (1, i) - 0.5 * buffer.getSample (0, i));
                    m.transientRatioError = std::max (m.transientRatioError, error);
                    if (base + i >= startMeasure) m.ratioError = std::max (m.ratioError, error);
                }
            }
    }
    m.inputRms = std::sqrt (inputEnergy / measured);
    m.outputRms = std::sqrt (outputEnergy / measured);
    return m;
}
void limiterTests()
{
    for (int window : { 1, 2, 3, 67, 72, 144, 288 })
    {
        rack::LimiterModule::SlidingMinimum minimum;
        minimum.prepare (window);
        std::vector<float> reference ((size_t) window, 1.0f);
        bool exact = true;
        const int iterations = window * 12 + 37;
        for (int i = 0; i < iterations; ++i)
        {
            float value = 1.0f;
            switch (i % 5)
            {
                case 0: value = (float) (i % 17) / 16.0f; break;
                case 1: value = (float) (16 - (i % 17)) / 16.0f; break;
                case 2: value = 0.375f; break;
                case 3: value = (i % 3 == 0) ? 0.25f : 0.75f; break;
                default: value = 1.0f; break;
            }
            reference[(size_t) (i % window)] = value;
            float expected = 1.0f;
            for (float candidate : reference) expected = std::min (expected, candidate);
            exact &= minimum.push (value) == expected;
            if (i == iterations / 2)
            {
                minimum.reset();
                std::fill (reference.begin(), reference.end(), 1.0f);
            }
        }
        check (exact, "limiter sliding minimum matches initialized ring, reset/wrap window=" + juce::String (window));
    }
    ZychanStripProcessor proc;
    check (proc.apvts.getRawParameterValue ("lim_auto_gain") != nullptr, "Auto Gain parameter exists");
    if (proc.apvts.getRawParameterValue ("lim_auto_gain") == nullptr) return;
    check (proc.apvts.getRawParameterValue ("lim_auto_gain")->load() > 0.5f, "new instances enable Enhance Auto Gain");
    for (int channels : { 1, 2 })
    {
        rack::LimiterModule limiter (proc.apvts, "lim_");
        limiter.prepare (48000, 257);
        setParam (proc, "lim_auto_gain", 1);
        setParam (proc, "lim_bypass", 0);
        const std::array<std::array<float, 4>, 5> settings {{
            {{ 0, 0, -0.3f, 100 }}, {{ 6, 0, -0.3f, 10 }}, {{ 0, 100, -0.3f, 1000 }},
            {{ 18, 100, -6, 100 }}, {{ 0, 75, -12, 10 }} }};
        for (const auto& s : settings)
        {
            setParam (proc, "lim_gain", s[0]); setParam (proc, "lim_enhance", s[1]);
            setParam (proc, "lim_ceiling", s[2]); setParam (proc, "lim_release", s[3]);
            const auto m = measureLimiter (limiter, 48000, channels, 0.08);
            const auto error = 20.0 * std::log10 (m.outputRms / m.inputRms);
            check (m.finite && std::abs (error) < 0.6,
                   "Enhance Auto Gain matches module input, gain=" + juce::String (s[0]) + " enhance="
                       + juce::String (s[1]) + " ceiling=" + juce::String (s[2]) + " " + juce::String (channels)
                       + "ch RMS error=" + juce::String (error, 3) + "dB");
            check (m.peak <= juce::Decibels::decibelsToGain ((double) s[2]) + 1.0e-6,
                   "Auto Gain respects limiter ceiling");
        }
        // Nonlinear Enhance can alter ratios; with Enhance off the same linked gain must preserve them.
        setParam (proc, "lim_enhance", 0); setParam (proc, "lim_gain", 12);
        const auto linked = measureLimiter (limiter, 48000, channels, 0.08);
        check (linked.finite && linked.ratioError < 2.0e-6,
               "Auto Gain correction is channel-linked, settled error=" + juce::String (linked.ratioError, 9)
                   + " transition error=" + juce::String (linked.transientRatioError, 6));
        const auto quiet = measureLimiter (limiter, 48000, channels, 1.0e-8, 3);
        check (quiet.finite && quiet.outputRms < 2.0e-6, "near-silence cannot cause Auto Gain runaway");
        const auto silent = measureLimiter (limiter, 48000, channels, 0, 3);
        check (silent.finite && silent.outputRms < 1.0e-10, "silence settles to exact silence");
        const auto recovered = measureLimiter (limiter, 48000, channels, 0.08, 6);
        check (recovered.finite && std::abs (20.0 * std::log10 (recovered.outputRms / recovered.inputRms)) < 0.6,
               "Auto Gain recovers after silence");
    }
    setParam (proc, "lim_bypass", 0); setParam (proc, "lim_auto_gain", 0);
    setParam (proc, "lim_enhance", 0); setParam (proc, "lim_gain", 6); setParam (proc, "lim_ceiling", -0.3f);
    rack::LimiterModule manual (proc.apvts, "lim_"); manual.prepare (48000, 257);
    const auto off = measureLimiter (manual, 48000, 2, 0.02, 3);
    check (std::abs (20.0 * std::log10 (off.outputRms / off.inputRms) - 6.0) < 0.05,
           "Auto Gain off preserves manual limiter gain");
    // Bypassed path must remain the original sample-exact lookahead delay.
    for (double sr : { 44100.0, 96000.0 })
    {
        rack::LimiterModule limiter (proc.apvts, "lim_");
        limiter.prepare (sr, 512);
        setParam (proc, "lim_bypass", 1);
        juce::AudioBuffer<float> impulse (2, 512); impulse.clear();
        impulse.setSample (0, 0, 0.5f); impulse.setSample (1, 0, -0.25f);
        limiter.run (impulse);
        bool exact = true;
        const int delay = limiter.getLatencySamples();
        for (int i = 0; i < impulse.getNumSamples(); ++i)
            exact &= impulse.getSample (0, i) == (i == delay ? 0.5f : 0.0f)
                  && impulse.getSample (1, i) == (i == delay ? -0.25f : 0.0f);
        check (exact, "Auto Gain leaves bypass delay sample-exact @" + juce::String (sr));
    }
    setParam (proc, "lim_bypass", 0);
    setParam (proc, "lim_auto_gain", 1);
    setParam (proc, "con_eq_500", 7.5f);
    auto state = proc.apvts.copyState();
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary (*state.createXml(), blob);
    ZychanStripProcessor restored;
    restored.setStateInformation (blob.getData(), (int) blob.getSize());
    check (restored.apvts.getRawParameterValue ("lim_auto_gain")->load() > 0.5f
           && std::abs (restored.apvts.getRawParameterValue ("con_eq_500")->load() - 7.5f) < 0.001f,
           "new band and Auto Gain settings survive session recall");
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).getProperty ("id").toString() == "lim_auto_gain") state.removeChild (i, nullptr);
    blob.reset(); juce::AudioProcessor::copyXmlToBinary (*state.createXml(), blob);
    restored.setStateInformation (blob.getData(), (int) blob.getSize());
    check (restored.apvts.getRawParameterValue ("lim_auto_gain")->load() < 0.5f,
           "legacy sessions restore Auto Gain off");
}
void newEditorTests (ZychanStripProcessor& proc, ZychanStripEditor& editor)
{
    for (auto id : eqIds)
    {
        auto* slider = dynamic_cast<juce::Slider*> (findNamed (editor, id));
        check (slider != nullptr, juce::String (id) + " EQ560 fader exists");
        if (slider == nullptr) continue;
        check (slider->getSliderStyle() == juce::Slider::LinearHorizontal
               && slider->getMinimum() == -12.0 && slider->getMaximum() == 12.0,
               juce::String (id) + " horizontal +/-12dB range");
        slider->setValue (5, juce::sendNotificationSync);
        check (std::abs (proc.apvts.getRawParameterValue (id)->load() - 5.0f) < 0.001f,
               juce::String (id) + " GUI-to-audio attachment");
        setParam (proc, id, -4);
        check (std::abs (slider->getValue() + 4) < 0.001, juce::String (id) + " host-to-GUI attachment");
        setParam (proc, id, 0);
    }
    auto* toggle = dynamic_cast<juce::ToggleButton*> (findNamed (editor, "Auto Gain"));
    check (toggle != nullptr, "Enhance Auto Gain toggle exists");
    if (toggle != nullptr)
    {
        toggle->setToggleState (false, juce::sendNotificationSync);
        check (proc.apvts.getRawParameterValue ("lim_auto_gain")->load() < 0.5f, "Auto Gain GUI-to-audio attachment");
        setParam (proc, "lim_auto_gain", 1);
        check (toggle->getToggleState(), "Auto Gain host-to-GUI attachment");
    }
    auto* unity = editor.getUnityButtonForTest();
    check (unity != nullptr && unity->getName() == "UNITY", "UNITY host-automatable button exists beside RESET");
    if (unity != nullptr)
    {
        auto mouse = [&] (bool down)
        {
            const auto t = juce::Time::getCurrentTime();
            const juce::Point<float> pt (unity->getWidth() * 0.5f, unity->getHeight() * 0.5f);
            const juce::MouseEvent ev (juce::Desktop::getInstance().getMainMouseSource(), pt,
                down ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys(), 1, 0, 0, 0, 0,
                unity, unity, t, pt, t, 1, false);
            if (down) unity->mouseDown (ev); else unity->mouseUp (ev);
        };
        mouse (true); mouse (false);
        check (proc.apvts.getRawParameterValue ("unity")->load() > 0.5f, "UNITY GUI-to-audio host parameter");
        setParam (proc, "unity", 0);
    }
}
