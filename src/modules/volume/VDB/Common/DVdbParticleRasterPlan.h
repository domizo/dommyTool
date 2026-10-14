#pragma once

#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "math/DBounds.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dommy {
namespace vdb {

struct ParticleRasterParameters {
    ParticleRasterParameters()
        : voxelSize(0.0),
          halfBandVoxels(0.0),
          radiusScale(0.0),
          minimumRadiusVoxels(0.0),
          maskWidth(0.0),
          velocityMultiplier(0.0),
          velocitySpacing(0.0),
          rasterizeTrails(false),
          rasterPassCount(1U)
    {
    }

    double voxelSize;
    double halfBandVoxels;
    double radiusScale;
    double minimumRadiusVoxels;
    double maskWidth;
    double velocityMultiplier;
    double velocitySpacing;
    bool rasterizeTrails;
    std::size_t rasterPassCount;
};

class ParticleInputExtrema {
public:
    ParticleInputExtrema() : mMaximumRadius(0.0), mMaximumSpeed(0.0) {}

    template <typename VectorType>
    bool addPosition(const VectorType& value)
    {
        return mPositions.add(value[0], value[1], value[2]);
    }

    bool addRadius(double value)
    {
        if (!std::isfinite(value) || value < 0.0) return false;
        mMaximumRadius = std::max(mMaximumRadius, value);
        return true;
    }

    template <typename VectorType>
    bool addVelocity(const VectorType& value)
    {
        double length = 0.0;
        if (!vectorMagnitude(value, length)) return false;
        mMaximumSpeed = std::max(mMaximumSpeed, length);
        return true;
    }

    double maximumRadius() const { return mMaximumRadius; }
    double maximumSpeed() const { return mMaximumSpeed; }
    const math::Bounds3d& positions() const { return mPositions; }

private:
    template <typename VectorType>
    static bool isFiniteVector(const VectorType& value)
    {
        return std::isfinite(static_cast<double>(value[0])) &&
               std::isfinite(static_cast<double>(value[1])) &&
               std::isfinite(static_cast<double>(value[2]));
    }

    template <typename VectorType>
    static bool vectorMagnitude(const VectorType& value, double& length)
    {
        if (!isFiniteVector(value)) return false;
        const double x = static_cast<double>(value[0]);
        const double y = static_cast<double>(value[1]);
        const double z = static_cast<double>(value[2]);
        const double squaredLength = x * x + y * y + z * z;
        if (!std::isfinite(squaredLength)) return false;
        length = std::sqrt(squaredLength);
        return std::isfinite(length);
    }

