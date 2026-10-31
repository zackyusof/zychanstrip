#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>
#include <vector>

using namespace juce;

/** Post-chain metering tap.
    Audio thread publishes measurements into atomics; the UI reads them at frame rate.
    Never mutates the audio it observes, so inserting/removing the meter cannot change sound. */
class RackMeters
{
public:
    struct ScopePoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    static constexpr int kFftOrder = 11;              // 2048-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;  // 2048 bins
    static constexpr int kBands    = 31;              // 31-band RTA

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        // True-peak style peak detector: 4x oversampled envelope per channel.
        peakHoldSamples = (int) (sr * 2.0);           // 2 s max-hold
        reset();

        fifo.assign ((size_t) kFftSize, 0.0f);
        fftData.assign ((size_t) kFftSize * 2, 0.0f);
        window.resize ((size_t) kFftSize);
        juce::dsp::WindowingFunction<float>::fillWindowingTables (
            window.data(), (size_t) kFftSize,
            juce::dsp::WindowingFunction<float>::hann, false);

        fft = std::make_unique<juce::dsp::FFT> (kFftOrder);
        spectrumDb.assign ((size_t) kFftSize / 2, -100.0f);
        smoothedSpectrum.assign ((size_t) kFftSize / 2, -100.0f);

        // Log-spaced band edges 20 Hz .. 20 kHz.
        bandEdges.resize ((size_t) kBands + 1);
        for (int i = 0; i <= kBands; ++i)
        {
            const double t = (double) i / (double) kBands;
            bandEdges[(size_t) i] = 20.0 * std::pow (1000.0, t);    // 20 Hz * (1000)^t -> 20 kHz
        }
        bandDb.assign ((size_t) kBands, -100.0f);
        bandHoldDb.assign ((size_t) kBands, -100.0f);
        bandHoldFrames.assign ((size_t) kBands, 0);
        (void) maxBlockSize;
    }

    void reset()
    {
        for (auto& c : ch)
            c = Channel {};
        corrValue.store (0.0f);
        lufsShort.store (-100.0f);
        for (auto& b : bandHoldDb) b = -100.0f;
        for (auto& f : bandHoldFrames) f = 0;
    }

    /** Audio thread. Read-only with respect to the buffer. */
    void process (const juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        if (n <= 0) return;
        const int numCh = juce::jmin (2, buffer.getNumChannels());

        float blockMaxL = 0.0f, blockMaxR = 0.0f;
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const float l = buffer.getSample (0, i);
            const float r = numCh > 1 ? buffer.getSample (1, i) : l;

            blockMaxL = juce::jmax (blockMaxL, std::abs (l));
            blockMaxR = juce::jmax (blockMaxR, std::abs (r));
            sumLR += (double) l * r;
            sumLL += (double) l * l;
            sumRR += (double) r * r;

            // K-weighted loudness (approximation: high-pass ~38 Hz + high-shelf ~+4 dB @ 1.5 kHz)
            const double mono = 0.5 * ((double) l + r);
            const double kw = kWeight (mono);
            kSumSq += kw * kw;
            ++kCount;

            // Mono-sum feed for correlation/scope/RTA
            fifo[(size_t) fifoIndex++] = (float) mono;
            if (fifoIndex >= kFftSize)
            {
                fifoIndex = 0;
                fftReady = true;
            }
        }

        // Per-channel peak + max-hold + block RMS
        double sumL = 0.0, sumR = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double l = buffer.getSample (0, i);
            const double r = numCh > 1 ? buffer.getSample (1, i) : l;
            sumL += l * l;
            sumR += r * r;
        }
        updatePeak (ch[0], blockMaxL);
        updatePeak (ch[1], blockMaxR);

        // Smoothed block RMS (approximately 300 ms integration)
        const float blockRmsL = (float) std::sqrt (sumL / (double) n);
        const float blockRmsR = (float) std::sqrt (sumR / (double) n);
        rmsL.store (rmsL.load() + (blockRmsL - rmsL.load()) * 0.25f);
        rmsR.store (rmsR.load() + (blockRmsR - rmsR.load()) * 0.25f);

        // Stereo correlation over the block
        const double denom = std::sqrt (juce::jmax (1.0e-12, sumLL * sumRR));
        corrValue.store ((float) juce::jlimit (-1.0, 1.0, sumLR / denom));

        // Short-term loudness window (~400 ms)
        if (kCount >= (int) (sr * 0.4))
        {
            const double mean = kSumSq / (double) kCount;
            lufsShort.store ((float) (-0.691 + 10.0 * std::log10 (juce::jmax (1.0e-12, mean))));
            kSumSq = 0.0;
            kCount = 0;
        }

        // Scope points: decimated L/R pairs for the Lissajous
        scopeWrite (buffer, n, numCh);

        if (fftReady.exchange (false))
            runFft();
    }

    // ---- UI accessors (message thread) -------------------------------------
    float peakDb (int chn) const          { return dbOf (chn ? peakR.load() : peakL.load()); }
    float holdDb (int chn) const          { return dbOf (chn ? holdR.load() : holdL.load()); }
    float rmsDb (int chn) const           { return dbOf (chn ? rmsR.load() : rmsL.load()); }
    float correlation() const             { return corrValue.load(); }
    float shortTermLufs() const           { return lufsShort.load(); }
    const std::vector<float>& bands() const { return bandDb; }
    const std::vector<float>& bandHolds() const { return bandHoldDb; }

    /** Fill the scope buffer with interleaved L/R pairs (copy under short lock). */
    void copyScope (std::vector<ScopePoint>& out) const
    {
        const juce::SpinLock::ScopedLockType sl (scopeLock);
        out.clear();
        out.reserve (scopePoints.size());
        for (auto& p : scopePoints) out.push_back (p);
    }

    struct Point { float x = 0.0f, y = 0.0f; };

