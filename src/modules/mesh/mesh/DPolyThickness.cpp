//
//  DPolyThickness.cpp
//  dommyTool
//
//  Created by yuzo on 2018/05/30.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaMeshColorOutput.h"
#include "DPolyThickness.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DCheckedSize.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaMeshInput.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DNumericValidation.h"

#include "DMayaDebug.h"
#include "DMayaHeaders.h"
#include "general_ids.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

class IntersectionCacheGuard {
public:
    explicit IntersectionCacheGuard(MFnMesh& mesh) : mesh_(mesh) {}
    ~IntersectionCacheGuard()
    {
        mesh_.freeCachedIntersectionAccelerator();
    }

private:
    IntersectionCacheGuard(const IntersectionCacheGuard&);
    IntersectionCacheGuard& operator=(const IntersectionCacheGuard&);
    MFnMesh& mesh_;
};

MStatus rayParameters(const MPointArray& points,
                      float& maximumDistance,
                      double& sourceOffset)
{
    if (points.length() == 0U) {
        maximumDistance = 0.0f;
        sourceOffset = 0.0;
        return MS::kSuccess;
    }

    MPoint minimum = points[0U];
    MPoint maximum = points[0U];
    for (unsigned int index = 0U; index < points.length(); ++index) {
        const MPoint& point = points[index];
        if (!dommy::maya::isFloatRepresentablePoint(point)) return MS::kInvalidParameter;
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        minimum.z = std::min(minimum.z, point.z);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
        maximum.z = std::max(maximum.z, point.z);
    }

    const long double dx =
        static_cast<long double>(maximum.x) - minimum.x;
    const long double dy =
        static_cast<long double>(maximum.y) - minimum.y;
    const long double dz =
        static_cast<long double>(maximum.z) - minimum.z;
    const long double diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(static_cast<double>(diagonal))) {
        return MS::kInvalidParameter;
    }

    const double diagonalValue = static_cast<double>(diagonal);
    sourceOffset = std::max(1.0e-6, diagonalValue * 1.0e-6);
    const double maximumRayValue =
        std::max(1.0e-4, diagonalValue * 2.0 + sourceOffset);
    if (!dommy::numeric::checkedFloatCast(
            maximumRayValue, maximumDistance)) {
        return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

} // namespace

const MTypeId DPolyThickness::typeId(ID_MESH_THICKNESS);

MObject DPolyThickness::inBaseMesh;
MObject DPolyThickness::outMesh;

void* DPolyThickness::creator()
{
    return ::dommy::maya::createObject("DPolyThickness", [&]() {
        return new DPolyThickness();
    });
}

