#pragma once

#include "DMassProperties.h"
#include "../RigidBody/DBRigidBodyParameters.h"

namespace dommy {
namespace bullet {

inline bool validCapsuleDimensions(double radius, double height)
{
    return isSafeCollisionCoordinate(radius) && radius > 0.0 &&
        isSafeInverseOperand(static_cast<float>(radius)) &&
        static_cast<float>(radius) > 0.0f &&
        isSafeCollisionCoordinate(height) && height >= 0.0;
}

// Bullet's capsule height excludes its two hemispheres. A shorter segment
// would produce inverted bounds; reject it instead of silently changing shape.
template <typename Point>
inline bool ropeCapsuleHeight(const Point& source, const Point& target,
                              double radius, float& height)
{
    double delta[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (!isSafeCollisionCoordinate(source[axis]) ||
            !isSafeCollisionCoordinate(target[axis])) return false;
        delta[axis] = static_cast<double>(target[axis]) - source[axis];
    }
    const double length = std::hypot(std::hypot(delta[0], delta[1]), delta[2]);
    const double candidate = length - 2.0 * radius;
    if (!validCapsuleDimensions(radius, candidate)) return false;
    height = static_cast<float>(candidate);
    return true;
}

inline bool scaledCollisionPoint(const vec3f& point, const vec3f& scale,
                                 vec3f& output)
{
    vec3f candidate;
    for (int axis = 0; axis < 3; ++axis) {
        const double value = static_cast<double>(point[axis]) * scale[axis];
        if (!isSafeCollisionCoordinate(value)) return false;
        candidate[axis] = static_cast<float>(value);
    }
    output = candidate;
    return true;
}

inline bool prepareScaledCollisionVertices(const std::vector<vec3f>& vertices,
                                           const vec3f& scale,
                                           std::vector<vec3f>& output)
{
    if (vertices.empty() || !isFiniteVector(scale)) return false;
    std::vector<vec3f> candidate(vertices.size());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (!scaledCollisionPoint(vertices[i], scale, candidate[i])) return false;
    }
    output.swap(candidate);
    return true;
}

inline bool isSingularScale(const vec3f& scale)
{
    return scale[0] == 0.0f || scale[1] == 0.0f || scale[2] == 0.0f;
}

inline bool boxMassFromDimensions(const vec3f& dimensions, float& volume, vec3f& inertia)
{
    double d[3];
    for (int axis = 0; axis < 3; ++axis) {
        d[axis] = dimensions[axis];
        if (!isSafeCollisionCoordinate(d[axis]) || d[axis] < 0) return false;
    }
    const double v = d[0] * d[1] * d[2];
    const double values[3] = {(d[1]*d[1]+d[2]*d[2])/12,
                              (d[0]*d[0]+d[2]*d[2])/12,
                              (d[0]*d[0]+d[1]*d[1])/12};
    if (!std::isfinite(v) || v > std::numeric_limits<float>::max() ||
        (v != 0 && static_cast<float>(v) == 0)) return false;
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(values[axis]) || values[axis] > std::numeric_limits<float>::max() ||
            (v != 0 && values[axis] != 0 && static_cast<float>(values[axis]) == 0)) return false;
    }
    volume = static_cast<float>(v);
    inertia = v == 0 ? vec3f(0,0,0) : vec3f(values[0],values[1],values[2]);
    return true;
}

inline bool massScaledInertia(float mass, const vec3f& unitInertia, vec3f& output)
{
    if (!isSafeInverseOperand(mass)) return false;
    vec3f candidate;
    for (int axis = 0; axis < 3; ++axis) {
        const double value = static_cast<double>(mass) * unitInertia[axis];
        if (!std::isfinite(unitInertia[axis]) || unitInertia[axis] < 0 ||
            !std::isfinite(value) || value > std::numeric_limits<float>::max()) return false;
        candidate[axis] = static_cast<float>(value);
        if ((value != 0 && candidate[axis] == 0) ||
            !isSafeInverseOperand(candidate[axis])) return false;
    }
    output = candidate;
    return true;
}

inline bool safeBodyMassProperties(float mass, const vec3f& inertia)
{
    return isSafeInverseOperand(mass) && isSafeInverseOperand(inertia[0]) &&
           isSafeInverseOperand(inertia[1]) && isSafeInverseOperand(inertia[2]);
}

inline bool safeMassGravity(float mass, const vec3f& gravity)
{
    if (!isSafeInverseOperand(mass) || !isFiniteVector(gravity)) return false;
    if (mass == 0) return true;
    // setMassProps uses mass; setGravity reconstructs it from inverse mass.
    const float reconstructedMass = 1.0f / (1.0f / mass);
    for (int axis = 0; axis < 3; ++axis) {
        const double magnitude = std::abs(static_cast<double>(gravity[axis]));
        if (magnitude * mass > std::numeric_limits<float>::max() ||
            magnitude * reconstructedMass > std::numeric_limits<float>::max()) return false;
    }
    return true;
}

// The legacy body frame stays in input-mesh axes (no COM-frame shift for
// these shapes). Rotate the center-relative tensor back into those axes.
// Bullet's three-component interface retains only its diagonal; this does not
// introduce a full inertia tensor or change the historical body origin.
inline bool inertiaDiagonalInFrame(const mat3x3f& inertia,
                                   const quatf& frameRotation, vec3f& output)
{
    mat3x3f rotation;
    q_to_mat(frameRotation, rotation);
    vec3f candidate(0.0f, 0.0f, 0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        double value = 0.0;
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                value += static_cast<double>(rotation(axis, row)) *
                         inertia(row, column) * rotation(axis, column);
            }
        }
        if (!std::isfinite(value) || value < 0.0 ||
            value > std::numeric_limits<float>::max()) return false;
        candidate[axis] = static_cast<float>(value);
    }
    output = candidate;
    return true;
}

// Inverse-transpose of a diagonal scale. Use double before normalization to
// handle very small/large finite scales. Singular scales have no unique normal.
inline vec3f scaledDisplayNormal(const vec3f& normal, const vec3f& scale)
{
    double values[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(normal[axis]) || !std::isfinite(scale[axis]) ||
            scale[axis] == 0.0f) return vec3f(0.0f, 0.0f, 0.0f);
        values[axis] = static_cast<double>(normal[axis]) / scale[axis];
    }
    const double length = std::hypot(std::hypot(values[0], values[1]), values[2]);
    if (!std::isfinite(length) || length == 0.0) return vec3f(0.0f, 0.0f, 0.0f);
    return vec3f(static_cast<float>(values[0] / length),
                 static_cast<float>(values[1] / length),
                 static_cast<float>(values[2] / length));
}

} // namespace bullet
} // namespace dommy
