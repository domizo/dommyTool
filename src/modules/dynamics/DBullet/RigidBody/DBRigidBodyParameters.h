#pragma once

#include "common/utility/DNumericValidation.h"
#include <cmath>
#include <limits>

namespace dommy {
namespace bullet {

// Shared scalar admission for rigid and soft bodies. Keep it independent of
// vector/matrix headers so mesh-input and host-free checks stay lightweight.
// Bullet squares distances/cross products and transforms AABBs in btScalar;
// reserve headroom for three axes, margins and frame transforms.
inline bool isSafeCollisionCoordinate(double value)
{
    const double limit = std::sqrt(std::sqrt(
        static_cast<double>(std::numeric_limits<float>::max()))) / 64.0;
    return std::isfinite(value) && std::abs(value) <= limit;
}

// Both the operand and its reciprocal must remain normal floats. This also
// avoids platform-dependent flush-to-zero behavior in Bullet's inverse data.
// Explicit zero retains its static/locked-axis meaning.
inline bool isSafeInverseOperand(float value)
{
    if (!std::isfinite(value) || value < 0) return false;
    if (value == 0) return true;
    if (!std::isnormal(value)) return false;
    return std::isnormal(1.0f / value);
}

struct RigidBodyParameters
{
    RigidBodyParameters()
        : mass(0.0f),
          restitution(0.0f),
          friction(0.0f),
          linearDamping(0.0f),
          angularDamping(0.0f)
    {
    }

    float mass;
    float restitution;
    float friction;
    float linearDamping;
    float angularDamping;
};

inline bool validateRigidBodyParameters(
    double rawMass,
    double rawRestitution,
    double rawFriction,
    double rawLinearDamping,
    double rawAngularDamping,
    RigidBodyParameters& result)
{
    if (rawMass < 0.0 ||
        rawRestitution < 0.0 || rawRestitution > 1.0 ||
        rawFriction < 0.0 ||
        rawLinearDamping < 0.0 || rawLinearDamping > 1.0 ||
        rawAngularDamping < 0.0 || rawAngularDamping > 1.0) {
        return false;
    }

    RigidBodyParameters candidate;
    if (!numeric::checkedFloatCast(rawMass, candidate.mass) ||
        !numeric::checkedFloatCast(
            rawRestitution, candidate.restitution) ||
        !numeric::checkedFloatCast(rawFriction, candidate.friction) ||
        !numeric::checkedFloatCast(
            rawLinearDamping, candidate.linearDamping) ||
        !numeric::checkedFloatCast(
            rawAngularDamping, candidate.angularDamping)) {
        return false;
    }

    if ((rawMass != 0 && candidate.mass == 0) ||
        !isSafeInverseOperand(candidate.mass)) return false;
    result = candidate;
    return true;
}

} // namespace bullet
} // namespace dommy
