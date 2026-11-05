#pragma once

void enhanceContractTests()
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    // Exercise the actual processor (not only LimiterModule). This detects the
    // former compensated-output feedback, which left exactly half the gain in dB.
    for (double sr : { 48000., 96000. })
        for (int blockSize : { 32, 512 })
            for (int mode = 0; mode < 3; ++mode)
            {
                ZychanStripProcessor p;
                bypassAll (p);
                setParam (p, "lim_bypass", 0);
                setParam (p, "lim_auto_gain", 1);
                setParam (p, "colour_mode", (float) mode);
                p.setPlayConfigDetails (2, 2, sr, blockSize);
                p.prepareToPlay (sr, blockSize);
                const int latency = p.getLatencySamples();
                int64_t time = 0;
                auto measure = [&] ()
                {
                    double input = 0, output = 0;
                    const int total = (int) (2 * sr);
                    juce::MidiBuffer midi;
                    for (int offset = 0; offset < total; offset += blockSize)
                    {
                        const int n = std::min (blockSize, total - offset);
                        juce::AudioBuffer<float> b (2, n);
                        for (int i = 0; i < n; ++i)
                        {
                            const float value = 0.03f * (float) std::sin (twoPi * 1000 * (time + i) / sr);
                            b.setSample (0, i, value);
                            b.setSample (1, i, value * 0.5f);
                            if (offset + i >= total - (int) (sr / 4)) input += (double) value * value;
                        }
                        p.processBlock (b, midi);
                        for (int i = 0; i < n; ++i)
                            if (offset + i >= total - (int) (sr / 4))
                                output += (double) b.getSample (0, i) * b.getSample (0, i);
                        time += n;
                    }
                    return 10 * std::log10 (output / std::max (input, 1.e-20));
                };
                double worstError = 0;
                for (float gain : { 0.f, 6.f, 12.f, 18.f, 6.f, 0.f })
                {
                    setParam (p, "lim_gain", gain);
                    worstError = std::max (worstError, std::abs (measure()));
                }
                setParam (p, "lim_auto_gain", 0);
                setParam (p, "lim_gain", 6);
                const double manualError = std::abs (measure() - 6.0);
                check (worstError < 0.12 && manualError < 0.05 && p.getLatencySamples() == latency,
                    "processor Auto Gain follows threshold down/up mode=" + juce::String (mode)
                    + " rate=" + juce::String (sr) + " block=" + juce::String (blockSize)
                    + " matchErrorDb=" + juce::String (worstError, 5));
            }

    // A residual enhancer must actually add nonlinear content, while keeping
    // the original small-signal response (checked by the frequency sweep).
    for (double sr : { 48000., 96000. })
    {
        ZychanStripProcessor controls;
        setParam (controls, "colour_mode", 1); setParam (controls, "lim_auto_gain", 0);
        setParam (controls, "lim_ceiling", 0);
        rack::LimiterModule limiter (controls.apvts, "lim_"); limiter.prepare (sr, 128);
        std::array<double, 2> harmonics {};
        double fundamental = 0;
        for (int setting = 0; setting < 2; ++setting)
        {
            setParam (controls, "lim_enhance", setting == 0 ? 0.f : 100.f);
            std::array<std::complex<double>, 5> partials {};
            const int measured = (int) (sr / 5), total = (int) sr;
            for (int sample = 0; sample < total; sample += 128)
            {
                const int n = std::min (128, total - sample);
                juce::AudioBuffer<float> b (2, n);
                for (int i = 0; i < n; ++i)
                    for (int c = 0; c < 2; ++c)
                        b.setSample (c, i, 0.5f * (float) std::sin (twoPi * 1000 * (sample + i) / sr));
                limiter.run (b);
                for (int i = 0; i < n; ++i)
                    if (sample + i >= total - measured)
                        for (int h = 1; h <= 5; ++h)
                        {
                            const double phase = twoPi * 1000 * h * (sample + i) / sr;
                            partials[(size_t) h - 1] += (double) b.getSample (0, i)
                                * std::complex<double> (std::sin (phase), std::cos (phase));
                        }
            }
            for (int h = 1; h < 5; ++h) harmonics[(size_t) setting] += std::norm (partials[(size_t) h]) / (measured * measured);
            if (setting == 1) fundamental = std::norm (partials[0]) / (measured * measured);
        }
        check (harmonics[1] > harmonics[0] + 1.e-12 && harmonics[1] / fundamental < 0.01,
            "NAM residual adds bounded harmonics rate=" + juce::String (sr)
            + " harmonicRatio=" + juce::String (std::sqrt (harmonics[1] / fundamental), 6));
    }

    // Headroom is a physical constraint: Auto Gain must report its limit rather
    // than exceed the ceiling or claim a complete match for an impossible target.
    for (int channels : { 1, 2 })
    {
        ZychanStripProcessor p;
        bypassAll (p);
        setParam (p, "lim_bypass", 0);
        setParam (p, "colour_mode", 2);
        setParam (p, "lim_auto_gain", 1);
        setParam (p, "lim_gain", 18);
        setParam (p, "lim_ceiling", -12);
        p.setPlayConfigDetails (channels, channels, 48000, 128);
        p.prepareToPlay (48000, 128);
        juce::MidiBuffer midi;
        float peak = 0;
        bool finite = true;
        for (int base = 0; base < 144000; base += 128)
        {
            juce::AudioBuffer<float> b (channels, 128);
            for (int i = 0; i < 128; ++i)
                for (int c = 0; c < channels; ++c)
                    b.setSample (c, i, 0.8f * (float) std::sin (twoPi * 1000 * (base + i) / 48000));
            p.processBlock (b, midi);
            for (int i = 0; i < 128; ++i)
                for (int c = 0; c < channels; ++c)
                {
                    finite &= std::isfinite (b.getSample (c, i));
                    peak = std::max (peak, std::abs (b.getSample (c, i)));
                }
        }
        check (finite && peak <= 0.251189f && p.isAutoGainCeilingLimited(),
            "Auto Gain reports unavailable ceiling headroom channels=" + juce::String (channels));
    }

    // Irregular and empty host blocks must not reuse the old block's mismatched
    // energy references, and must not change latency.
    for (int mode = 0; mode < 3; ++mode)
    {
        ZychanStripProcessor p;
        bypassAll (p); setParam (p, "lim_bypass", 0);
        setParam (p, "lim_auto_gain", 1); setParam (p, "lim_gain", 12);
        setParam (p, "colour_mode", (float) mode);
        p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        const int latency = p.getLatencySamples();
        double input = 0, output = 0;
        int time = 0, block = 0;
        const std::array<int, 7> sizes { 32, 128, 7, 512, 0, 96, 31 };
        juce::MidiBuffer midi;
        bool finite = true;
        while (time < 144000)
        {
            const int n = sizes[(size_t) block++ % sizes.size()];
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                const float value = 0.03f * (float) std::sin (twoPi * 1000 * (time + i) / 48000);
                b.setSample (0, i, value); b.setSample (1, i, value);
                if (time + i >= 120000) input += (double) value * value;
            }
            p.processBlock (b, midi);
            for (int i = 0; i < n; ++i)
            {
                const float value = b.getSample (0, i);
                finite &= std::isfinite (value);
                if (time + i >= 120000) output += (double) value * value;
            }
            time += n;
        }
        const double error = std::abs (10 * std::log10 (output / input));
        check (finite && error < 0.12 && p.getLatencySamples() == latency,
            "Auto Gain variable/empty blocks mode=" + juce::String (mode) + " matchErrorDb=" + juce::String (error, 5));
    }
}