MStatus DPolyThickness::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) return MS::kUnknownParameter;

    MStatus status;
    MObject inputMesh;
    status = dommy::maya::getConnectedMeshObject(
        thisMObject(), inBaseMesh, data, inputMesh);
    if (!status) {
        EchoWarning(name().asChar(), "A valid input mesh is required.");
        return status;
    }

    MFnMesh inputMeshFn(inputMesh, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const int rawPointCount = inputMeshFn.numVertices(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    std::size_t pointCount = 0U;
    if (!dommy::generation::checkedCount(
            rawPointCount, dommy::generation::kMaxGeneratedPointCount,
            pointCount)) {
        return MS::kInvalidParameter;
    }

    MFnMeshData outputDataCreator;
    MObject outputData = outputDataCreator.create(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (outputData.isNull()) return MS::kFailure;

    MFnMesh outputMesh;
    const MObject copiedMesh = outputMesh.copy(inputMesh, outputData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (copiedMesh.isNull()) return MS::kFailure;

    MPointArray points;
    DOMMY_RETURN_IF_FAILURE(outputMesh.getPoints(points, MSpace::kObject));
    if (points.length() != pointCount || !dommy::maya::areFinitePoints(points)) {
        return MS::kInvalidParameter;
    }

    float maximumRayDistance = 0.0f;
    double sourceOffset = 0.0;
    DOMMY_RETURN_IF_FAILURE(
        rayParameters(points, maximumRayDistance, sourceOffset));

    MFloatArray thicknesses;
    DOMMY_RETURN_IF_FAILURE(thicknesses.setLength(static_cast<unsigned int>(pointCount)));
    if (thicknesses.length() != pointCount) return MS::kFailure;
    for (unsigned int i = 0U; i < thicknesses.length(); ++i) thicknesses[i] = -1.0f;
    float maximumThickness = 0.0f;
    MMeshIsectAccelParams accelerator = MFnMesh::autoUniformGridParams();
    IntersectionCacheGuard cacheGuard(outputMesh);

    MItMeshVertex vertex(outputData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    status = dommy::maya::visitMeshElements(vertex, pointCount, [&](MItMeshVertex& item) -> MStatus {
        const int rawIndex = item.index(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (!dommy::memory::indexInRange(rawIndex, pointCount)) {
            return MS::kInvalidParameter;
        }
        const std::size_t index = static_cast<std::size_t>(rawIndex);

        MVector normal;
        DOMMY_RETURN_IF_FAILURE(
            item.getNormal(normal, MSpace::kObject));
        if (!dommy::maya::isFiniteVector(normal)) return MS::kInvalidParameter;
        const double normalLength = normal.length();
        if (!std::isfinite(normalLength)) return MS::kInvalidParameter;
        if (normalLength <= std::numeric_limits<double>::epsilon()) return MS::kSuccess;
        DOMMY_RETURN_IF_FAILURE(normal.normalize());
        const MVector rayDirection = -normal;
        const MPoint source = points[index] + rayDirection * sourceOffset;
        if (!dommy::maya::isFloatRepresentablePoint(source) ||
            !dommy::maya::isFloatRepresentableVector(rayDirection)) {
            return MS::kInvalidParameter;
        }

        MFloatPoint hitPoint;
        float hitParameter = 0.0f;
        status = MS::kSuccess;
        const bool hit = outputMesh.closestIntersection(
            MFloatPoint(source), MFloatVector(rayDirection),
            NULL, NULL, false, MSpace::kObject, maximumRayDistance, false,
            &accelerator, hitPoint, &hitParameter,
            NULL, NULL, NULL, NULL, 1.0e-6f, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (!hit) return MS::kSuccess;
        if (!std::isfinite(hitParameter) || hitParameter < 0.0f) return MS::kInvalidParameter;

        float thickness = 0.0f;
        if (!dommy::numeric::checkedFloatCast(
                static_cast<double>(hitParameter) + sourceOffset,
                thickness)) {
            return MS::kInvalidParameter;
        }
        thicknesses[index] = thickness;
        maximumThickness = std::max(maximumThickness, thickness);
        return MS::kSuccess;
    });
    DOMMY_RETURN_IF_FAILURE(status);

    MColorArray colors;
    DOMMY_RETURN_IF_FAILURE(colors.setLength(static_cast<unsigned int>(pointCount)));
    if (colors.length() != pointCount) return MS::kFailure;
    for (unsigned int i = 0U; i < colors.length(); ++i) colors[i] = MColor(0, 0, 0, 1);
    if (maximumThickness > 0.0f) {
        for (std::size_t index = 0U; index < pointCount; ++index) {
            if (thicknesses[index] < 0.0f) continue;
            const float ratio = std::max(
                0.0f, std::min(1.0f, thicknesses[index] / maximumThickness));
            colors[index] = MColor(ratio, ratio, ratio, 1.0f);
        }
    }

    DOMMY_RETURN_IF_FAILURE(dommy::maya::setMeshDataVertexColors(
        outputMesh, colors, MString("thickness")));

    MDataHandle outputHandle = data.outputValue(outMesh, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(outputHandle.set(outputData));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPolyThickness")

MStatus DPolyThickness::initialize() try
{
    MStatus status;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;

    inBaseMesh = genericAttribute.create("inBaseMesh", "inbm", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inBaseMesh));

    outMesh = typedAttribute.create(
        "outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inBaseMesh, outMesh));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPolyThickness", "initialize", std::current_exception());
}
