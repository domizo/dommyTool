#include "DRayDeformer.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DDeformerGuard.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DMayaDeformerGeometry.h"
#include "common/utility/DMayaMeshClosestPoint.h"
#include "common/utility/DMayaValueValidation.h"

#include <maya/MDataHandle.h>
#include <maya/MFnData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MItGeometry.h>
#include <maya/MMatrix.h>

#include <cmath>

namespace {

const double kMinimumTransformDeterminant = 1.0e-12;

class IntersectionCacheGuard {
  public:
    explicit IntersectionCacheGuard(MFnMesh& meshFn) : meshFn_(meshFn) {}
    ~IntersectionCacheGuard() noexcept {
        meshFn_.freeCachedIntersectionAccelerator();
    }

  private:
    IntersectionCacheGuard(const IntersectionCacheGuard&);
    IntersectionCacheGuard& operator=(const IntersectionCacheGuard&);
    MFnMesh& meshFn_;
};

} // namespace

MTypeId DRayDeformer::typeId(0x0091A);
MString DRayDeformer::typeName("DRayDeformer");

MObject DRayDeformer::rayType;
MObject DRayDeformer::distance;
MObject DRayDeformer::inObject;
MObject DRayDeformer::inObjectMatrix;

void* DRayDeformer::creator() {
    return ::dommy::maya::createObject("DRayDeformer", [&]() {
        return new DRayDeformer();
    });
}

