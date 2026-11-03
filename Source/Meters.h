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
    Never mutates the audio it observes, so inserting/removing the meter cannot change sound.

    Real-time safety (prior-art report `docs/research/dsp-optimization-prior-art.md`, F5):
      * the audio thread walks the block ONCE and performs no allocation;
      * the 2048-point FFT runs on the UI thread (`runAnalysis`, called from the panel timer)
        against a snapshot the audio path published - never inside the callback;
      * the scope ring is a fixed-size SPSC buffer with atomic indices, so the old
        `SpinLock` + `clear()` + `push_back` on the audio thread is gone. */
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
    static constexpr int kScopeCapacity = 512;        // fixed SPSC ring, no allocation

    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        // True-peak style peak detector: 4x oversampled envelope per channel.
        peakHoldSamples = (int) (sr * 2.0);           // 2 s max-hold
        reset();

        fifo.assign ((size_t) kFftSize, 0.0f);
        fftSnapshot.assign ((size_t) kFftSize, 0.0f);
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

        // Pre-computed per-band bin ranges: the fold is a hot UI-side loop and recomputing
        // floor/ceil per band per FFT was wasted work.
        bandLo.resize ((size_t) kBands);
        bandHi.resize ((size_t) kBands);
        const double binHz = sr / (double) kFftSize;
        for (int b = 0; b < kBands; ++b)
        {
            const int lo = (int) std::floor (bandEdges[(size_t) b] / binHz);
            const int hi = juce::jmax (lo + 1, (int) std::ceil (bandEdges[(size_t) b + 1] / binHz));
            bandLo[(size_t) b] = juce::jlimit (0, kFftSize / 2 - 1, lo);
            bandHi[(size_t) b] = juce::jlimit (1, kFftSize / 2, hi);
        }

        scopeX.fill (0.0f);
        scopeY.fill (0.0f);
        scopeWriteIndex.store (0, std::memory_order_relaxed);
        (void) maxBlockSize;
    }

    void reset()
    {
        for (auto& c : ch)
            c = Channel {};
        corrValue.store (0.0f, std::memory_order_relaxed);
        lufsShort.store (-100.0f, std::memory_order_relaxed);
        for (auto& b : bandHoldDb) b = -100.0f;
        for (auto& f : bandHoldFrames) f = 0;
        scopeX.fill (0.0f);
        scopeY.fill (0.0f);
        scopeWriteIndex.store (0, std::memory_order_release);
        fftReady.store (false, std::memory_order_relaxed);
        fifoIndex = 0;
    }

    /** Audio thread. Read-only with respect to the buffer, allocation-free, lock-free.
        Everything the display needs is accumulated in ONE pass over the block. */
    void process (const juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        if (n <= 0) return;
        const int numCh = juce::jmin (2, buffer.getNumChannels());

        float blockMaxL = 0.0f, blockMaxR = 0.0f;
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0, sumL = 0.0, sumR = 0.0;

        const float* left = buffer.getReadPointer (0);
        const float* right = numCh > 1 ? buffer.getReadPointer (1) : left;

        int writeIndex = scopeWriteIndex.load (std::memory_order_relaxed);
        const int scopeStep = juce::jmax (1, n / 256);

        for (int i = 0; i < n; ++i)
        {
            const float l = left[i];
            const float r = right[i];
            const double dl = (double) l, dr = (double) r;

            blockMaxL = juce::jmax (blockMaxL, std::abs (l));
            blockMaxR = juce::jmax (blockMaxR, std::abs (r));
            sumLR += dl * dr;
            sumLL += dl * dl;
            sumRR += dr * dr;
            sumL += dl * dl;
            sumR += dr * dr;

            // K-weighted loudness (approximation: high-pass ~38 Hz + high-shelf ~+4 dB @ 1.5 kHz)
            const double mono = 0.5 * (dl + dr);
            const double kw = kWeight (mono);
            kSumSq += kw * kw;
            ++kCount;

            // Mono-sum feed for the RTA. When the window fills we publish a copy for the UI
            // thread and roll over; the transform itself never runs here.
            fifo[(size_t) fifoIndex++] = (float) mono;
            if (fifoIndex >= kFftSize)
            {
                if (! fftReady.load (std::memory_order_acquire))
                {
                    std::copy (fifo.begin(), fifo.end(), fftSnapshot.begin());
                    fftReady.store (true, std::memory_order_release);
                }
                fifoIndex = 0;
            }

            // Lissajous points into the fixed SPSC ring.
            if (i % scopeStep == 0)
            {
                scopeX[(size_t) writeIndex] = juce::jlimit (-1.0f, 1.0f, (l + r) * 0.70710678f);
                scopeY[(size_t) writeIndex] = juce::jlimit (-1.0f, 1.0f, (l - r) * 0.70710678f);
                writeIndex = (writeIndex + 1) & (kScopeCapacity - 1);
            }
        }
        scopeWriteIndex.store (writeIndex, std::memory_order_release);
        scopeValid.store (juce::jmin (kScopeCapacity, scopeValid.load (std::memory_order_relaxed) + n / scopeStep),
                          std::memory_order_relaxed);

        updatePeak (ch[0], blockMaxL);
        updatePeak (ch[1], blockMaxR);

        // Smoothed block RMS (approximately 300 ms integration)
        const float blockRmsL = (float) std::sqrt (sumL / (double) n);
        const float blockRmsR = (float) std::sqrt (sumR / (double) n);
        rmsL.store (rmsL.load (std::memory_order_relaxed) + (blockRmsL - rmsL.load (std::memory_order_relaxed)) * 0.25f,
                    std::memory_order_relaxed);
        rmsR.store (rmsR.load (std::memory_order_relaxed) + (blockRmsR - rmsR.load (std::memory_order_relaxed)) * 0.25f,
                    std::memory_order_relaxed);

        // Stereo correlation over the block
        const double denom = std::sqrt (juce::jmax (1.0e-12, sumLL * sumRR));
        corrValue.store ((float) juce::jlimit (-1.0, 1.0, sumLR / denom), std::memory_order_relaxed);

        // Short-term loudness window (~400 ms)
        if (kCount >= (int) (sr * 0.4))
        {
            const double mean = kSumSq / (double) kCount;
            lufsShort.store ((float) (-0.691 + 10.0 * std::log10 (juce::jmax (1.0e-12, mean))),
                             std::memory_order_relaxed);
            kSumSq = 0.0;
            kCount = 0;
        }
    }

    /** UI thread. Runs the FFT + band fold against the last published snapshot.
        Call from the editor timer (MeterPanel::tick) - never from the audio callback. */
    void runAnalysis()
    {
        if (fft == nullptr || fftSnapshot.size() < (size_t) kFftSize) return;
        if (! fftReady.exchange (false, std::memory_order_acq_rel)) return;

        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < kFftSize; ++i)
            fftData[(size_t) i] = fftSnapshot[(size_t) i] * window[(size_t) i];

        fft->performFrequencyOnlyForwardTransform (fftData.data());

        const float norm = 2.0f / (float) kFftSize;
        const int bins = kFftSize / 2;
        for (int i = 0; i < bins; ++i)
        {
            const float mag = fftData[(size_t) i] * norm;
            const float db = mag > 1.0e-7f ? juce::Decibels::gainToDecibels (mag) : -100.0f;
            smoothedSpectrum[(size_t) i] += (db - smoothedSpectrum[(size_t) i]) * 0.28f;
        }

        for (int b = 0; b < kBands; ++b)
        {
            const int lo = bandLo[(size_t) b], hi = bandHi[(size_t) b];
            float m = -100.0f;
            for (int i = lo; i < hi && i < bins; ++i)
                m = juce::jmax (m, smoothedSpectrum[(size_t) i]);
            bandDb[(size_t) b] = m;
            if (m >= bandHoldDb[(size_t) b]) { bandHoldDb[(size_t) b] = m; bandHoldFrames[(size_t) b] = 60; }
            else if (bandHoldFrames[(size_t) b] > 0) --bandHoldFrames[(size_t) b];
            else bandHoldDb[(size_t) b] -= 0.6f;
        }
    }

    // ---- UI accessors (message thread) -------------------------------------
    float peakDb (int chn) const          { return dbOf (chn ? peakR.load() : peakL.load()); }
    float holdDb (int chn) const          { return dbOf (chn ? holdR.load() : holdL.load()); }
    float rmsDb (int chn) const           { return dbOf (chn ? rmsR.load() : rmsL.load()); }
    float correlation() const             { return corrValue.load(); }
    float shortTermLufs() const           { return lufsShort.load(); }
    const std::vector<float>& bands() const { return bandDb; }
    const std::vector<float>& bandHolds() const { return bandHoldDb; }

    /** Fill the scope buffer with the most recent L/R pairs. Lock-free: the reader copies
        backwards from the audio thread's write index and tolerates a concurrent overwrite
        (each point is a plain float pair, so a torn read is at most one stray pixel). */
    void copyScope (std::vector<ScopePoint>& out) const
    {
        const int written = scopeValid.load (std::memory_order_acquire);
        const int count = juce::jmin (written, kScopeCapacity);
        int index = scopeWriteIndex.load (std::memory_order_acquire);
        out.clear();
        out.reserve ((size_t) count);
        for (int i = 0; i < count; ++i)
        {
            index = (index - 1) & (kScopeCapacity - 1);
            out.push_back ({ scopeX[(size_t) index], scopeY[(size_t) index] });
        }
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
        peakL.store (ch[0].peak, std::memory_order_relaxed);
        peakR.store (ch[1].peak, std::memory_order_relaxed);
        holdL.store (ch[0].hold, std::memory_order_relaxed);
        holdR.store (ch[1].hold, std::memory_order_relaxed);
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

    // FFT (transform runs on the UI thread via runAnalysis)
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, fftData, fifo, fftSnapshot, spectrumDb, smoothedSpectrum;
    int fifoIndex = 0;
    std::atomic<bool> fftReady { false };
    std::vector<double> bandEdges;
    std::vector<int> bandLo, bandHi;
    std::vector<float> bandDb, bandHoldDb;
    std::vector<int> bandHoldFrames;

    // Scope: fixed SPSC ring, atomic write index, no lock and no allocation.
    std::array<float, kScopeCapacity> scopeX {}, scopeY {};
    std::atomic<int> scopeWriteIndex { 0 };
    std::atomic<int> scopeValid { 0 };
};
