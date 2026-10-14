#pragma once

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace dommy {
namespace vdb {

// The legacy node evaluates octaves + 1 samples. This is an execution budget,
// independent of libnoise's internal octave count (which remains one).
const int kMaxNoiseOctaves = 32;

struct NoiseSettings {
    int mMaskMode = 0;
    float mOffset = 0.0f, mThreshold = 0.0f, mFallOff = 0.0f;
    std::array<double, 3> mNOffset{{0.0, 0.0, 0.0}};
    float mAmp = 1.0f, mGain = 1.0f;
    int mNoiseMode = 0, mOctaves = 1;
    float mFreq = 2.0f, mLacunarity = 2.0f, mRoughness = 1.0f;
};

inline bool validNoiseSettings(const NoiseSettings& s, bool usesMask) {
    return std::isfinite(s.mOffset) && std::isfinite(s.mAmp) && std::isfinite(s.mGain) &&
           std::isfinite(s.mFreq) && s.mFreq >= 0.0f && std::isfinite(s.mLacunarity) &&
           s.mLacunarity >= 0.0f && std::isfinite(s.mRoughness) && s.mRoughness >= 0.0f &&
           std::isfinite(s.mNOffset[0]) && std::isfinite(s.mNOffset[1]) &&
           std::isfinite(s.mNOffset[2]) && s.mNoiseMode >= 0 && s.mNoiseMode <= 2 &&
           s.mOctaves >= 1 && s.mOctaves <= kMaxNoiseOctaves &&
           (!usesMask || (s.mMaskMode >= 0 && s.mMaskMode <= 3 && std::isfinite(s.mThreshold) &&
                          s.mThreshold >= 0.0f && std::isfinite(s.mFallOff) && s.mFallOff >= 0.0f));
}

inline bool addNoiseWork(std::uint64_t activeVoxels, int octaves, std::size_t& work) {
    if (octaves < 1 || octaves > kMaxNoiseOctaves || activeVoxels > generation::kMaxWorkItemCount)
        return false;
    std::size_t samples = 0U;
    // Active tiles count conservatively as their covered voxels. The bound is
    // across all processed grids, not a sparse-tree memory or wall-clock bound.
    return generation::checkedProduct(static_cast<std::size_t>(activeVoxels),
                                      static_cast<std::size_t>(octaves) + 1U,
                                      generation::kMaxWorkItemCount, samples) &&
           generation::checkedSum(work, samples, generation::kMaxWorkItemCount, work);
}

inline bool noiseStencilCoordinate(int coordinate) {
    return coordinate > std::numeric_limits<int>::min() &&
           coordinate < std::numeric_limits<int>::max();
}

inline bool noiseMaskCoordinate(double value, bool needsStencil, int& coordinate) {
    // BoxSampler uses floor(index) and its +1 neighbor. A second-order stencil
    // additionally uses trunc(index) +/- 1. Retain historical truncation.
    if (!std::isfinite(value))
        return false;
    const double floorValue = std::floor(value);
    if (floorValue < std::numeric_limits<int>::min() ||
        floorValue >= std::numeric_limits<int>::max() ||
        !numeric::checkedNumericCast(value, coordinate))
        return false;
    return !needsStencil || noiseStencilCoordinate(coordinate);
}

inline bool noiseMaskWeight(double sample, const NoiseSettings& s, double alignment,
                            double& weight) {
    if (!std::isfinite(sample) || !std::isfinite(alignment) || !std::isfinite(s.mThreshold) ||
        !std::isfinite(s.mFallOff) || s.mFallOff < 0.0f || s.mMaskMode < 0 || s.mMaskMode > 3)
        return false;
    if (s.mMaskMode == 3) {
        weight = 1.0;
        return true;
    }
    const double distance = s.mMaskMode == 0 ? sample - s.mThreshold : s.mThreshold - sample;
    if (!std::isfinite(distance))
        return false;
    if (distance < 0.0) {
        // In mode 2 only aligned normals suppress noise above the threshold.
        // The old path produced a negative weight (or divided by zero) here.
        weight = s.mMaskMode == 2 && alignment <= 0.9 ? 1.0 : 0.0;
    } else if (s.mFallOff == 0.0f || distance >= s.mFallOff) {
        weight = 1.0;
    } else {
        weight = distance / s.mFallOff;
    }
    return std::isfinite(weight) && weight >= 0.0 && weight <= 1.0;
}

template <typename Generator>
bool calculateVdbNoise(std::array<double, 3> point, const Generator& generator,
                       const NoiseSettings& s, double frequencyMultiplier, double& output) {
    if (!validNoiseSettings(s, false) || !std::isfinite(frequencyMultiplier))
        return false;
    double amplitude = s.mAmp;
    double frequency = double(s.mFreq) * frequencyMultiplier;
    double result = 0.0;
    if (!std::isfinite(frequency))
        return false;
    for (int octave = 0; octave <= s.mOctaves; ++octave) {
        for (double& component : point) {
            component *= frequency;
            // Perlin applies its configured frequency once again. Keep that
            // historical appearance, but reject overflow before calling it.
            if (!std::isfinite(component) || !std::isfinite(component * double(s.mFreq)))
                return false;
        }
        double signal = 2.0 * generator.GetValue(point[0], point[1], point[2]) - 1.0;
        if (!std::isfinite(signal))
            return false;
        if (s.mNoiseMode > 0)
            signal = std::pow(std::abs(signal), double(s.mGain));
        if (!std::isfinite(signal))
            return false;
        const double contribution = signal * amplitude;
        if (!std::isfinite(contribution) || !std::isfinite(result + contribution))
            return false;
        result += contribution;
        if (octave != s.mOctaves) {
            frequency = s.mLacunarity;
            amplitude *= s.mRoughness;
            if (!std::isfinite(amplitude))
                return false;
        }
    }
    output = s.mNoiseMode == 1 ? -result : result;
    return true;
}

template <typename Value>
bool noiseOutputValue(Value source, double noise, double weight, float offset, Value& output) {
    if (!std::isfinite(source) || !std::isfinite(noise) || !std::isfinite(weight) ||
        !std::isfinite(offset) || weight < 0.0 || weight > 1.0)
        return false;
    return numeric::checkedNumericCast(static_cast<long double>(source) +
                                           static_cast<long double>(weight) *
                                               (static_cast<long double>(noise) - offset),
                                       output);
}

} // namespace vdb
} // namespace dommy
