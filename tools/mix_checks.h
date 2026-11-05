#pragma once

void parallelMixTests()
{
    constexpr double pi = juce::MathConstants<double>::pi;
    // A changing mix must null against 100% wet for a bypassed rack, from the
    // first sample onward. This catches endpoint ring freezing, 0% PDC mismatch,
    // odd block sizes, and the new per-module bypass padding.
    for (double sr : { 44100., 48000., 88200., 96000., 176400., 192000. })
        for (int blockSize : { 31, 512 })
        {
            ZychanStripProcessor p, reference;
            for (auto* instance : { &p, &reference })
            {
                bypassAll (*instance); setParam (*instance, "colour_mode", 2);
                instance->setPlayConfigDetails (2, 2, sr, blockSize);
                instance->prepareToPlay (sr, blockSize);
            }
            const int latency = p.getLatencySamples();
            juce::MidiBuffer midi;
            uint32_t random = 91;
            float error = 0;
            for (float target : { 100.f, 50.f, 100.f, 25.f, 0.f, 100.f, 1.f, 5.f, 95.f, 99.f, 50.f, 0.f, 75.f })
            {
                setParam (p, "master_mix", target);
                for (int sample = 0; sample < latency * 2 + 2048; sample += blockSize)
                {
                    juce::AudioBuffer<float> a (2, blockSize), b;
                    for (int i = 0; i < blockSize; ++i)
                        for (int c = 0; c < 2; ++c)
                        {
                            random = random * 1664525u + 1013904223u;
                            a.setSample (c, i, (float) (((double) (random >> 8) / 16777216.0 - 0.5) * 0.6));
                        }
                    b.makeCopyOf (a); p.processBlock (a, midi); reference.processBlock (b, midi);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < blockSize; ++i)
                            error = std::max (error, std::abs (a.getSample (c, i) - b.getSample (c, i)));
                }
            }
            check (error < 6.e-8f && p.getLatencySamples() == latency,
                "mix endpoints/transitions stay aligned rate=" + juce::String (sr) + " block="
                + juce::String (blockSize) + " maxError=" + juce::String (error, 9));
        }

    // The active flat EQ must be phase coherent with compensated dry in every
    // rack order, not merely when it is bypassed (the old test's blind spot).
    for (double sr : { 48000., 96000. })
    {
        std::array<int, 4> order { 0, 1, 2, 3 };
        bool all = true;
        double worstPhase = 0;
        do
        {
            ZychanStripProcessor p;
            setParam (p, "colour_mode", 2); setParam (p, "lim_auto_gain", 0);
            setParam (p, "master_mix", 50); p.setOrder (order);
            p.setPlayConfigDetails (2, 2, sr, 128); p.prepareToPlay (sr, 128);
            for (double hz : { 8000., 16000. })
            {
                std::complex<double> output {};
                const int count = (int) (sr / 10);
                const int total = (((int) (sr / 5) + count + 127) / 128) * 128;
                juce::MidiBuffer midi;
                for (int t = 0; t < total; t += 128)
                {
                    juce::AudioBuffer<float> b (2, 128);
                    for (int i = 0; i < 128; ++i)
                        for (int c = 0; c < 2; ++c)
                            b.setSample (c, i, .01f * (float) std::sin (2 * pi * hz * (t + i) / sr));
                    p.processBlock (b, midi);
                    for (int i = 0; i < 128; ++i)
                        if (t + i >= total - count)
                        {
                            const double phase = 2 * pi * hz * (t + i - p.getLatencySamples()) / sr;
                            output += (double) b.getSample (0, i) * std::complex<double> (std::sin (phase), std::cos (phase));
                        }
                }
                const double phase = std::abs (std::arg (output));
                const double gain = 2 * std::abs (output) / (count * .01);
                worstPhase = std::max (worstPhase, phase);
                all &= phase < 1.e-4 && gain > .98 && gain < 1.02;
            }
        } while (std::next_permutation (order.begin(), order.end()));
        check (all, "50% mix active flat CLEAN rack coherent in all 24 orders rate=" + juce::String (sr)
            + " phaseDegrees=" + juce::String (worstPhase * 180 / pi, 6));
    }

    // Pinned capture transport offsets/polarity must be compensated independently
    // of their remaining analog frequency response. Test the actual neural engine.
    for (double sr : { 44100., 48000., 96000., 192000. })
        for (int model = 0; model < 3; ++model)
        {
            rack::ColourEngine engine; engine.prepare (sr, model, 1);
            bool aligned = true;
            double worstDegrees = 0;
            for (double hz : { 1000., 4000., 8000. })
            {
                std::complex<double> result {};
                const int total = (int) (sr / 2), count = (int) (sr / 4);
                for (int t = 0; t < total; ++t)
                {
                    float raw[2] { .01f * (float) std::sin (2 * pi * hz * t / sr), 0 }, output[2] {};
                    engine.frame (raw, raw, output, 1, 1);
                    if (t >= total - count)
                    {
                        const double phase = 2 * pi * hz * (t - engine.latency()) / sr;
                        result += (double) output[0] * std::complex<double> (std::sin (phase), std::cos (phase));
                    }
                }
                worstDegrees = std::max (worstDegrees, std::abs (std::arg (result)) * 180 / pi);
                aligned &= result.real() > 0 && std::abs (std::arg (result)) < pi / 9;
            }
            check (aligned, "NAM transport/polarity aligned model=" + juce::String (model)
                + " rate=" + juce::String (sr) + " worstDegrees=" + juce::String (worstDegrees, 3));
        }
    // Quantify cancellation in the complete active NAM rack against an in-phase
    // blend of equal component amplitudes. Ordinary captured EQ remains allowed;
    // deep comb notches from polarity/transport offsets do not.
    for (double sr : { 44100., 48000., 96000., 192000. })
        for (float level : { .01f, .1f, .5f })
        {
            bool valid = true;
            double worstLoss = 0;
            for (double hz : { 100., 1000., 4000., 8000., 16000. })
            {
                ZychanStripProcessor half, full;
                for (auto* p : { &half, &full })
                {
                    setParam (*p, "colour_mode", 1); setParam (*p, "lim_auto_gain", 0);
                    setParam (*p, "master_mix", p == &half ? 50.f : 100.f);
                    p->setPlayConfigDetails (2, 2, sr, 128); p->prepareToPlay (sr, 128);
                }
                const int count = (int) (sr / 10);
                const int total = (((int) (sr / 5) + count + 127) / 128) * 128;
                std::complex<double> mixed {}, wet {};
                juce::MidiBuffer midi;
                for (int t = 0; t < total; t += 128)
                {
                    juce::AudioBuffer<float> a (2, 128), b;
                    for (int i = 0; i < 128; ++i)
                        for (int c = 0; c < 2; ++c)
                            a.setSample (c, i, level * (float) std::sin (2 * pi * hz * (t + i) / sr));
                    b.makeCopyOf (a); half.processBlock (a, midi); full.processBlock (b, midi);
                    for (int i = 0; i < 128; ++i)
                        if (t + i >= total - count)
                        {
                            const double phase = 2 * pi * hz * (t + i - half.getLatencySamples()) / sr;
                            const std::complex<double> basis (std::sin (phase), std::cos (phase));
                            mixed += (double) a.getSample (0, i) * basis;
                            wet += (double) b.getSample (0, i) * basis;
                        }
                }
                const double coherent = .5 * (count * level / 2 + std::abs (wet));
                const double loss = 20 * std::log10 (std::abs (mixed) / coherent);
                worstLoss = std::min (worstLoss, loss);
                valid &= std::isfinite (loss) && loss > -3.0;
            }
            check (valid, "50% active NAM mix avoids transport comb cancellation rate=" + juce::String (sr)
                + " peak=" + juce::String (level) + " worstAdditionalLossDb=" + juce::String (worstLoss, 3));
        }

}