MStatus DRayDeformer::deform(MDataBlock& data, MItGeometry& iter, const MMatrix& localToWorld,
                             unsigned int geomIndex) try {
    MStatus status;
    MDataHandle envHandle = data.inputValue(envelope, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float env = envHandle.asFloat();
    if (!std::isfinite(env)) {
        return MS::kFailure;
    }
    if (env == 0.0f) {
        return MS::kSuccess;
    }

    MDataHandle targetHandle = data.inputValue(inObject, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (targetHandle.type() != MFnData::kMesh) {
        MGlobal::displayWarning("DRayDeformer input is not a mesh.");
        return MS::kFailure;
    }

    MFnMesh targetMeshFn(targetHandle.data(), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    IntersectionCacheGuard cacheGuard(targetMeshFn);

    MDataHandle targetToWorldHandle = data.inputValue(inObjectMatrix, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const MMatrix targetToWorld = targetToWorldHandle.asMatrix();
    const double targetDeterminant = targetToWorld.det4x4();
    if (!dommy::maya::isFiniteMatrix(targetToWorld) || !std::isfinite(targetDeterminant) ||
        std::abs(targetDeterminant) <= kMinimumTransformDeterminant) {
        MGlobal::displayError("DRayDeformer inObjectMatrix must be finite and invertible.");
        return MS::kFailure;
    }
    const MMatrix worldToTarget = targetToWorld.inverse();

    const double localDeterminant = localToWorld.det4x4();
    if (!dommy::maya::isFiniteMatrix(localToWorld) || !std::isfinite(localDeterminant) ||
        std::abs(localDeterminant) <= kMinimumTransformDeterminant) {
        MGlobal::displayError("DRayDeformer geometry transform must be finite and invertible.");
        return MS::kFailure;
    }

    MDataHandle distanceValueHandle = data.inputValue(distance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const float distanceValue = distanceValueHandle.asFloat();
    if (!std::isfinite(distanceValue)) {
        return MS::kFailure;
    }
    MDataHandle rayTypeValueHandle = data.inputValue(rayType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short rayTypeValue = rayTypeValueHandle.asShort();
    if (rayTypeValue < 0 || rayTypeValue > 1) {
        return MS::kFailure;
    }

    MMeshIsectAccelParams accelerator = targetMeshFn.autoUniformGridParams();
    dommy::maya::MeshClosestPointQuery closestPointQuery;
    if (rayTypeValue == 0) {
        status = closestPointQuery.create(targetHandle.data());
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    const MMatrix worldToLocal = localToWorld.inverse();
    if (!dommy::maya::isFiniteMatrix(worldToTarget) ||
        !dommy::maya::isFiniteMatrix(worldToLocal))
        return MS::kInvalidParameter;
    return dommy::maya::editDeformerPoints(
        iter, [&](unsigned int pointIndex, const MPoint& source, MPoint& result) -> MStatus {
            const float pointWeight = weightValue(data, geomIndex, pointIndex);
            if (!std::isfinite(pointWeight)) {
                return MS::kFailure;
            }
            if (pointWeight == 0.0f) {
                return MS::kSuccess;
            }

            const MPoint worldPoint = source * localToWorld;
            if (!dommy::maya::isFinitePoint(worldPoint)) {
                return MS::kFailure;
            }

            const MPoint targetPointObject = worldPoint * worldToTarget;
            if (!dommy::maya::isFinitePoint(targetPointObject)) {
                return MS::kFailure;
            }

            MPoint closestPointObject;
            if (rayTypeValue == 0) {
                status = closestPointQuery.getClosestPoint(targetPointObject, closestPointObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
            } else {
                MVector worldDirection = iter.normal(MSpace::kObject, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                worldDirection = worldDirection.transformAsNormal(localToWorld);
                MVector targetDirection = worldDirection * worldToTarget;
                if (!dommy::deformer::normalizeVector(targetDirection))
                    return MS::kInvalidParameter;
                if (!dommy::maya::isFloatRepresentablePoint(targetPointObject) ||
                    !dommy::maya::isFloatRepresentableVector(targetDirection) ||
                    targetDirection.length() <= 1.0e-12) {
                    return MS::kFailure;
                }

                MFloatPoint hitPoint;
                const bool hit = targetMeshFn.closestIntersection(
                    MFloatPoint(targetPointObject), MFloatVector(targetDirection), NULL, NULL, true,
                    MSpace::kObject, 1.0e10f, true, &accelerator, hitPoint, NULL, NULL, NULL, NULL,
                    NULL, 1.0e-6f, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (!hit)
                    return MS::kSuccess;
                closestPointObject = MPoint(hitPoint);
            }
            const MPoint closestPoint = closestPointObject * targetToWorld;
            if (!dommy::maya::isFinitePoint(closestPoint)) {
                return MS::kFailure;
            }

            MVector direction = worldPoint - closestPoint;
            if (!dommy::deformer::normalizeVector(direction))
                return MS::kInvalidParameter;
            MPoint targetPoint(closestPoint);
            if (direction.length() > 1.0e-12) {
                if (!dommy::deformer::offsetPoint(closestPoint, direction, distanceValue, 1.0, 1.0,
                                                  1.0, 1.0, targetPoint))
                    return MS::kInvalidParameter;
            }

            const double blend = double(pointWeight) * double(env);
            MPoint resultWorld;
            if (!dommy::deformer::blendPoint(worldPoint, targetPoint, blend, resultWorld))
                return MS::kInvalidParameter;
            result = resultWorld * worldToLocal;
            return MS::kSuccess;
        });
}
DOMMY_DEFORMER_CATCH("DRayDeformer")

MStatus DRayDeformer::initialize()
try {
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnEnumAttribute enumAttribute;
    MFnMatrixAttribute matrixAttribute;

    rayType = enumAttribute.create("rayType", "rayt", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Nearest", 0));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Vector", 1));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(rayType));

    distance = numericAttribute.create(
        "distance", "dist", MFnNumericData::kFloat, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(distance));

    inObject = genericAttribute.create("inObject", "iobj", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    inObjectMatrix = matrixAttribute.create(
        "inObjectMatrix", "iomat", MFnMatrixAttribute::kDouble, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setDefault(MMatrix::identity));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matrixAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObjectMatrix));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(rayType, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(distance, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObject, outputGeom));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObjectMatrix, outputGeom));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DRayDeformer", "initialize", std::current_exception());
}
