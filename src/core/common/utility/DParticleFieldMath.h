#pragma once

#include "DForceMath.h"
#include "DProductSum.h"
#include "DScaledProduct.h"

#include <cmath>
#include <cstddef>

namespace dommy {
namespace field {

inline bool hasMatchingParticleFieldCounts(std::size_t pointCount,
                                           std::size_t positionCount,
                                           std::size_t velocityCount,
                                           std::size_t inverseMassCount)
{
    return pointCount == positionCount &&
           pointCount == velocityCount &&
           pointCount == inverseMassCount;
}

inline bool isValidOptionalFieldArrayCount(std::size_t pointCount,
                                           std::size_t valueCount)
{
    return valueCount == 0U || valueCount == pointCount;
}

inline bool isValidOptionalFieldMassCount(std::size_t pointCount,
                                          std::size_t massCount)
{
    return massCount == 0U || massCount == 1U || massCount == pointCount;
}

inline bool isValidFieldMass(double mass)
{
    return std::isfinite(mass) && mass >= 0.0;
}

inline bool checkedMultiplyFieldValue(double lhs,
                                      double rhs,
                                      double& result)
{
    return numeric::scaledProduct(std::array<double, 2>{{lhs, rhs}}, result);
}

inline bool checkedAccumulateFieldForce(double accumulated,
                                        double additional,
                                        double& result)
{
    return numeric::checkedProductSum(std::array<double, 2>{{accumulated, additional}},
                                      std::array<double, 2>{{1.0, 1.0}}, result);
}

// Preserve the historical DPFluidVectorField response while making each
// final result reject NaN, infinity, and unrepresentable output.
inline bool computeVectorFieldForceComponent(double velocity,
                                             double attributeValue,
                                             double magnitude,
                                             double& result)
{
    return numeric::checkedProductSum(std::array<double, 2>{{velocity, attributeValue}},
                                      std::array<double, 2>{{1.0, magnitude}}, result);
}

// Preserve the historical DPFluidNoiseField response while making the
// chained multiplication fail transactionally on invalid values.
inline bool computeNoiseFieldForceComponent(double velocity,
                                            double noise,
                                            double magnitude,
                                            double mass,
                                            double& result)
{
    if (!isValidFieldMass(mass)) return false;

    return numeric::scaledProduct(std::array<double, 4>{{velocity, noise, magnitude, mass}}, result);
}

inline bool computeGoalFieldForceComponent(double receptorPosition,
                                           double targetPosition,
                                           double velocity,
                                           double springConstant,
                                           double damping,
                                           double scale,
                                           double& result)
{
    return numeric::checkedSpringDampingResponse(receptorPosition, targetPosition,
        velocity, springConstant, damping, scale, 1.0, result);
}

inline bool computeGoalDistanceScale(double distance,
                                     double maximumDistance,
                                     double attenuation,
                                     double magnitude,
                                     double& result)
{
    if (!std::isfinite(distance) || distance < 0.0 ||
        !std::isfinite(maximumDistance) || maximumDistance <= 1.0e-12 ||
        !std::isfinite(attenuation) || !std::isfinite(magnitude)) {
        return false;
    }
    if (distance > maximumDistance) {
        result = 0.0;
        return true;
    }

    double attenuationScale = 1.0;
    if (attenuation > 0.0) {
        const double normalizedDistance = distance / maximumDistance;
        attenuationScale = std::pow(1.0 - normalizedDistance, attenuation);
        if (!std::isfinite(attenuationScale)) return false;
    }
    return checkedMultiplyFieldValue(
        magnitude, attenuationScale, result);
}

inline bool fieldMassFromInverseMass(double inverseMass, double& mass)
{
    if (!std::isfinite(inverseMass) || inverseMass < 0.0) {
        return false;
    }

    // An inverse mass of zero represents a fixed particle. MFnField requires
    // a finite mass value, so use a neutral placeholder and discard the
    // resulting force when velocity is integrated.
    const double candidate = inverseMass == 0.0 ? 1.0 : 1.0 / inverseMass;
    if (!std::isfinite(candidate) || candidate <= 0.0) {
        return false;
    }

    mass = candidate;
    return true;
}

inline bool isValidFieldTimeStep(double deltaTime)
{
    return std::isfinite(deltaTime) && deltaTime > 0.0;
}

inline bool integrateFieldVelocity(double velocity,
                                   double force,
                                   double inverseMass,
                                   double deltaTime,
                                   double& result)
{
    if (!std::isfinite(velocity) || !std::isfinite(force) ||
        !std::isfinite(inverseMass) || inverseMass < 0.0 ||
        !isValidFieldTimeStep(deltaTime)) {
        return false;
    }

    return numeric::checkedIntegratedVelocity(velocity, force, inverseMass, deltaTime, result);
}

} // namespace field
} // namespace dommy