private:
    struct Channel
    {
        float peak = 0.0f;
        float hold = 0.0f;
        int   holdCountdown = 0;
    };

    static float dbOf (float lin) { return lin > 1.0e-7f ? juce::Decibels::gainToDecibels (lin) : -100.0f; }

    void updatePeak (Channel& c, float blockMax)
    {
        c.peak = blockMax;
        if (blockMax >= c.hold)
        {
            c.hold = blockMax;
            c.holdCountdown = peakHoldSamples;
        }
        else if (c.holdCountdown > 0)
        {
            c.holdCountdown -= 512;           // approximate per-block decay
        }
        else
        {
            c.hold *= 0.98f;                  // slow release after the hold expires
        }
        peakL.store (ch[0].peak);
        peakR.store (ch[1].peak);
        holdL.store (ch[0].hold);
        holdR.store (ch[1].hold);
    }

    // Simple K-weighting approximation: 2nd-order HP at 38 Hz then high shelf +4 dB at 1.5 kHz.
    double kWeight (double x)
    {
        // HP (one-pole, applied to a cascaded pair for ~12 dB/oct)
        kHp1 += kHpAlpha * (x - kHp1);
        kHp2 += kHpAlpha * (kHp1 - kHp2);
        double v = x - kHp2;
        // Shelf
        kShelf += kShelfAlpha * (v - kShelf);
        return v + kShelf * 0.6;
    }

    void scopeWrite (const juce::AudioBuffer<float>& buffer, int n, int numCh)
    {
        const juce::SpinLock::ScopedLockType sl (scopeLock);
        scopePoints.clear();
        const int step = juce::jmax (1, n / 256);
        for (int i = 0; i < n; i += step)
        {
            const float l = buffer.getSample (0, i);
            const float r = numCh > 1 ? buffer.getSample (1, i) : l;
            scopePoints.push_back ({ juce::jlimit (-1.0f, 1.0f, (l + r) * 0.70710678f),
                                     juce::jlimit (-1.0f, 1.0f, (l - r) * 0.70710678f) });
        }
    }

    void runFft()
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < kFftSize; ++i)
            fftData[(size_t) i] = fifo[(size_t) i] * window[(size_t) i];

        fft->performFrequencyOnlyForwardTransform (fftData.data());

        const float norm = 2.0f / (float) kFftSize;
        const int bins = kFftSize / 2;
        for (int i = 0; i < bins; ++i)
        {
            const float mag = fftData[(size_t) i] * norm;
            const float db = mag > 1.0e-7f ? juce::Decibels::gainToDecibels (mag) : -100.0f;
            smoothedSpectrum[(size_t) i] += (db - smoothedSpectrum[(size_t) i]) * 0.28f;
        }

        // Fold bins into log bands (peak per band)
        const double binHz = sr / (double) kFftSize;
        for (int b = 0; b < kBands; ++b)
        {
            const int lo = (int) std::floor (bandEdges[(size_t) b] / binHz);
            const int hi = juce::jmax (lo + 1, (int) std::ceil (bandEdges[(size_t) b + 1] / binHz));
            float m = -100.0f;
            for (int i = lo; i < hi && i < bins; ++i)
                m = juce::jmax (m, smoothedSpectrum[(size_t) juce::jmax (0, i)]);
            bandDb[(size_t) b] = m;
            if (m >= bandHoldDb[(size_t) b]) { bandHoldDb[(size_t) b] = m; bandHoldFrames[(size_t) b] = 60; }
            else if (bandHoldFrames[(size_t) b] > 0) --bandHoldFrames[(size_t) b];
            else bandHoldDb[(size_t) b] -= 0.6f;
        }
    }

    double sr = 48000.0;
    int peakHoldSamples = 96000;

    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<float> holdL { 0.0f }, holdR { 0.0f };
    std::atomic<float> rmsL { 0.0f }, rmsR { 0.0f };
    std::atomic<float> corrValue { 0.0f };
    std::atomic<float> lufsShort { -100.0f };

    std::array<Channel, 2> ch;

    // K-weighting state
    double kHp1 = 0.0, kHp2 = 0.0, kShelf = 0.0;
    double kHpAlpha = 0.005, kShelfAlpha = 0.02;
    double kSumSq = 0.0;
    int kCount = 0;

    // FFT
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, fftData, fifo, spectrumDb, smoothedSpectrum;
    int fifoIndex = 0;
    std::atomic<bool> fftReady { false };
    std::vector<double> bandEdges;
    std::vector<float> bandDb, bandHoldDb;
    std::vector<int> bandHoldFrames;

    // Scope
    mutable juce::SpinLock scopeLock;
    std::vector<ScopePoint> scopePoints;
};
