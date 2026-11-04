#pragma once
#include <memory>
namespace rack {
// Global order is ALGO=0, NAM=1, CLEAN=2. Module bypass forces the clean branch.
class ColourEngine {
public:
    ColourEngine();
    ~ColourEngine();
    void prepare(double sampleRate, int model, int initialMode);
    void reset() noexcept;
    int latency() const noexcept;
    float delayedClean(int channel) const noexcept;
    void frame(const float* clean, const float* algo, float* output, int channels,
               int mode, float namAmount = 1.0f, float namDrive = 1.0f) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
