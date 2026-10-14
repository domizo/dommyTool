#pragma once

#include "modules/point/DPoint/math/DVectorMath.h"
#include <maya/MStatus.h>

namespace dommy {
namespace curve {

template <typename Value> bool finiteTriple(const Value& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

template <typename Vector> bool normalizeDirection(Vector& value)
{
    Vector candidate(value);
    double x = 0.0, y = 0.0, z = 0.0;
    if (!math::normalized3OrZero(double(value[0]), double(value[1]), double(value[2]),
            x, y, z, 1.0e-12)) return false;
    candidate[0] = x;
    candidate[1] = y;
    candidate[2] = z;
    value = candidate;
    return true;
}

// CVs need not lie on the curve. Never use getParamAtPoint on an arbitrary CV
// or supply an uninitialized parameter as a closestPoint initial guess.
template <typename Curve, typename Point, typename Space>
MStatus closestPointAndParameter(Curve& curve, const Point& point, double tolerance,
                                Space space, Point& closest, double& parameter)
{
    if (!finiteTriple(point) || !std::isfinite(tolerance) || tolerance <= 0.0)
        return MS::kInvalidParameter;
    MStatus status;
    double candidateParameter = 0.0;
    const Point candidate = curve.closestPoint(
        point, &candidateParameter, tolerance, space, &status);
    if (status != MS::kSuccess) return status;
    if (!finiteTriple(candidate) || !std::isfinite(candidateParameter))
        return MS::kInvalidParameter;
    closest = candidate;
    parameter = candidateParameter;
    return MS::kSuccess;
}

template <typename Curve, typename Vector, typename Space>
MStatus directionAtParameter(Curve& curve, double parameter, bool normal,
                             Space space, Vector& result)
{
    if (!std::isfinite(parameter)) return MS::kInvalidParameter;
    MStatus status;
    const Vector candidate = normal ? curve.normal(parameter, space, &status)
                                    : curve.tangent(parameter, space, &status);
    if (status != MS::kSuccess) return status;
    if (!finiteTriple(candidate)) return MS::kInvalidParameter;
    result = candidate;
    return MS::kSuccess;
}

template <typename Curve, typename Point, typename Vector, typename Space>
MStatus directionAtClosestPoint(Curve& curve, const Point& point, bool normal,
                               double tolerance, Space space, Vector& result)
{
    Point closest;
    double parameter = 0.0;
    const MStatus status = closestPointAndParameter(curve, point, tolerance, space, closest, parameter);
    if (status != MS::kSuccess) return status;
    return directionAtParameter(curve, parameter, normal, space, result);
}

// Preserve the historical transported frame: seed it with the curve normal,
// then derive later normals from the previous binormal and current tangent.
// All outputs stay unchanged when a query or normalization fails.
template <typename Curve, typename Point, typename Vector, typename Space>
MStatus frameAtClosestPoint(Curve& curve, const Point& point,
                           const Vector* previousBinormal, double tolerance, Space space,
                           Vector& tangent, Vector& normal, Vector& binormal)
{
    Point closest;
    double parameter = 0.0;
    MStatus status = closestPointAndParameter(curve, point, tolerance, space, closest, parameter);
    if (status != MS::kSuccess) return status;
    Vector candidateTangent = curve.tangent(parameter, space, &status);
    if (status != MS::kSuccess) return status;
    if (!normalizeDirection(candidateTangent)) return MS::kInvalidParameter;
    Vector candidateNormal;
    if (previousBinormal) {
        if (!finiteTriple(*previousBinormal)) return MS::kInvalidParameter;
        candidateNormal = *previousBinormal ^ candidateTangent;
    } else {
        candidateNormal = curve.normal(parameter, space, &status);
        if (status != MS::kSuccess) return status;
    }
    if (!normalizeDirection(candidateNormal)) return MS::kInvalidParameter;
    Vector candidateBinormal = candidateTangent ^ candidateNormal;
    if (!normalizeDirection(candidateBinormal)) return MS::kInvalidParameter;
    tangent = candidateTangent;
    normal = candidateNormal;
    binormal = candidateBinormal;
    return MS::kSuccess;
}

template <typename Curve, typename Point, typename Vector, typename Space>
MStatus sampleAtLength(Curve& curve, double distance, Space space,
                       Point& point, Vector& tangent)
{
    if (!std::isfinite(distance) || distance < 0.0) return MS::kInvalidParameter;
    MStatus status;
    const double parameter = curve.findParamFromLength(distance, &status);
    if (status != MS::kSuccess) return status;
    if (!std::isfinite(parameter)) return MS::kInvalidParameter;
    Point candidatePoint;
    status = curve.getPointAtParam(parameter, candidatePoint, space);
    if (status != MS::kSuccess) return status;
    Vector candidateTangent;
    status = directionAtParameter(curve, parameter, false, space, candidateTangent);
    if (status != MS::kSuccess) return status;
    if (!finiteTriple(candidatePoint) || !normalizeDirection(candidateTangent))
        return MS::kInvalidParameter;
    point = candidatePoint;
    tangent = candidateTangent;
    return MS::kSuccess;
}

} // namespace curve
} // namespace dommy
