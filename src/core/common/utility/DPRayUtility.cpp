#include "DPRayUtility.h"

#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"

#include <maya/MDoubleArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVector.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>

namespace {

bool toDPoint(const MPoint& source, DPoint& destination)
{
    if (!dommy::maya::isFinitePoint(source)) {
        return false;
    }

    float values[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (!dommy::numeric::checkedFloatCast(source[axis], values[axis])) {
            return false;
        }
    }
    destination = DPoint(values[0], values[1], values[2]);
    return true;
}

bool toMPoint(const DPoint& source, MPoint& destination)
{
    destination = MPoint(source[0], source[1], source[2]);
    return dommy::maya::isFinitePoint(destination);
}

bool toFloatPoint(const MPoint& source, MFloatPoint& destination)
{
    if (!dommy::maya::isFinitePoint(source)) {
        return false;
    }

    float values[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (!dommy::numeric::checkedFloatCast(source[axis], values[axis])) {
            return false;
        }
    }
    destination = MFloatPoint(values[0], values[1], values[2]);
    return true;
}

bool toNormalizedFloatVector(
    const MVector& source, MFloatVector& destination)
{
    MVector normalized;
    if (!dommy::maya::normalizedVector(source, normalized)) {
        return false;
    }

    float values[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (!dommy::numeric::checkedFloatCast(normalized[axis], values[axis])) {
            return false;
        }
    }
    destination = MFloatVector(values[0], values[1], values[2]);
    return true;
}

MStatus applyHit(
    const DPRayParams& params,
    const MPoint& basePoint,
    const MPoint& hitPoint,
    const MVector* normal,
    DPoint& destination)
{
    if (!dommy::maya::isFinitePoint(basePoint) ||
        !dommy::maya::isFinitePoint(hitPoint)) {
        return MS::kInvalidParameter;
    }

    MPoint finalPoint = basePoint;
    if (params.mIsTransform) {
        finalPoint = params.mScale == 1.0f
            ? hitPoint
            : basePoint + (hitPoint - basePoint) * params.mScale;
    }

    if (params.mLift != 0.0f) {
        if (!normal || !dommy::maya::isFiniteVector(*normal)) {
            return MS::kFailure;
        }
        finalPoint += *normal * params.mLift;
    }

    return toDPoint(finalPoint, destination)
        ? MStatus(MS::kSuccess)
        : MStatus(MS::kInvalidParameter);
}

bool exceedsMaximumDistance(
    const DPRayParams& params,
    const MPoint& first,
    const MPoint& second,
    MStatus& status)
{
    const double distance = first.distanceTo(second);
    if (!std::isfinite(distance)) {
        status = MS::kInvalidParameter;
        return true;
    }
    status = MS::kSuccess;
    return params.mIsMaxDistance && distance > params.mMaxDist;
}

class MeshAcceleratorGuard
{
public:
    explicit MeshAcceleratorGuard(MFnMesh& mesh)
        : mesh_(std::addressof(mesh))
    {
    }

