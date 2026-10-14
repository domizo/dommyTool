//
//  DBSFlipMesh.cpp
//  dommyTool
//
//  Created by yuzo on 2018/03/18.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DBSFlipMesh.h"
#include "common/utility/DStatusOperation.h"

#include "common/utility/DCheckedSize.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaMeshInput.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DMayaPointBlend.h"

#include "DMayaDebug.h"
#include "DMayaHeaders.h"
#include "general_ids.h"

#include <algorithm>
#include <cmath>
#include <vector>

const MTypeId DBSFlipMesh::typeId(ID_MESH_BLEND_SHAPE_FLIP);

MObject DBSFlipMesh::inBaseMesh;
MObject DBSFlipMesh::inTargetMesh;
MObject DBSFlipMesh::aUseMask;
MObject DBSFlipMesh::aInvert;
MObject DBSFlipMesh::inMaskMesh;
MObject DBSFlipMesh::aTolerance;
MObject DBSFlipMesh::outMesh;

void* DBSFlipMesh::creator()
{
    return ::dommy::maya::createObject("DBSFlipMesh", [&]() {
        return new DBSFlipMesh();
    });
}

MStatus DBSFlipMesh::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) return MS::kUnknownParameter;

    MStatus status;
    MDataHandle control = data.inputValue(aTolerance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double tolerance = control.asDouble();
    if (!std::isfinite(tolerance) || tolerance < 0.0) {
        return MS::kInvalidParameter;
    }
    control = data.inputValue(aUseMask, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useMask = control.asBool();
    control = data.inputValue(aInvert, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool invert = control.asBool();

    dommy::maya::MeshPairInput meshes;
    status = dommy::maya::getMatchingMeshPair(
        thisMObject(), inBaseMesh, inTargetMesh, data, meshes);
    if (!status) {
        EchoWarning(name().asChar(),
                    "Base and target meshes must contain matching finite point arrays.");
        return status;
    }

    MColorArray maskColors;
    if (useMask) {
        MObject maskMeshObject;
        status = dommy::maya::getConnectedMeshObject(
            thisMObject(), inMaskMesh, data, maskMeshObject);
        if (!status) {
            EchoWarning(name().asChar(), "A valid mask mesh is required.");
            return status;
        }

        MFnMesh maskMesh(maskMeshObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const int maskPointCount = maskMesh.numVertices(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (maskPointCount < 0 ||
            static_cast<std::size_t>(maskPointCount) != meshes.pointCount) {
            return MS::kInvalidParameter;
        }
        const int colorCount = maskMesh.numColors(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (colorCount < 0) return MS::kInvalidParameter;
        if (colorCount == 0) {
            EchoWarning(name().asChar(), "Not color set in mask mesh.");
            return MS::kFailure;
        }
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPerVertexColors(
            maskMesh, meshes.pointCount, maskColors));
        if (invert) {
            for (unsigned int index = 0U; index < maskColors.length(); ++index) {
                maskColors[index][0] = 1.0f - maskColors[index][0];
            }
        }
    }

    MFnMeshData outputDataCreator;
    MObject outputData = outputDataCreator.create(&status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (outputData.isNull()) return MS::kFailure;

    MFnMesh outputMesh;
    const MObject copiedMesh = outputMesh.copy(
        meshes.firstObject, outputData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (copiedMesh.isNull()) return MS::kFailure;

    std::vector<std::size_t> mirroredMatches;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getMirroredPointMatches(
        meshes.firstPoints, tolerance, mirroredMatches));

    MItMeshVertex vertex(outputData, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::visitMeshElements(
        vertex, meshes.pointCount, [&](MItMeshVertex& current) -> MStatus {
        const int rawIndex = current.index(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (!dommy::memory::indexInRange(rawIndex, meshes.pointCount)) {
            return MS::kInvalidParameter;
        }
        const std::size_t index = static_cast<std::size_t>(rawIndex);
        const std::size_t match = mirroredMatches[index];
        if (match == dommy::mesh::kNoPointMatch) return MS::kSuccess;
        if (!dommy::memory::indexInRange(match, meshes.pointCount)) {
            return MS::kInvalidParameter;
        }

        const MPoint point = current.position(MSpace::kObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const MPoint& targetPoint = meshes.secondPoints[match];
        const MPoint mirroredTarget(
            -targetPoint.x, targetPoint.y, targetPoint.z);

        double blend = 1.0;
        if (useMask) {
            const double exclusion = std::max(
                0.0, std::min(
                         1.0, static_cast<double>(maskColors[index][0])));
            blend = 1.0 - exclusion;
        }
        MPoint result;
        if (!dommy::maya::checkedPointBlend(point, mirroredTarget, blend, result,
                dommy::maya::PointBlendRounding::LegacyDisplacement)) {
            return MS::kInvalidParameter;
        }
        return current.setPosition(result, MSpace::kObject);
    }));

    DOMMY_RETURN_IF_FAILURE(outputMesh.updateSurface());

    MDataHandle outputHandle = data.outputValue(outMesh, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(outputHandle.set(outputData));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DBSFlipMesh")

MStatus DBSFlipMesh::initialize() try
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;

    inBaseMesh = genericAttribute.create("inBaseMesh", "inbm", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inBaseMesh));

    inTargetMesh = genericAttribute.create("inTargetMesh", "intm", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetMesh));

    aUseMask = numericAttribute.create(
        "aUseMask", "umsk", MFnNumericData::kBoolean, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseMask));

    aInvert = numericAttribute.create(
        "Invert", "inv", MFnNumericData::kBoolean, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));

    inMaskMesh = genericAttribute.create("inMaskMesh", "inmsk", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(genericAttribute.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(genericAttribute.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inMaskMesh));

    aTolerance = numericAttribute.create(
        "Tolerance", "tol", MFnNumericData::kDouble, 0.0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    outMesh = typedAttribute.create(
        "outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inBaseMesh, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inTargetMesh, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseMask, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aInvert, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inMaskMesh, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTolerance, outMesh));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DBSFlipMesh", "initialize", std::current_exception());
}
