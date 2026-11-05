// Measure the actual NAM renderer at nominal reference, plus below/above it.
// These are software calibration checks, not hardware-transfer validation.
void operatingLevelTests()
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    for (double sr : { 44100., 48000., 96000. })
        for (int model = 0; model < 4; ++model)
        {
            rack::ColourEngine engine;
            engine.prepare(sr, model, 1);
            double nominalError = 0, previousRms = 0;
            bool valid = true, rising = true;
            int64_t t = 0;
            for (float offset : { -18.f, -6.f, 0.f, 6.f })
            {
                const double peak = rack::operating::calibrationPeak(model) * std::pow(10., offset / 20.);
                double inputEnergy = 0, outputEnergy = 0;
                const int count = (int)(sr * .4);
                for (int i = 0; i < count; ++i, ++t)
                {
                    float input[2]{(float)(peak * std::sin(twoPi * 1000 * t / sr)), 0}, out[2]{};
                    input[1] = input[0];
                    // Tape's complete capture is internal; frame exposes the bounded
                    // residual. Test reference unity on the other three full captures.
                    engine.frame(input, input, out, 2, 1, model == 3 ? .3f : 1.f);
                    valid &= std::isfinite(out[0]) && out[0] == out[1];
                    if (i >= count / 2)
                    {
                        inputEnergy += input[0] * (double)input[0];
                        outputEnergy += out[0] * (double)out[0];
                    }
                }
                const double rms = std::sqrt(outputEnergy / (count / 2));
                rising &= rms > previousRms;
                previousRms = rms;
                if (offset == 0) nominalError = 10 * std::log10(outputEnergy / inputEnergy);
            }
            check(valid && rising && std::abs(nominalError) < (model == 3 ? 1.0 : .12),
                  "UA reference NAM rate=" + juce::String(sr) + " model=" + juce::String(model)
                  + " nominal error dB=" + juce::String(nominalError, 4) + " rising level sweep");
        }

    // Exercise the additional features in every order with actual colour and
    // changing levels. No reset/normalization is inserted at module boundaries.
    for (int mode = 0; mode < 3; ++mode)
        for (int mix : { 0, 50, 100 })
            for (bool unity : { false, true })
            {
                ZychanStripProcessor proc;
                setParam(proc, "colour_mode", (float)mode);
                setParam(proc, "master_mix", (float)mix);
                setParam(proc, "unity", unity ? 1.f : 0.f);
                setParam(proc, "lim_auto_gain", 1);
                setParam(proc, "con_eq_1k", 2);
                setParam(proc, "fet_input", 6);
                setParam(proc, "opto_peak", 35);
                setParam(proc, "opto_gain", 10);
                setParam(proc, "lim_gain", 6);
                setParam(proc, "lim_enhance", 100);
                proc.setPlayConfigDetails(2, 2, 48000, 128);
                proc.prepareToPlay(48000, 128);
                const int latency = proc.getLatencySamples();
                std::array<int,4> order{0,1,2,3};
                bool valid = true;
                int64_t time = 0;
                do
                {
                    proc.setOrder(order);
                    for (int block = 0; block < 80; ++block)
                    {
                        juce::AudioBuffer<float> b(2,128);
                        const float level = block < 20 ? .01f : block < 40 ? .05f : block < 60 ? .2f : .7f;
                        for (int i=0;i<128;++i)
                            for (int c=0;c<2;++c)
                                b.setSample(c,i,level*(float)std::sin(twoPi*997*(time+i)/48000.));
                        juce::MidiBuffer midi;
                        proc.processBlock(b,midi);
                        for (int i=0;i<128;++i)
                            valid &= std::isfinite(b.getSample(0,i)) && b.getSample(0,i)==b.getSample(1,i)
                                     && std::abs(b.getSample(0,i)) < 8.f;
                        valid &= proc.getLatencySamples() == latency;
                        time += 128;
                    }
                } while(std::next_permutation(order.begin(),order.end()));
                check(valid, "UA feature interaction all 24 orders mode=" + juce::String(mode)
                      + " mix=" + juce::String(mix) + " unity=" + juce::String((int)unity));
            }
}
