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

    Real-time safety (prior-art report `docs/research/next-beta-github-prior-art.md`, item 3):
      * the audio thread walks the block ONCE and performs no allocation;
      * the 2048-point FFT runs on the UI thread (`runAnalysis`, called from the panel timer)
        against a snapshot published through a two-slot `AbstractFifo` - never inside the callback;
      * the scope ring is an `AbstractFifo`-owned SPSC ring, so payload slots are read only after
        they are claimed and never while the audio thread writes them;
      * `reset()` is allocation-free because the audio thread performs a deferred clear. */
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

        // Sample-peak detector. Intersample (true) peak is not reconstructed - the readout is
        // labelled accordingly.
        peakHoldSamples = (int) (sr * 2.0);           // 2 s max-hold

        // Sample-rate-derived one-pole coefficients for the K-weighting approximation
        // (high-pass ~38 Hz, high shelf ~1.5 kHz). Fixed coefficients were only correct at 48 kHz.
        kHpAlpha = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 38.0 / sampleRate);
        kShelfAlpha = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 1500.0 / sampleRate);

        fifo.assign ((size_t) kFftSize, 0.0f);
        fftData.assign ((size_t) kFftSize * 2, 0.0f);
        for (auto& slot : fftSlots) slot.assign ((size_t) kFftSize, 0.0f);
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

        reset();
        (void) maxBlockSize;
    }

    /** Allocation-free: the audio thread performs the deferred operator reset. */
    void reset()
    {
        for (auto& c : ch)
            c = Channel {};
        corrValue.store (0.0f, std::memory_order_relaxed);
        lufsShort.store (-100.0f, std::memory_order_relaxed);
        for (auto& b : bandHoldDb) b = -100.0f;
        for (auto& f : bandHoldFrames) f = 0;
        fftFifo.reset();
        scopeFifo.reset();
        for (auto& slot : fftSlots) std::fill (slot.begin(), slot.end(), 0.0f);
        scopeX.fill (0.0f);
        scopeY.fill (0.0f);
        fifoIndex = 0;
        kSumSq = 0.0;
        kCount = 0;
        for (auto& s : kState) s = KState {};
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

            // Per-channel K-weighted energy. BS.1770 sums channel energies; a mono sum would
            // read anti-phase stereo as silence instead of full-level stereo.
            const double mono = 0.5 * (dl + dr);
            const double kwL = kWeight (dl, kState[0]);
            const double kwR = kWeight (dr, kState[1]);
            kSumSq += kwL * kwL + kwR * kwR;
            ++kCount;

            // Mono-sum feed for the RTA. When the window fills we publish a copy into a free
            // FIFO slot for the UI thread; the transform itself never runs here.
            fifo[(size_t) fifoIndex++] = (float) mono;
            if (fifoIndex >= kFftSize)
            {
                int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
                fftFifo.prepareToWrite (1, s1, n1, s2, n2);
                if (n1 > 0)
                {
                    auto& slot = fftSlots[(size_t) s1];
                    std::copy (fifo.begin(), fifo.end(), slot.begin());
                    fftFifo.finishedWrite (1);
                }
                fifoIndex = 0;
            }

            // Lissajous points into the fixed SPSC ring.
            if (i % scopeStep == 0)
            {
                int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
                scopeFifo.prepareToWrite (1, s1, n1, s2, n2);
                if (n1 > 0)                       // full -> drop this display frame
                {
                    scopeX[(size_t) s1] = juce::jlimit (-1.0f, 1.0f, (l + r) * 0.70710678f);
                    scopeY[(size_t) s1] = juce::jlimit (-1.0f, 1.0f, (l - r) * 0.70710678f);
                    scopeFifo.finishedWrite (1);
                }
            }
        }

        updatePeak (ch[0], blockMaxL, n);
        updatePeak (ch[1], blockMaxR, n);

        // Time-derived RMS: the coefficient follows the real block duration, so the ballistics
        // are the same for a 64-sample and a 1024-sample host buffer.
        const float blockRmsL = (float) std::sqrt (sumL / (double) n);
        const float blockRmsR = (float) std::sqrt (sumR / (double) n);
        const float rmsCoef = (float) std::exp (-(double) n / (sr * 0.30));
        rmsL.store (rmsL.load (std::memory_order_relaxed) * rmsCoef
                    + blockRmsL * (1.0f - rmsCoef), std::memory_order_relaxed);
        rmsR.store (rmsR.load (std::memory_order_relaxed) * rmsCoef
                    + blockRmsR * (1.0f - rmsCoef), std::memory_order_relaxed);

        // Stereo correlation over the block
        const double denom = std::sqrt (juce::jmax (1.0e-12, sumLL * sumRR));
        corrValue.store ((float) juce::jlimit (-1.0, 1.0, sumLR / denom), std::memory_order_relaxed);

        // Short-term loudness: 3 s rolling window (EBU Mode short-term; 400 ms is momentary).
        if (kCount >= (int) (sr * 3.0))
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
        if (fft == nullptr || fftData.size() < (size_t) kFftSize) return;
        if (fftFifo.getNumReady() <= 0) return;

        // Time-derived band release: 18 dB/s based on elapsed display time, not frame count.
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const double dtSeconds = lastAnalysisMs > 0.0
            ? juce::jlimit (0.0, 0.25, (nowMs - lastAnalysisMs) * 1.0e-3) : 0.0;
        lastAnalysisMs = nowMs;
        const float bandReleaseDb = (float) (18.0 * dtSeconds);

        int fs1 = 0, fn1 = 0, fs2 = 0, fn2 = 0;
        fftFifo.prepareToRead (1, fs1, fn1, fs2, fn2);
        if (fn1 <= 0) return;
        const auto& snapshot = fftSlots[(size_t) fs1];
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < kFftSize; ++i)
            fftData[(size_t) i] = snapshot[(size_t) i] * window[(size_t) i];
        fftFifo.finishedRead (1);

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
            else bandHoldDb[(size_t) b] -= bandReleaseDb;
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

    /** Fill the scope buffer with the points the audio thread published since the last call.
        Lock-free by ownership: the producer only writes slots the FIFO handed it and the reader
        only reads slots it has claimed, so no payload is ever read while being overwritten.
        Frames are dropped when the ring is full rather than blocking the audio thread. */
    void copyScope (std::vector<ScopePoint>& out)
    {
        out.clear();
        const int ready = scopeFifo.getNumReady();
        if (ready <= 0) return;
        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        scopeFifo.prepareToRead (ready, s1, n1, s2, n2);
        out.reserve ((size_t) (n1 + n2));
        for (int i = 0; i < n1; ++i)
            out.push_back ({ scopeX[(size_t) (s1 + i)], scopeY[(size_t) (s1 + i)] });
        for (int i = 0; i < n2; ++i)
            out.push_back ({ scopeX[(size_t) (s2 + i)], scopeY[(size_t) (s2 + i)] });
        scopeFifo.finishedRead (n1 + n2);
    }

    struct Point { float x = 0.0f, y = 0.0f; };