    double mMaximumRadius;
    double mMaximumSpeed;
    math::Bounds3d mPositions;
};

template <typename VectorType>
inline bool isFiniteVector3(const VectorType& value)
{
    return std::isfinite(static_cast<double>(value[0])) &&
           std::isfinite(static_cast<double>(value[1])) &&
           std::isfinite(static_cast<double>(value[2]));
}

template <typename VectorType>
inline bool vectorLength(const VectorType& value, double& length)
{
    if (!isFiniteVector3(value)) return false;
    const double x = static_cast<double>(value[0]);
    const double y = static_cast<double>(value[1]);
    const double z = static_cast<double>(value[2]);
    const double squaredLength = x * x + y * y + z * z;
    if (!std::isfinite(squaredLength)) return false;
    length = std::sqrt(squaredLength);
    return std::isfinite(length);
}

inline bool hasValidParticleRasterParameters(
    const ParticleRasterParameters& parameters)
{
    return std::isfinite(parameters.voxelSize) && parameters.voxelSize >= 1e-6 &&
           parameters.voxelSize <= 1e6 &&
           std::isfinite(parameters.halfBandVoxels) &&
               parameters.halfBandVoxels > 0.0 && parameters.halfBandVoxels <= 64.0 &&
           std::isfinite(parameters.radiusScale) && parameters.radiusScale > 0.0 &&
           std::isfinite(parameters.minimumRadiusVoxels) &&
               parameters.minimumRadiusVoxels >= 0.0 &&
           std::isfinite(parameters.maskWidth) && parameters.maskWidth >= 0.0 &&
           std::isfinite(parameters.velocityMultiplier) &&
               parameters.velocityMultiplier >= 0.0 &&
           std::isfinite(parameters.velocitySpacing) &&
               parameters.velocitySpacing > 0.0 &&
           parameters.rasterPassCount > 0U;
}

inline bool checkedParticleRasterWork(
    std::size_t pointCount,
    double maximumBaseRadius,
    double maximumSpeed,
    const ParticleRasterParameters& parameters,
    std::size_t maximumWorkItems,
    std::size_t& workItems)
{
    if (pointCount == 0U ||
        pointCount > generation::kMaxGeneratedPointCount ||
        !hasValidParticleRasterParameters(parameters) ||
        !std::isfinite(maximumBaseRadius) || maximumBaseRadius < 0.0 ||
        !std::isfinite(maximumSpeed) || maximumSpeed < 0.0) {
        return false;
    }

    const double maskScale = 1.0 + parameters.maskWidth;
    const double radiusVoxels =
        maximumBaseRadius * parameters.radiusScale * maskScale /
        parameters.voxelSize;
    const double trailVoxels = parameters.rasterizeTrails
        ? maximumSpeed * parameters.velocityMultiplier / parameters.voxelSize
        : 0.0;
    std::size_t trailSteps = 1U;
    if (parameters.rasterizeTrails) {
        // OpenVDB 5.2 uses float radii/distances but double positions. Its trail
        // loop has no iteration cap. Reserve rounding headroom for cancellation
        // in R0-(R0-Rmin)*d/speed, then require progress throughout the admitted
        // +/-1e6 index domain (enforced by planParticleConversion).
        float minimumRadius = 0.0f, spacing = 0.0f;
        if (!numeric::checkedNumericCast(parameters.minimumRadiusVoxels, minimumRadius) ||
            !numeric::checkedNumericCast(parameters.velocitySpacing, spacing) ||
            minimumRadius <= 0.0f || spacing <= 0.0f ||
            minimumRadius < 64.0 * std::numeric_limits<float>::epsilon() *
                                std::max(radiusVoxels, double(minimumRadius))) {
            return false;
        }
        // One eighth of spacing*Rmin leaves headroom for radius, direction,
        // coordinate-addition and distance rounding in the native loop.
        const double minimumProgress = 0.125 * double(spacing) * minimumRadius;
        const double roundingMargin =
            64.0 * std::numeric_limits<double>::epsilon() * 1e6;
        if (!std::isfinite(minimumProgress) || minimumProgress < roundingMargin ||
            !std::isfinite(trailVoxels)) return false;
        const double steps = std::ceil(trailVoxels / minimumProgress) + 2.0;
        if (!std::isfinite(steps) || steps > static_cast<double>(maximumWorkItems) ||
            !numeric::checkedNumericCast(steps, trailSteps)) return false;
    }
    const double halfExtent = std::ceil(
        std::max(radiusVoxels, parameters.minimumRadiusVoxels) +
        parameters.halfBandVoxels + trailVoxels);
    const double dimensionValue = 2.0 * halfExtent + 1.0;
    if (!std::isfinite(dimensionValue) || dimensionValue < 1.0 ||
        dimensionValue > static_cast<double>(maximumWorkItems)) {
        return false;
    }

    const std::size_t dimension = static_cast<std::size_t>(dimensionValue);
    std::size_t perParticle = 0U;
    std::size_t perTrail = 0U;
    std::size_t allParticles = 0U;
    std::size_t allPasses = 0U;
    if (!generation::checkedProduct(
            dimension, dimension, maximumWorkItems, perParticle) ||
        !generation::checkedProduct(
            perParticle, dimension, maximumWorkItems, perParticle) ||
        !generation::checkedProduct(
            perParticle, trailSteps, maximumWorkItems, perTrail) ||
        !generation::checkedProduct(
            perTrail, pointCount, maximumWorkItems, allParticles) ||
        !generation::checkedProduct(
            allParticles, parameters.rasterPassCount, maximumWorkItems,
            allPasses)) {
        return false;
    }

    workItems = allPasses;
    return true;
}

inline bool hasValidPointSamplingInput(std::size_t pointCount, double voxelSize)
{
    return pointCount > 0U &&
           pointCount <= generation::kMaxGeneratedPointCount &&
           std::isfinite(voxelSize) && voxelSize > 0.0;
}

inline bool isValidIndexCoordinate(double value)
{
    return std::isfinite(value) &&
           value >= static_cast<double>(std::numeric_limits<int>::min()) &&
           value <= static_cast<double>(std::numeric_limits<int>::max());
}

// Preserve the sampling node's historical float rounding, then truncation
// toward zero. Validate after rounding as well: INT_MAX can round to 2^31.
inline bool checkedSamplingCoordinate(double value, int& coordinate)
{
    float rounded = 0.0f;
    return isValidIndexCoordinate(value) &&
           numeric::checkedNumericCast(value, rounded) &&
           numeric::checkedNumericCast(rounded, coordinate);
}

} // namespace vdb
} // namespace dommy
