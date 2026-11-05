#pragma once

// End-to-end tests of the production limiter law, rather than a mocked NAM blend.
void namEnhanceTests()
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    for (double sr : { 44100., 48000., 96000. })
        for (float level : { 0.01f, 0.1f, 0.5f })
            for (double hz : { 50., 100., 1000., 8000., 16000. })
            {
                ZychanStripProcessor controls;
                setParam (controls, "colour_mode", 1);
                setParam (controls, "lim_auto_gain", 0);
                setParam (controls, "lim_ceiling", 0);
                rack::LimiterModule limiter (controls.apvts, "lim_");
                limiter.prepare (sr, 128);
                const int latency = limiter.getLatencySamples();
                const int measured = (int) (sr / 10); // whole cycles for every test frequency
                const int samples = (((int) (sr / 5) + measured + 127) / 128) * 128;
                int64_t time = 0;
                std::array<double, 11> gains {};
                bool finite = true, neutral = true;
                for (int point = 0; point <= 10; ++point)
                {
                    setParam (controls, "lim_enhance", (float) point * 10);
                    std::complex<double> fundamental {};
                    for (int offset = 0; offset < samples; offset += 128)
                    {
                        juce::AudioBuffer<float> b (2, 128);
                        for (int i = 0; i < 128; ++i)
                            for (int c = 0; c < 2; ++c)
                                b.setSample (c, i, level * (float) std::sin (twoPi * hz * (time + i) / sr));
                        limiter.run (b);
                        for (int i = 0; i < 128; ++i)
                        {
                            const float value = b.getSample (0, i);
                            finite &= std::isfinite (value) && value == b.getSample (1, i);
                            if (offset + i >= samples - measured)
                            {
                                const double phase = twoPi * hz * (time + i - latency) / sr;
                                fundamental += (double) value * std::complex<double> (std::sin (phase), std::cos (phase));
                                if (point == 0)
                                    neutral &= std::abs (value - level * (float) std::sin (phase)) < 1.e-6f;
                            }
                        }
                        time += 128;
                    }
                    gains[(size_t) point] = 20 * std::log10 (2 * std::abs (fundamental) / (measured * level));
                }
                const double direction = gains.back() >= gains.front() ? 1. : -1.;
                bool progressive = true;
                double largestStep = 0, largestDeviation = 0;
                for (int point = 1; point <= 10; ++point)
                {
                    const double step = gains[(size_t) point] - gains[(size_t) point - 1];
                    progressive &= direction * step >= -0.02; // tolerate measurement/model residuals
                    largestStep = std::max (largestStep, std::abs (step));
                    largestDeviation = std::max (largestDeviation, std::abs (gains[(size_t) point]));
                }
                check (finite && neutral && progressive && largestStep < 0.65 && largestDeviation < 4.5
                           && limiter.getLatencySamples() == latency,
                    "NAM Enhance tone progression rate=" + juce::String (sr) + " Hz=" + juce::String (hz)
                    + " peak=" + juce::String (level) + " maxStepDb=" + juce::String (largestStep, 4)
                    + " endDb=" + juce::String (gains.back(), 4));
            }

    // Move Enhance with EVERY module active and colouring the signal. Unlike the
    // old isolated-limiter order test, this exercises genuine downstream interaction.
    for (double sr : { 48000., 96000. })
        for (bool autoGain : { false, true })
        {
            std::array<int, 4> order { 0, 1, 2, 3 };
            bool valid = true;
            float maxJump = 0;
            double leastResponse = 1.e9;
            int orders = 0;
            do
            {
                ZychanStripProcessor p, unchanged;
                for (auto* instance : { &p, &unchanged })
                {
                    setParam (*instance, "colour_mode", 1);
                    setParam (*instance, "lim_auto_gain", autoGain ? 1.f : 0.f);
                    setParam (*instance, "con_eq_1k", 3);
                    setParam (*instance, "fet_input", 12);
                    setParam (*instance, "fet_output", -6);
                    setParam (*instance, "opto_peak", 30);
                    setParam (*instance, "opto_gain", 40);
                    instance->setOrder (order);
                    instance->setPlayConfigDetails (2, 2, sr, 128);
                    instance->prepareToPlay (sr, 128);
                }
                const int latency = p.getLatencySamples();
                double difference = 0, reference = 0;
                float previous = 0;
                juce::MidiBuffer midi;
                for (int segment = 0; segment < 3; ++segment)
                {
                    setParam (p, "lim_enhance", segment == 1 ? 100.f : 0.f);
                    for (int sample = 0; sample < 16384; sample += 128)
                    {
                        juce::AudioBuffer<float> b (2, 128);
                        for (int i = 0; i < 128; ++i)
                        {
                            const double t = (segment * 16384 + sample + i) / sr;
                            const float value = (float) (0.015 * (std::sin (twoPi * 375 * t)
                                + 0.5 * std::sin (twoPi * 1500 * t) + 0.25 * std::sin (twoPi * 6000 * t)));
                            b.setSample (0, i, value);
                            b.setSample (1, i, value * 0.8f);
                        }
                        juce::AudioBuffer<float> control;
                        control.makeCopyOf (b);
                        p.processBlock (b, midi);
                        unchanged.processBlock (control, midi);
                        for (int i = 0; i < 128; ++i)
                        {
                            const float value = b.getSample (0, i);
                            for (int c = 0; c < 2; ++c)
                                valid &= std::isfinite (b.getSample (c, i)) && std::abs (b.getSample (c, i)) < 4.f;
                            if (segment > 0) maxJump = std::max (maxJump, std::abs (value - previous));
                            previous = value;
                            if (sample + i >= 15360)
                            {
                                if (segment == 1)
                                {
                                    const double unmodified = control.getSample (0, i);
                                    difference += std::pow ((double) value - unmodified, 2);
                                    reference += unmodified * unmodified;
                                }
                            }
                        }
                    }
                }
                const double response = std::sqrt (difference / std::max (reference, 1.e-20));
                leastResponse = std::min (leastResponse, response);
                valid &= response > 1.e-4 && p.getLatencySamples() == latency && p.getOrder() == order;
                ++orders;
            } while (std::next_permutation (order.begin(), order.end()));
            check (valid && orders == 24 && maxJump < 0.1f,
                "NAM moving Enhance responds through all 24 active orders rate=" + juce::String (sr)
                + " auto=" + juce::String ((int) autoGain) + " minRelativeResponse=" + juce::String (leastResponse, 5)
                + " maxJump=" + juce::String (maxJump, 6));
        }
}