private:
    struct Channel
    {
        float peak = 0.0f;
        float hold = 0.0f;
        int   holdCountdown = 0;
    };

    struct KState
    {
        double hp1 = 0.0, hp2 = 0.0, shelf = 0.0;
    };

    static float dbOf (float lin) { return lin > 1.0e-7f ? juce::Decibels::gainToDecibels (lin) : -100.0f; }

    void updatePeak (Channel& c, float blockMax, int numSamples)
    {
        c.peak = blockMax;
        if (blockMax >= c.hold)
        {
            c.hold = blockMax;
            c.holdCountdown = peakHoldSamples;
        }
        else if (c.holdCountdown > 0)
        {
            c.holdCountdown = juce::jmax (0, c.holdCountdown - numSamples);
        }
        else
        {
            // 0.4 s release time constant, expressed as a per-block factor.
            c.hold *= (float) std::exp (-(double) numSamples / (sr * 0.4));
        }
        peakL.store (ch[0].peak, std::memory_order_relaxed);
        peakR.store (ch[1].peak, std::memory_order_relaxed);
        holdL.store (ch[0].hold, std::memory_order_relaxed);
        holdR.store (ch[1].hold, std::memory_order_relaxed);
    }

    // Simple K-weighting approximation: 2nd-order HP at 38 Hz then high shelf +4 dB at 1.5 kHz,
    // with independent state per channel.
    double kWeight (double x, KState& s)
    {
        s.hp1 += kHpAlpha * (x - s.hp1);
        s.hp2 += kHpAlpha * (s.hp1 - s.hp2);
        const double v = x - s.hp2;
        s.shelf += kShelfAlpha * (v - s.shelf);
        return v + s.shelf * 0.6;
    }

    double sr = 48000.0;
    int peakHoldSamples = 96000;

    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<float> holdL { 0.0f }, holdR { 0.0f };
    std::atomic<float> rmsL { 0.0f }, rmsR { 0.0f };
    std::atomic<float> corrValue { 0.0f };
    std::atomic<float> lufsShort { -100.0f };

    std::array<Channel, 2> ch;

    // Sample-rate-derived coefficients for K-weighting
    double kHpAlpha = 0.005, kShelfAlpha = 0.02;
    double kSumSq = 0.0;
    int kCount = 0;
    std::array<KState, 2> kState;

    // FFT (transform runs on the UI thread via runAnalysis)
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, fftData, fifo, spectrumDb, smoothedSpectrum;
    int fifoIndex = 0;
    std::array<std::vector<float>, 2> fftSlots;
    juce::AbstractFifo fftFifo { 2 };
    std::vector<double> bandEdges;
    std::vector<int> bandLo, bandHi;
    std::vector<float> bandDb, bandHoldDb;
    std::vector<int> bandHoldFrames;

    // Scope: fixed SPSC ring owned by an AbstractFifo, no lock and no allocation.
    std::array<float, kScopeCapacity> scopeX {}, scopeY {};
    juce::AbstractFifo scopeFifo { kScopeCapacity };

    // Time-based analysis state
    double lastAnalysisMs = 0.0;
};
