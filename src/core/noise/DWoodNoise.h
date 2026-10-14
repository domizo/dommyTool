#pragma once

#include "noise/PerlinNoise.h"
#include "common/utility/DScaledProduct.h"

#include <modulebase.h>
#include <array>
#include <cmath>
#include <cstdint>

namespace dommy {
namespace noise {

struct WoodSettings {
    std::uint32_t seed = 0U;
    int warpOctaves = 1;
    double ringFrequency = 1.0;
    double grainStretch = 0.0;
    double grainAmount = 1.0;
    double bias = 0.0;
    double warpFrequency = 1.0;
    double warpAmount = 0.0;
    std::array<double, 3> rotationRadians = {{0.0, 0.0, 0.0}};
};

// Project-owned radial-ring field with axial grain and a seeded domain warp.
// The libnoise base is an interface adapter for existing noise workers; no
// libnoise texture-example module graph or reference-code implementation is used.
class WoodNoise final : public ::noise::module::Module {
public:
    explicit WoodNoise(const WoodSettings& settings)
        : ::noise::module::Module(0), settings_(settings),
          grain_(3, settings.seed ^ 0x53A9B4D1U, 0.5, 2.0),
          warpX_(settings.warpOctaves, settings.seed ^ 0xA61D093BU, 0.5, 2.0),
          warpY_(settings.warpOctaves, settings.seed ^ 0x7C205E49U, 0.5, 2.0),
          warpZ_(settings.warpOctaves, settings.seed ^ 0x1BF638A7U, 0.5, 2.0)
    {
        if (settings.warpOctaves < 1 || settings.warpOctaves > ::noise::kMaxPerlinOctaves ||
            !std::isfinite(settings.ringFrequency) || !std::isfinite(settings.grainStretch) ||
            !std::isfinite(settings.grainAmount) || !std::isfinite(settings.bias) ||
            !std::isfinite(settings.warpFrequency) || settings.warpFrequency < 0.0 ||
            !std::isfinite(settings.warpAmount)) throw ::noise::ExceptionInvalidParam();
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            const double angle = settings.rotationRadians[axis];
            if (!std::isfinite(angle)) throw ::noise::ExceptionInvalidParam();
            rotation_[axis] = {{std::cos(angle), std::sin(angle)}};
        }
    }

    int GetSourceModuleCount() const override { return 0; }

    double GetValue(double x, double y, double z) const override
    {
        std::array<double, 3> point = {{x, y, z}};
        for (double value : point)
            if (!std::isfinite(value)) throw ::noise::ExceptionInvalidParam();

        // Right-handed rotations about X, then Y, then Z. Rotation changes
        // the grain's axis, not the caller's point storage or coordinate frame.
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            const unsigned int first = (axis + 1U) % 3U;
            const unsigned int second = (axis + 2U) % 3U;
            const double a = point[first], b = point[second];
            point[first] = a * rotation_[axis][0] - b * rotation_[axis][1];
            point[second] = a * rotation_[axis][1] + b * rotation_[axis][0];
        }

        if (settings_.warpAmount != 0.0) {
            std::array<double, 3> coordinate;
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                coordinate[axis] = product(point[axis], settings_.warpFrequency);
            double warp[3];
            if (!warpX_.tryNoise(coordinate[0], coordinate[1], coordinate[2], warp[0]) ||
                !warpY_.tryNoise(coordinate[0], coordinate[1], coordinate[2], warp[1]) ||
                !warpZ_.tryNoise(coordinate[0], coordinate[1], coordinate[2], warp[2]))
                throw ::noise::ExceptionInvalidParam();
            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                point[axis] += product(settings_.warpAmount, warp[axis]);
                if (!std::isfinite(point[axis])) throw ::noise::ExceptionInvalidParam();
            }
        }

        const double radialCycles = product(std::hypot(point[0], point[2]), settings_.ringFrequency);
        const double rings = std::cos(std::remainder(radialCycles, 1.0) * (2.0 * std::acos(-1.0)));
        double grain = 0.0;
        if (settings_.grainAmount != 0.0 && !grain_.tryNoise(
                product(point[0], 8.0), product(point[1], settings_.grainStretch),
                product(point[2], 8.0), grain)) throw ::noise::ExceptionInvalidParam();
        const double result = rings + product(settings_.grainAmount, grain) + settings_.bias;
        if (!std::isfinite(result)) throw ::noise::ExceptionInvalidParam();
        return result;
    }

private:
    WoodNoise(const WoodNoise&) = delete;
    WoodNoise& operator=(const WoodNoise&) = delete;

    static double product(double first, double second)
    {
        double result = 0.0;
        if (!dommy::numeric::scaledProduct(std::array<double, 2>{{first, second}}, result))
            throw ::noise::ExceptionInvalidParam();
        return result;
    }

    WoodSettings settings_;
    std::array<std::array<double, 2>, 3> rotation_;
    ::noise::PerlinOctave grain_, warpX_, warpY_, warpZ_;
};

} // namespace noise
} // namespace dommy
