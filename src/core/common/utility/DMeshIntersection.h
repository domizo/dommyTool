#pragma once

#include "DCurveEvaluation.h"
#include "DMayaCheckedArray.h"
#include "DNumericValidation.h"

namespace dommy {
namespace maya {

template <typename Mesh>
class MeshAcceleratorGuard final {
public:
    explicit MeshAcceleratorGuard(Mesh& mesh) : mesh_(mesh), active_(true) {}
    ~MeshAcceleratorGuard() { if (active_) mesh_.freeCachedIntersectionAccelerator(); }
    MStatus release() {
        if (!active_) return MS::kSuccess;
        active_ = false;
        return mesh_.freeCachedIntersectionAccelerator();
    }
    MeshAcceleratorGuard(const MeshAcceleratorGuard&) = delete;
    MeshAcceleratorGuard& operator=(const MeshAcceleratorGuard&) = delete;
private:
    Mesh& mesh_;
    bool active_;
};

template <typename Point, typename Vector, typename FloatPoint, typename FloatVector>
bool makeFloatRay(const Point& point, const Vector& direction,
                  FloatPoint& source, FloatVector& ray)
{
    Vector normalized(direction);
    if (!curve::normalizeDirection(normalized)) return false;
    FloatPoint candidateSource;
    FloatVector candidateRay;
    for (unsigned int axis = 0; axis < 3; ++axis) {
        float coordinate = 0.0f, component = 0.0f;
        if (!numeric::checkedFloatCast(double(point[axis]), coordinate) ||
            !numeric::checkedFloatCast(double(normalized[axis]), component)) return false;
        candidateSource[axis] = coordinate;
        candidateRay[axis] = component;
    }
    source = candidateSource;
    ray = candidateRay;
    return true;
}

// A successful query with no hit is not an API failure. Failed queries never
// publish partially populated arrays; every hit is checked before publication.
template <typename Mesh, typename Point, typename Vector, typename Accel,
          typename Space, typename PointArray>
MStatus checkedAllIntersections(Mesh& mesh, const Point& source, const Vector& ray,
                               Space space, Accel* accelerator, PointArray& output,
                               float maximumParameter = 9999.0f,
                               float tolerance = 1.0e-6f)
{
    if (!std::isfinite(maximumParameter) || maximumParameter <= 0.0f ||
        !std::isfinite(tolerance) || tolerance <= 0.0f) return MS::kInvalidParameter;
    MStatus status;
    PointArray candidate;
    const bool hit = mesh.allIntersections(source, ray, nullptr, nullptr, true,
        space, maximumParameter, false, accelerator, true, candidate,
        nullptr, nullptr, nullptr, nullptr, nullptr, tolerance, &status);
    if (status != MS::kSuccess) return status;
    if (hit != (candidate.length() != 0U) ||
        candidate.length() > generation::kMaxGeneratedPointCount) return MS::kFailure;
    for (unsigned int i = 0; i < candidate.length(); ++i)
        if (!curve::finiteTriple(candidate[i])) return MS::kInvalidParameter;
    return copyCheckedArray(candidate, output);
}

template <typename Mesh, typename Point, typename Vector, typename Accel, typename Space>
MStatus checkedClosestIntersection(Mesh& mesh, const Point& source, const Vector& ray,
                                   Space space, Accel* accelerator, bool& hit, Point& output)
{
    MStatus status;
    Point candidate;
    const bool found = mesh.closestIntersection(source, ray, nullptr, nullptr, true,
        space, 9999.0f, false, accelerator, candidate,
        nullptr, nullptr, nullptr, nullptr, nullptr, 1.0e-6f, &status);
    if (status != MS::kSuccess) return status;
    if (found && !curve::finiteTriple(candidate)) return MS::kInvalidParameter;
    if (found) output = candidate;
    hit = found;
    return MS::kSuccess;
}

template <typename Mesh, typename Point, typename Vector, typename Accel, typename Space>
MStatus checkedClosestPointAndNormal(Mesh& mesh, const Point& source, Space space,
                                    Accel* accelerator, Point& point, Vector& normal)
{
    if (!curve::finiteTriple(source)) return MS::kInvalidParameter;
    Point candidatePoint;
    Vector candidateNormal;
    const MStatus status = mesh.getClosestPointAndNormal(source, candidatePoint,
        candidateNormal, space, nullptr, accelerator);
    if (status != MS::kSuccess) return status;
    if (!curve::finiteTriple(candidatePoint) || !curve::normalizeDirection(candidateNormal))
        return MS::kInvalidParameter;
    point = candidatePoint;
    normal = candidateNormal;
    return MS::kSuccess;
}

} // namespace maya
} // namespace dommy
