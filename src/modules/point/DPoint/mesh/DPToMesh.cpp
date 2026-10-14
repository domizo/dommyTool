//
//  DPToMesh.cpp
//  dommyTool
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPToMesh.h"
#include "common/utility/DMayaMeshDataOutput.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "DNodeOperation.h"
#include "DPData.h"
#include "DPointCommon.h"
#include "dpoint_ids.h"

#include <cstddef>

const MTypeId DPToMesh::typeId(ID_DP_TO_MESH);

MObject DPToMesh::inDP;
MObject DPToMesh::aGenerateType;
MObject DPToMesh::aNormalAttribute;
MObject DPToMesh::aNormal;
MObject DPToMesh::aMinAngle;
MObject DPToMesh::aRadius;
MObject DPToMesh::aDistance;
MObject DPToMesh::outMesh;

void* DPToMesh::creator()
{
    return ::dommy::maya::createObject("DPToMesh", [&]() {
        return new DPToMesh();
    });
}

MStatus DPToMesh::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) return MS::kUnknownParameter;

    MStatus status;
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kFailure;

    const MDataHandle generationHandle = data.inputValue(aGenerateType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short generationType = generationHandle.asShort();
    if (generationType != 0) {
        MGlobal::displayError(
            "DPToMesh supports Ordered Triangle only. The historical "
            "reconstruction modes were incomplete and have been retired.");
        return MS::kInvalidParameter;
    }

    const std::size_t inputPointCount = input->numberOfPoints();
    if (inputPointCount < 3U ||
        inputPointCount > dommy::generation::kMaxGeneratedPointCount) {
        MGlobal::displayError(
            "DPToMesh requires at least three points within the generation "
            "budget.");
        return MS::kInvalidParameter;
    }

    // Admit the complete conversion before requesting a Maya point array.
    // Include the discarded tail in point storage, but only complete triples
    // in face/corner storage. This is the shared staging estimate, not a
    // guarantee about native Maya mesh/DG allocation overhead.
    const std::size_t usablePoints = inputPointCount - inputPointCount % 3U;
    dommy::mesh::MeshMergePlan plan;
    if (!dommy::mesh::appendMeshMergePlan(
            inputPointCount, usablePoints / 3U, usablePoints, 0U, plan) ||
        !dommy::mesh::validMeshOperationStorage(plan)) {
        return MS::kFailure;
    }
    const unsigned int usablePointCount = static_cast<unsigned int>(usablePoints);
    const unsigned int faceCount = static_cast<unsigned int>(plan.faces);

    MPointArray points = input->getMPosition();
    if (points.length() != inputPointCount) return MS::kFailure;
    for (unsigned int index = 0U; index < points.length(); ++index) {
        if (!dommy::maya::isFinitePoint(points[index])) {
            return MS::kInvalidParameter;
        }
    }

    if (usablePointCount != points.length()) {
        DOMMY_RETURN_IF_FAILURE(points.setLength(usablePointCount));
    }

    MIntArray faceCounts;
    DOMMY_RETURN_IF_FAILURE(faceCounts.setLength(faceCount));
    for (unsigned int index = 0U; index < faceCount; ++index) {
        faceCounts[index] = 3;
    }
    MIntArray connections;
    DOMMY_RETURN_IF_FAILURE(connections.setLength(usablePointCount));
    for (unsigned int index = 0U; index < usablePointCount; ++index) {
        connections[index] = static_cast<int>(index);
    }

    MObject meshData;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::createMeshDataObject(meshData));
    MFnMesh meshFn;
    const MObject geometry = meshFn.create(
        usablePointCount, faceCount, points, faceCounts, connections,
        meshData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (geometry.isNull()) return MS::kFailure;

    MDataHandle output = data.outputValue(outMesh, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(output.set(meshData));
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPToMesh")

MStatus DPToMesh::initialize()
{
    MStatus status;
    MFnGenericAttribute genericAttribute;
    MFnNumericAttribute numericAttribute;
    MFnTypedAttribute typedAttribute;

    inDP = typedAttribute.create(
        "inDP", "idp", DPData::id, MObject::kNullObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnEnumAttribute generationAttribute;
    aGenerateType = generationAttribute.create(
        "GenerateType", "gty", 0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        generationAttribute.addField("Ordered Triangle", 0));
    // Preserve historical numeric values so old scenes fail explicitly
    // instead of silently selecting a different operation.
    DOMMY_RETURN_IF_FAILURE(
        generationAttribute.addField("Retired Shrink Volume", 1));
    DOMMY_RETURN_IF_FAILURE(
        generationAttribute.addField("Retired Convex3D", 2));
    DOMMY_RETURN_IF_FAILURE(generationAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(generationAttribute.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGenerateType));

    MFnStringData stringData;
    const MObject defaultNormalAttribute = stringData.create("N", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    aNormalAttribute = typedAttribute.create(
        "NormalAttribute", "nattr", MFnData::kString,
        defaultNormalAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttribute));

    aNormal = numericAttribute.create(
        "Normal", "norm", MFnNumericData::k3Double, 0.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        numericAttribute.setDefault(0.0, 1.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormal));

    aMinAngle = numericAttribute.create(
        "MinAngle", "mang", MFnNumericData::kDouble, 20.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMinAngle));

    aRadius = numericAttribute.create(
        "Radius", "rad", MFnNumericData::kDouble, 30.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRadius));

    aDistance = numericAttribute.create(
        "Distance", "dist", MFnNumericData::kDouble, 30.0, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));

    outMesh = genericAttribute.create("outMesh", "omh", &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aGenerateType, outMesh));
    DOMMY_RETURN_IF_FAILURE(
        attributeAffects(aNormalAttribute, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aNormal, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinAngle, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRadius, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDistance, outMesh));
    return MS::kSuccess;
}
