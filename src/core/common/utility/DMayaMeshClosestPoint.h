#pragma once

#include "DMayaValueValidation.h"

#include <maya/MMeshIntersector.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <cfloat>
#include <cmath>

namespace dommy {
namespace maya {

// Builds Maya's thread-safe mesh intersector once per evaluation and reuses it
// for every query. The matrix maps the mesh's object space into query space.
class MeshClosestPointQuery final
{
public:
    MeshClosestPointQuery()
        : initialized_(false)
    {
    }

    MStatus create(const MObject& meshObject,
                   const MMatrix& objectToQuerySpace = MMatrix::identity)
    {
        initialized_ = false;
        if (meshObject.isNull() || !isFiniteMatrix(objectToQuerySpace)) {
            return MS::kInvalidParameter;
        }

        // Maya 2019's API takes a non-const MObject reference but does not
        // transfer ownership or mutate the underlying mesh data.
        MObject mutableMeshObject = meshObject;
        const MStatus status =
            intersector_.create(mutableMeshObject, objectToQuerySpace);
        if (!status || !intersector_.isCreated()) {
            return status ? MStatus(MS::kFailure) : status;
        }

        initialized_ = true;
        return MS::kSuccess;
    }

    MStatus getClosestPoint(const MPoint& point,
                            MPoint& closestPoint,
                            double maximumDistance = DBL_MAX) const
    {
        if (!initialized_ || !isFinitePoint(point) ||
            std::isnan(maximumDistance) || maximumDistance < 0.0) {
            return MS::kInvalidParameter;
        }

        MPointOnMesh meshPoint;
        const MStatus status =
            intersector_.getClosestPoint(point, meshPoint, maximumDistance);
        if (!status) return status;

        const MFloatPoint& candidate = meshPoint.getPoint();
        const MPoint converted(
            static_cast<double>(candidate.x),
            static_cast<double>(candidate.y),
            static_cast<double>(candidate.z));
        if (!isFinitePoint(converted)) return MS::kFailure;

        closestPoint = converted;
        return MS::kSuccess;
    }

private:
    MMeshIntersector intersector_;
    bool initialized_;
};

} // namespace maya
} // namespace dommy