    ~MeshAcceleratorGuard()
    {
        if (mesh_) {
            mesh_->freeCachedIntersectionAccelerator();
        }
    }

private:
    MFnMesh* mesh_;
};

MStatus projectToSurface(
    const DPRayParams& params,
    DPointList& points,
    DPIdList& hits)
{
    MStatus status;
    MFnNurbsSurface surface(params.m_object, &status);
    if (!status) return status;

    for (std::size_t index = 0U; index < points.size(); ++index) {
        MPoint basePoint;
        if (!toMPoint(points[index], basePoint)) {
            return MS::kInvalidParameter;
        }

        MDoubleArray uCoordinates;
        MDoubleArray vCoordinates;
        MDoubleArray distances;
        MPointArray hitPoints;
        const bool intersected = surface.intersect(
            basePoint,
            params.mRayVec,
            uCoordinates,
            vCoordinates,
            hitPoints,
            params.mRayTolerance,
            MSpace::kWorld,
            true,
            &distances,
            false,
            nullptr,
            &status);
        if (!status) return status;
        if (!intersected) continue;

        const unsigned int count = hitPoints.length();
        if (count == 0U ||
            uCoordinates.length() != count ||
            vCoordinates.length() != count ||
            distances.length() != count) {
            return MS::kFailure;
        }

        unsigned int selected = 0U;
        for (unsigned int hitIndex = 0U; hitIndex < count; ++hitIndex) {
            if (!dommy::maya::isFinitePoint(hitPoints[hitIndex]) ||
                !std::isfinite(uCoordinates[hitIndex]) ||
                !std::isfinite(vCoordinates[hitIndex]) ||
                !std::isfinite(distances[hitIndex])) {
                return MS::kInvalidParameter;
            }
            const bool replace = params.mIsFarthest
                ? distances[hitIndex] > distances[selected]
                : distances[hitIndex] < distances[selected];
            if (replace) selected = hitIndex;
        }

        if (params.mIsMaxDistance &&
            distances[selected] > params.mMaxDist) {
            continue;
        }

        MVector normal;
        MVector* normalPointer = nullptr;
        if (params.mLift != 0.0f) {
            normal = surface.normal(
                uCoordinates[selected],
                vCoordinates[selected],
                MSpace::kWorld,
                &status);
            if (!status) return status;
            normalPointer = &normal;
        }

        status = applyHit(
            params,
            basePoint,
            hitPoints[selected],
            normalPointer,
            points[index]);
        if (!status) return status;
        hits[index] = true;
    }

    return MS::kSuccess;
}

MStatus projectToMesh(
    const DPRayParams& params,
    DPointList& points,
    DPIdList& hits)
{
    float tolerance = 0.0f;
    if (!dommy::numeric::checkedFloatCast(params.mRayTolerance, tolerance) ||
        tolerance < 0.0f) {
        return MS::kInvalidParameter;
    }
    MStatus status;
    MFnMesh mesh(params.m_object, &status);
    if (!status) return status;

    MMeshIsectAccelParams accelerator = mesh.autoUniformGridParams();
    MeshAcceleratorGuard acceleratorGuard(mesh);

    MFloatVector rayDirection;
    if (!toNormalizedFloatVector(params.mRayVec, rayDirection)) {
        return MS::kInvalidParameter;
    }
    const float maxParameter = params.mIsMaxDistance
        ? params.mMaxDist
        : std::numeric_limits<float>::max();

    for (std::size_t index = 0U; index < points.size(); ++index) {
        MPoint basePoint;
        MFloatPoint raySource;
        if (!toMPoint(points[index], basePoint) ||
            !toFloatPoint(basePoint, raySource)) {
            return MS::kInvalidParameter;
        }

        MFloatPointArray hitPoints;
        MFloatArray hitParameters;
        MIntArray hitFaces;
        const bool intersected = mesh.allIntersections(
            raySource,
            rayDirection,
            nullptr,
            nullptr,
            true,
            MSpace::kWorld,
            maxParameter,
            false,
            &accelerator,
            true,
            hitPoints,
            &hitParameters,
            &hitFaces,
            nullptr,
            nullptr,
            nullptr,
            tolerance,
            &status);
        if (!status) return status;
        if (!intersected) continue;

        const unsigned int count = hitPoints.length();
        if (count == 0U ||
            hitParameters.length() != count ||
            hitFaces.length() != count) {
            return MS::kFailure;
        }
        const unsigned int selected =
            params.mIsFarthest ? count - 1U : 0U;
        if (hitFaces[selected] < 0 ||
            !std::isfinite(hitParameters[selected])) {
            return MS::kInvalidParameter;
        }

        MVector normal;
        MVector* normalPointer = nullptr;
        if (params.mLift != 0.0f) {
            status = mesh.getPolygonNormal(
                hitFaces[selected], normal, MSpace::kWorld);
            if (!status) return status;
            normalPointer = &normal;
        }

        status = applyHit(
            params,
            basePoint,
            MPoint(hitPoints[selected]),
            normalPointer,
            points[index]);
        if (!status) return status;
        hits[index] = true;
    }

    return MS::kSuccess;
}

MStatus moveToCurve(
    const DPRayParams& params,
    DPointList& points,
    DPIdList& hits)
{
    MStatus status;
    MFnNurbsCurve curve(params.m_object, &status);
    if (!status) return status;

    for (std::size_t index = 0U; index < points.size(); ++index) {
        MPoint basePoint;
        if (!toMPoint(points[index], basePoint)) {
            return MS::kInvalidParameter;
        }

        double parameter = 0.0;
        const MPoint closestPoint = curve.closestPoint(
            basePoint,
            &parameter,
            params.mRayTolerance,
            MSpace::kWorld,
            &status);
        if (!status || !std::isfinite(parameter) ||
            !dommy::maya::isFinitePoint(closestPoint)) {
            return status ? MStatus(MS::kInvalidParameter) : status;
        }
        if (exceedsMaximumDistance(
                params, basePoint, closestPoint, status)) {
            if (!status) return status;
            continue;
        }

        MVector normal;
        MVector* normalPointer = nullptr;
        if (params.mLift != 0.0f) {
            normal = curve.normal(parameter, MSpace::kWorld, &status);
            if (!status) return status;
            normalPointer = &normal;
        }

        status = applyHit(
            params,
            basePoint,
            closestPoint,
            normalPointer,
            points[index]);
        if (!status) return status;
        hits[index] = true;
    }

    return MS::kSuccess;
}

MStatus moveToSurface(
    const DPRayParams& params,
    DPointList& points,
    DPIdList& hits)
{
    MStatus status;
    MFnNurbsSurface surface(params.m_object, &status);
    if (!status) return status;

    for (std::size_t index = 0U; index < points.size(); ++index) {
        MPoint basePoint;
        if (!toMPoint(points[index], basePoint)) {
            return MS::kInvalidParameter;
        }

        double uParameter = 0.0;
        double vParameter = 0.0;
        const MPoint closestPoint = surface.closestPoint(
            basePoint,
            &uParameter,
            &vParameter,
            false,
            params.mRayTolerance,
            MSpace::kWorld,
            &status);
        if (!status ||
            !std::isfinite(uParameter) ||
            !std::isfinite(vParameter) ||
            !dommy::maya::isFinitePoint(closestPoint)) {
            return status ? MStatus(MS::kInvalidParameter) : status;
        }
        if (exceedsMaximumDistance(
                params, basePoint, closestPoint, status)) {
            if (!status) return status;
            continue;
        }

        MVector normal;
        MVector* normalPointer = nullptr;
        if (params.mLift != 0.0f) {
            normal = surface.normal(
                uParameter,
                vParameter,
                MSpace::kWorld,
                &status);
            if (!status) return status;
            normalPointer = &normal;
        }

        status = applyHit(
            params,
            basePoint,
            closestPoint,
            normalPointer,
            points[index]);
        if (!status) return status;
        hits[index] = true;
    }

    return MS::kSuccess;
}

MStatus moveToMesh(
    const DPRayParams& params,
    DPointList& points,
    DPIdList& hits,
    const DPIdList* selection,
    const DVec3DataList* directions)
{
    MStatus status;
    MFnMesh mesh(params.m_object, &status);
    if (!status) return status;

    for (std::size_t index = 0U; index < points.size(); ++index) {
        if (selection && !(*selection)[index]) continue;

        MPoint basePoint;
        if (!toMPoint(points[index], basePoint)) {
            return MS::kInvalidParameter;
        }

        MPoint closestPoint;
        MVector normal;
        bool usedDirectionalHit = false;
        if (directions) {
            const MVector rawDirection(
                (*directions)[index][0],
                (*directions)[index][1],
                (*directions)[index][2]);
            MVector direction;
            if (!dommy::maya::isFiniteVector(rawDirection)) {
                return MS::kInvalidParameter;
            }
            if (rawDirection.length() <= kMFnMeshPointTolerance) {
                continue;
            }
            if (!dommy::maya::normalizedVector(rawDirection, direction)) {
                return MS::kInvalidParameter;
            }

            MPointArray hitPoints;
            MIntArray polygonIds;
            const bool intersected = mesh.intersect(
                basePoint,
                direction,
                hitPoints,
                kMFnMeshPointTolerance,
                MSpace::kWorld,
                &polygonIds,
                &status);
            if (!status) return status;
            if (intersected) {
                if (hitPoints.length() == 0U ||
                    polygonIds.length() == 0U ||
                    polygonIds[0] < 0) {
                    return MS::kFailure;
                }
                closestPoint = hitPoints[0];
                status = mesh.getPolygonNormal(
                    polygonIds[0], normal, MSpace::kWorld);
                if (!status) return status;
                usedDirectionalHit = true;
            }
        }

        if (!usedDirectionalHit) {
            status = mesh.getClosestPointAndNormal(
                basePoint,
                closestPoint,
                normal,
                MSpace::kWorld,
                nullptr,
                nullptr);
            if (!status) return status;
        }
        if (!dommy::maya::isFinitePoint(closestPoint) ||
            !dommy::maya::isFiniteVector(normal)) {
            return MS::kInvalidParameter;
        }
        if (exceedsMaximumDistance(
                params, basePoint, closestPoint, status)) {
            if (!status) return status;
            continue;
        }

        status = applyHit(
            params,
            basePoint,
            closestPoint,
            &normal,
            points[index]);
        if (!status) return status;
        hits[index] = true;
    }

    return MS::kSuccess;
}

MStatus validateCollections(
    const std::vector<DPointList*>& points,
    const std::vector<DPIdList>& hits,
    const std::vector<DPIdList>* selections,
    const std::vector<DVec3DataList>* directions)
{
    if (points.size() != hits.size() ||
        (selections && selections->size() != points.size()) ||
        (directions && directions->size() != points.size())) {
        return MS::kInvalidParameter;
    }

    for (std::size_t index = 0U; index < points.size(); ++index) {
        if (!points[index] || hits[index].size() != points[index]->size() ||
            (selections &&
             (*selections)[index].size() != points[index]->size()) ||
            (directions &&
             (*directions)[index].size() != points[index]->size())) {
            return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}

} // namespace

namespace dommy {
namespace ray {

Generator::Generator(
    const DPRayParams& params,
    std::vector<DPointList*>& points,
    std::vector<DPIdList>& hits)
    : params_(params)
    , points_(points)
    , hits_(hits)
    , selections_(nullptr)
    , directions_(nullptr)
{
}

Generator::Generator(
    const DPRayParams& params,
    std::vector<DPointList*>& points,
    std::vector<DPIdList>& hits,
    std::vector<DPIdList>& selections)
    : params_(params)
    , points_(points)
    , hits_(hits)
    , selections_(&selections)
    , directions_(nullptr)
{
}

Generator::Generator(
    const DPRayParams& params,
    std::vector<DPointList*>& points,
    std::vector<DPIdList>& hits,
    std::vector<DPIdList>& selections,
    std::vector<DVec3DataList>& directions)
    : params_(params)
    , points_(points)
    , hits_(hits)
    , selections_(&selections)
    , directions_(&directions)
{
}

MStatus Generator::operator()() const
{
    MStatus status = validateCollections(
        points_, hits_, selections_, directions_);
    if (!status) return status;
    if (params_.m_object.isNull()) return MS::kInvalidParameter;

    for (std::size_t index = 0U; index < points_.size(); ++index) {
        if (params_.m_type == DP_RAY_PROJECTION) {
            if (params_.m_objectType == MFnData::kNurbsSurface) {
                status = projectToSurface(
                    params_, *points_[index], hits_[index]);
            } else if (params_.m_objectType == MFnData::kMesh) {
                status = projectToMesh(
                    params_, *points_[index], hits_[index]);
            } else {
                return MS::kInvalidParameter;
            }
        } else if (params_.m_type == DP_RAY_MINIMUM) {
            if (params_.m_objectType == MFnData::kNurbsCurve) {
                status = moveToCurve(
                    params_, *points_[index], hits_[index]);
            } else if (params_.m_objectType == MFnData::kNurbsSurface) {
                status = moveToSurface(
                    params_, *points_[index], hits_[index]);
            } else if (params_.m_objectType == MFnData::kMesh) {
                const DPIdList* selection = selections_
                    ? &(*selections_)[index] : nullptr;
                const DVec3DataList* direction = directions_
                    ? &(*directions_)[index] : nullptr;
                status = moveToMesh(
                    params_,
                    *points_[index],
                    hits_[index],
                    selection,
                    direction);
            } else {
                return MS::kInvalidParameter;
            }
        } else {
            return MS::kInvalidParameter;
        }

        if (!status) return status;
    }

    return MS::kSuccess;
}

} // namespace ray
} // namespace dommy
