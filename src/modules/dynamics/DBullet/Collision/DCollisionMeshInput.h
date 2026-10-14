#pragma once

#include "common/utility/DGenerationLimits.h"
#include "../SoftBody/DSoftBodyPreparation.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace bullet {

template<typename Vec3Type>
inline bool isFiniteVec3(const Vec3Type& value)
{
    return std::isfinite(static_cast<double>(value[0])) &&
           std::isfinite(static_cast<double>(value[1])) &&
           std::isfinite(static_cast<double>(value[2]));
}

template<typename QuaternionType>
inline bool hasValidQuaternion(const QuaternionType& value)
{
    double lengthSquared = 0.0;
    for (int component = 0; component < 4; ++component) {
        const double coordinate = static_cast<double>(value[component]);
        if (!std::isfinite(coordinate)) {
            return false;
        }
        lengthSquared += coordinate * coordinate;
    }
    return std::isfinite(lengthSquared) &&
           lengthSquared > std::numeric_limits<double>::epsilon();
}

inline bool hasValidSphereInput(float radius)
{
    return std::isfinite(static_cast<double>(radius)) && radius > 0.0f;
}

template<typename Vec3Type>
inline bool hasValidPlaneInput(const Vec3Type& normal, float distance)
{
    if (!isFiniteVec3(normal) || !std::isfinite(static_cast<double>(distance))) {
        return false;
    }
    const double lengthSquared =
        static_cast<double>(normal[0]) * normal[0] +
        static_cast<double>(normal[1]) * normal[1] +
        static_cast<double>(normal[2]) * normal[2];
    return std::isfinite(lengthSquared) &&
           lengthSquared > std::numeric_limits<double>::epsilon();
}

template<typename Vec3Type>
inline bool hasValidBoxInput(const Vec3Type& halfExtents)
{
    return isFiniteVec3(halfExtents) &&
           halfExtents[0] > 0 && halfExtents[1] > 0 && halfExtents[2] > 0;
}

/// Validate the raw mesh contract shared by Bullet collision-shape factories.
/// Vec3Type must provide indexed access to three numeric components.
template<typename Vec3Type>
inline bool hasValidCollisionMeshInput(const Vec3Type* vertices,
                                       std::size_t vertexCount,
                                       const Vec3Type* normals,
                                       const unsigned int* indices,
                                       std::size_t indexCount)
{
    if (!vertices || !normals || !indices || vertexCount < 3 || indexCount < 3 ||
        indexCount % 3 != 0 ||
        vertexCount > generation::kMaxGeneratedPointCount ||
        indexCount / 3U > generation::kMaxGeneratedFaceCount ||
        vertexCount > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        indexCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(static_cast<double>(vertices[vertex][axis])) ||
                !std::isfinite(static_cast<double>(normals[vertex][axis]))) {
                return false;
            }
        }
    }

    for (std::size_t index = 0; index < indexCount; index += 3U) {
        const unsigned int a = indices[index];
        const unsigned int b = indices[index + 1U];
        const unsigned int c = indices[index + 2U];
        if (static_cast<std::size_t>(a) >= vertexCount ||
            static_cast<std::size_t>(b) >= vertexCount ||
            static_cast<std::size_t>(c) >= vertexCount ||
            a == b || b == c || c == a) {
            return false;
        }
    }
    return true;
}

inline bool hasValidSoftBodyMeshInput(
    const std::vector<float>& coordinates,
    const std::vector<int>& indices)
{
    if (coordinates.size() < 9U || coordinates.size() % 3U != 0U ||
        indices.size() < 3U || indices.size() % 3U != 0U) {
        return false;
    }

    const std::size_t vertexCount = coordinates.size() / 3U;
    const std::size_t triangleCount = indices.size() / 3U;
    if (vertexCount > kMaxSoftBodyVertices ||
        triangleCount > kMaxSoftBodyFaces ||
        vertexCount > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        triangleCount > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    for (std::size_t coordinate = 0; coordinate < coordinates.size(); ++coordinate) {
        if (!std::isfinite(static_cast<double>(coordinates[coordinate]))) {
            return false;
        }
    }
    for (std::size_t index = 0; index < indices.size(); index += 3U) {
        const int a = indices[index];
        const int b = indices[index + 1U];
        const int c = indices[index + 2U];
        if (a < 0 || b < 0 || c < 0 ||
            static_cast<std::size_t>(a) >= vertexCount ||
            static_cast<std::size_t>(b) >= vertexCount ||
            static_cast<std::size_t>(c) >= vertexCount ||
            a == b || b == c || c == a) {
            return false;
        }
        double area;
        if (!softBodyTriangleArea(coordinates,a,b,c,area)) return false;
    }
    return true;
}

} // namespace bullet
} // namespace dommy
