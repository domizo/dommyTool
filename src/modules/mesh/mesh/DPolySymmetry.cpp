//
//  DPolySymmetry.cpp
//  dommyTool
//
//  Created by yuzo on 2018/05/15.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPolySymmetry.h"
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

const MTypeId DPolySymmetry::typeId(ID_MESH_SYMMETRY);

MObject DPolySymmetry::inBaseMesh;
MObject DPolySymmetry::inTargetMesh;
MObject DPolySymmetry::aType;
MObject DPolySymmetry::aTolerance;
MObject DPolySymmetry::aUseColor;
MObject DPolySymmetry::outMesh;

void* DPolySymmetry::creator()
{
    return ::dommy::maya::createObject("DPolySymmetry", [&]() {
        return new DPolySymmetry();
    });
}

MStatus DPolySymmetry::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outMesh) return MS::kUnknownParameter;

    MStatus status;
    MDataHandle control = data.inputValue(aType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short type = control.asShort();
    if (type < 0 || type > 1) return MS::kInvalidParameter;

    control = data.inputValue(aTolerance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double tolerance = control.asDouble();
    if (!std::isfinite(tolerance) || tolerance < 0.0) {
        return MS::kInvalidParameter;
    }
    control = data.inputValue(aUseColor, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool useColor = control.asBool();

    dommy::maya::MeshPairInput meshes;
    status = dommy::maya::getMatchingMeshPair(
        thisMObject(), inBaseMesh, inTargetMesh, data, meshes);
    if (!status) {
        EchoWarning(name().asChar(),
                    "Base and target meshes must contain matching finite point arrays.");
        return status;
    }

    std::vector<double> weights(meshes.pointCount, 1.0);
    if (useColor) {
        MFnMesh targetMesh(meshes.secondObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        const int colorCount = targetMesh.numColors(&status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if (colorCount < 0) return MS::kInvalidParameter;
        if (colorCount > 0) {
            MColorArray colors;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::getPerVertexColors(
                targetMesh, meshes.pointCount, colors));
            for (std::size_t index = 0U; index < meshes.pointCount; ++index) {
                weights[index] = std::max(
                    0.0, std::min(
                             1.0, static_cast<double>(colors[index][0])));
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
        const MPoint point = current.position(MSpace::kObject, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        if ((type == 0 && point.x < 0.0) ||
            (type == 1 && point.x > 0.0) || weights[index] == 0.0) {
            return MS::kSuccess;
        }

        const std::size_t match = mirroredMatches[index];
        if (match == dommy::mesh::kNoPointMatch) return MS::kSuccess;
        if (!dommy::memory::indexInRange(match, meshes.pointCount)) {
            return MS::kInvalidParameter;
        }

        const MPoint& targetPoint = meshes.secondPoints[match];
        const MPoint mirroredTarget(
            -targetPoint.x, targetPoint.y, targetPoint.z);
        MPoint result;
        if (!dommy::maya::checkedPointBlend(point, mirroredTarget, weights[index], result,
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
DOMMY_COMPUTE_CATCH("DPolySymmetry")

MStatus DPolySymmetry::initialize() try
{
    MStatus status;
    MFnNumericAttribute numericAttribute;
    MFnGenericAttribute genericAttribute;
    MFnTypedAttribute typedAttribute;
    MFnEnumAttribute enumAttribute;

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

    aType = enumAttribute.create("Type", "ty", 0, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Left to Right", 0));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.addField("Right to Left", 1));
    DOMMY_RETURN_IF_FAILURE(enumAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aType));

    aTolerance = numericAttribute.create(
        "Tolerance", "tol", MFnNumericData::kDouble, 0.1, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTolerance));

    aUseColor = numericAttribute.create(
        "UseColor", "ucol", MFnNumericData::kBoolean, true, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(numericAttribute.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseColor));

    outMesh = typedAttribute.create(
        "outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(typedAttribute.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outMesh));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inBaseMesh, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inTargetMesh, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aType, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTolerance, outMesh));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUseColor, outMesh));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPolySymmetry", "initialize", std::current_exception());
}
