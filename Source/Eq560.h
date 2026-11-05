#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace rack
{
/** Deterministic EQ core adapted from chokehold's MIT-licensed EQ 560.
    Exact source revision and intentional differences: THIRD_PARTY_NOTICES.md. */
class Eq560
{
public:
    static constexpr std::array<double, 10> frequencies { 16000, 8000, 4000, 2000, 1000, 500, 250, 125, 63, 31 };
    static constexpr std::array<const char*, 10> bandIds { "eq_16k", "eq_8k", "eq_4k", "eq_2k", "eq_1k",
                                                         "eq_500", "eq_250", "eq_125", "eq_63", "eq_31" };

    static constexpr int latencySamples() { return 31; }

    static double bandWidth (double db)
    {
        const double mu = std::pow (std::abs (db) * 0.08333333333, 2.0);
        return 0.126984 * (1.0 - mu) + 5.763566 * mu;
    }

    void setBandGains (const std::array<float, 10>& values)
    {
        for (size_t i = 0; i < values.size(); ++i)
            target[i] = std::clamp ((double) values[i], -12.0, 12.0);
    }

    void prepare (double sr)
    {
        rate = sr;
        smoothing = std::exp (-1.0 / (0.02 * sr));
        current = target;
        updateCounter = 0;
        coefficientsDirty = false;
        for (auto& channel : channels)
        {
            channel = {};
            channel.up.prepare();
            channel.down.prepare();
        }
        updateCoefficients();
    }

    // Call once per native sample frame, then processSample for each channel.
    void beginSample()
    {
        bool changed = false;
        for (size_t i = 0; i < current.size(); ++i)
        {
            if (std::abs (current[i] - target[i]) < 1.0e-12) { current[i] = target[i]; continue; }
            current[i] = target[i] + smoothing * (current[i] - target[i]);
            if (std::abs (current[i] - target[i]) < 1.0e-8) current[i] = target[i];
            changed = true;
        }
        if (++updateCounter >= 8)
        {
            updateCounter = 0;
            if (changed || coefficientsDirty) updateCoefficients();
            coefficientsDirty = changed;
        }
    }

    double processSample (int channelIndex, double input)
    {
        auto& channel = channels[(size_t) channelIndex];
        double real = channel.up.tick (input * 2.0);
        double stuffed = channel.up.tick (0.0);
        for (size_t i = 0; i < current.size(); ++i)
        {
            // A zero-gain stage is the upstream direct-through path. Reset its
            // integrators when inactive so automation never revives stale audio.
            if (current[i] == 0.0) { channel.bands[i].reset(); continue; }
            real = channel.bands[i].tick (real);
            stuffed = channel.bands[i].tick (stuffed);
        }
        const double output = channel.down.tick (real);
        channel.down.tick (stuffed);
        return output;
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    struct Peak
    {
        double a1 = 1, a2 = 0, a3 = 0, m1 = 0, ic1 = 0, ic2 = 0;
        void reset() { ic1 = ic2 = 0; }
        void prepare (double sr, double hz, double q, double db)
        {
            const double A = std::pow (10.0, db * 0.025);
            const double g = std::tan (pi * hz / sr);
            const double k = 1.0 / (q * A);
            a1 = 1.0 / (1.0 + g * (g + k)); a2 = a1 * g; a3 = a2 * g;
            m1 = k * (A * A - 1.0);
        }
        double tick (double sample)
        {
            const double v3 = sample - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
            return sample + m1 * v1;
        }
    };
    // Symmetric 63-tap halfband interpolation/decimation. The two filters
    // together add exactly 31 native samples, without the old Butterworth
    // phase rotation that cancelled a flat EQ against dry at partial mix.
    struct Halfband
    {
        static constexpr int taps = 63, centre = 31;
        std::array<double, taps> coefficients {};
        std::array<double, taps * 2> history {};
        int position = 0;
        void prepare()
        {
            history.fill (0); position = 0;
            double sum = 0;
            for (int i = 0; i < taps; ++i)
            {
                const int offset = i - centre;
                const double window = 0.42 - 0.5 * std::cos (2 * pi * i / (taps - 1))
                                           + 0.08 * std::cos (4 * pi * i / (taps - 1));
                coefficients[(size_t) i] = offset == 0 ? 0.5 : (offset % 2 == 0 ? 0.0
                    : std::sin (0.5 * pi * offset) / (pi * offset) * window);
                sum += coefficients[(size_t) i];
            }
            for (auto& value : coefficients) value /= sum;
        }
        double tick (double sample)
        {
            history[(size_t) position] = history[(size_t) (position + taps)] = sample;
            const double* data = history.data() + position + 1;
            double output = coefficients[centre] * data[centre];
            for (int i = 0; i < taps; i += 2) output += coefficients[(size_t) i] * data[i];
            if (++position == taps) position = 0;
            return output;
        }
    };
    struct Channel { Halfband up, down; std::array<Peak, 10> bands; };
    void updateCoefficients()
    {
        for (auto& channel : channels)
            for (size_t i = 0; i < current.size(); ++i)
                channel.bands[i].prepare (rate * 2.0, frequencies[i], bandWidth (current[i]), current[i]);
    }
    std::array<Channel, 2> channels;
    std::array<double, 10> target {}, current {};
    double rate = 44100.0, smoothing = 0;
    int updateCounter = 0;
    bool coefficientsDirty = false;
};
} // namespace rack
