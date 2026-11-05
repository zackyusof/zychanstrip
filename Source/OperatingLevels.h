#pragma once
#include <cmath>

namespace rack::operating
{
// UA Realtime Rack manual v8.6, p136: most titles -18 dBFS; LA-2A
// Collection and Studer A800 -12 dBFS. Adopt that convention for this rack's
// API/FET and opto/tape respectively. These are NOT the capture creators'
// unrecorded converter calibrations, nor a claim of hardware transfer accuracy.
inline constexpr float referenceDb (int model) noexcept
{
    return model == 1 || model == 3 ? -12.0f : -18.0f;
}
// Our shared circuit coordinate has nominal -18 dBFS. Use inverse trims around
// colour only: actual module gain still drives the next module in any rack order.
inline float inputScale (int model) noexcept
{
    return std::pow (10.0f, (-18.0f - referenceDb (model)) * 0.05f);
}
// Explicit measurement convention: sine RMS relative to full-scale amplitude 1.
inline float calibrationPeak (int model) noexcept
{
    return std::sqrt (2.0f) * std::pow (10.0f, referenceDb (model) * 0.05f);
}
}
