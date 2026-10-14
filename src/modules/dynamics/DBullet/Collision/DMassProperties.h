#pragma once

#include "../mvl/mathUtils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace bullet {

struct MassProperties {
    float volume;
    vec3f center;
    mat3x3f inertia;

    MassProperties()
        : volume(0.0f), center(0.0f, 0.0f, 0.0f),
          inertia(0.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 0.0f) {}
};

inline bool isFiniteVector(const vec3f& value) {
    return std::isfinite(value[0]) &&
           std::isfinite(value[1]) &&
           std::isfinite(value[2]);
}

inline bool computeMassProperties(const vec3f* vertices,
                                  std::size_t vertexCount,
                                  const unsigned int* indices,
                                  std::size_t indexCount,
                                  MassProperties& output)
{
    if (!vertices || !indices || vertexCount < 4U || indexCount < 12U ||
        indexCount % 3U != 0U) {
        return false;
    }

    typedef mvl::vec<double, 3> Vec3d;
    typedef mvl::mat<double, 3, 3> Mat3d;

    double minimum[3] = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()
    };
    double maximum[3] = {
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max()
    };
    for (std::size_t index = 0; index < vertexCount; ++index) {
        if (!isFiniteVector(vertices[index])) {
            return false;
        }
        for (int axis = 0; axis < 3; ++axis) {
            const double coordinate = static_cast<double>(vertices[index][axis]);
            minimum[axis] = std::min(minimum[axis], coordinate);
            maximum[axis] = std::max(maximum[axis], coordinate);
        }
    }

    const Vec3d reference(vertices[0][0], vertices[0][1], vertices[0][2]);
    double signedSixVolume = 0.0;
    Vec3d centerAccumulator(0.0, 0.0, 0.0);
    for (std::size_t index = 0; index < indexCount; index += 3U) {
        const unsigned int a = indices[index];
        const unsigned int b = indices[index + 1U];
        const unsigned int c = indices[index + 2U];
        if (a >= vertexCount || b >= vertexCount || c >= vertexCount) {
            return false;
        }

        const Vec3d v0 = Vec3d(vertices[a][0], vertices[a][1], vertices[a][2]) - reference;
        const Vec3d v1 = Vec3d(vertices[b][0], vertices[b][1], vertices[b][2]) - reference;
        const Vec3d v2 = Vec3d(vertices[c][0], vertices[c][1], vertices[c][2]) - reference;
        const double tetraVolume = determinant(v0, v1, v2);
        signedSixVolume += tetraVolume;
        centerAccumulator += tetraVolume * (v0 + v1 + v2);
    }

    const double scale = std::max(
        std::max(maximum[0] - minimum[0], maximum[1] - minimum[1]),
        std::max(maximum[2] - minimum[2], 1.0));
    const double volumeTolerance = scale * scale * scale * 1.0e-12;
    if (!std::isfinite(signedSixVolume) ||
        std::abs(signedSixVolume) <= volumeTolerance) {
        return false;
    }

    const Vec3d center = reference + centerAccumulator / (signedSixVolume * 4.0);
    if (!std::isfinite(center[0]) || !std::isfinite(center[1]) ||
        !std::isfinite(center[2])) {
        return false;
    }

    Vec3d diagonal(0.0, 0.0, 0.0);
    Vec3d offDiagonal(0.0, 0.0, 0.0);
    double inertiaVolume = 0.0;
    for (std::size_t index = 0; index < indexCount; index += 3U) {
        const unsigned int a = indices[index];
        const unsigned int b = indices[index + 1U];
        const unsigned int c = indices[index + 2U];
        const Vec3d v0(vertices[a][0], vertices[a][1], vertices[a][2]);
        const Vec3d v1(vertices[b][0], vertices[b][1], vertices[b][2]);
        const Vec3d v2(vertices[c][0], vertices[c][1], vertices[c][2]);
        const Mat3d matrix(
            v0[0] - center[0], v0[1] - center[1], v0[2] - center[2],
            v1[0] - center[0], v1[1] - center[1], v1[2] - center[2],
            v2[0] - center[0], v2[1] - center[1], v2[2] - center[2]);
        const double determinantValue = determinant(
            Vec3d(matrix(0, 0), matrix(1, 0), matrix(2, 0)),
            Vec3d(matrix(0, 1), matrix(1, 1), matrix(2, 1)),
            Vec3d(matrix(0, 2), matrix(1, 2), matrix(2, 2)));
        inertiaVolume += determinantValue;

        for (int axis = 0; axis < 3; ++axis) {
            const int next = (axis + 1) % 3;
            const int last = (axis + 2) % 3;
            diagonal[axis] +=
                (matrix(0, axis) * matrix(1, axis) +
                 matrix(1, axis) * matrix(2, axis) +
                 matrix(2, axis) * matrix(0, axis) +
                 matrix(0, axis) * matrix(0, axis) +
                 matrix(1, axis) * matrix(1, axis) +
                 matrix(2, axis) * matrix(2, axis)) * determinantValue;
            offDiagonal[axis] +=
                (matrix(0, next) * matrix(1, last) +
                 matrix(1, next) * matrix(2, last) +
                 matrix(2, next) * matrix(0, last) +
                 matrix(0, next) * matrix(2, last) +
                 matrix(1, next) * matrix(0, last) +
                 matrix(2, next) * matrix(1, last) +
                 2.0 * matrix(0, next) * matrix(0, last) +
                 2.0 * matrix(1, next) * matrix(1, last) +
                 2.0 * matrix(2, next) * matrix(2, last)) * determinantValue;
        }
    }

    if (!std::isfinite(inertiaVolume) ||
        std::abs(inertiaVolume) <= volumeTolerance) {
        return false;
    }
    diagonal /= inertiaVolume * 10.0;
    offDiagonal /= inertiaVolume * 20.0;

    const double values[9] = {
        diagonal[1] + diagonal[2], -offDiagonal[2], -offDiagonal[1],
        -offDiagonal[2], diagonal[0] + diagonal[2], -offDiagonal[0],
        -offDiagonal[1], -offDiagonal[0], diagonal[0] + diagonal[1]
    };
    for (int index = 0; index < 9; ++index) {
        if (!std::isfinite(values[index]) ||
            std::abs(values[index]) > std::numeric_limits<float>::max()) {
            return false;
        }
    }

    const double physicalVolume = std::abs(signedSixVolume) / 6.0;
    if (!std::isfinite(physicalVolume) ||
        physicalVolume > std::numeric_limits<float>::max()) {
        return false;
    }

    MassProperties properties;
    properties.volume = static_cast<float>(physicalVolume);
    properties.center = vec3f(
        static_cast<float>(center[0]),
        static_cast<float>(center[1]),
        static_cast<float>(center[2]));
    properties.inertia = mat3x3f(
        static_cast<float>(values[0]), static_cast<float>(values[1]),
        static_cast<float>(values[2]), static_cast<float>(values[3]),
        static_cast<float>(values[4]), static_cast<float>(values[5]),
        static_cast<float>(values[6]), static_cast<float>(values[7]),
        static_cast<float>(values[8]));
    output = properties;
    return true;
}

inline bool computeScaledMassProperties(
    const std::vector<vec3f>& vertices,
    const std::vector<unsigned int>& indices,
    const vec3f& scale,
    MassProperties& output)
{
    if (vertices.empty() || indices.empty() || !isFiniteVector(scale)) {
        return false;
    }

    std::vector<vec3f> scaledVertices(vertices.size());
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        scaledVertices[index] = vec3f(
            scale[0] * vertices[index][0],
            scale[1] * vertices[index][1],
            scale[2] * vertices[index][2]);
    }
    return computeMassProperties(
        &scaledVertices[0], scaledVertices.size(), &indices[0], indices.size(),
        output);
}

} // namespace bullet
} // namespace dommy
