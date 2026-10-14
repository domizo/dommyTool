//
//  pointToSurfaceTransformNode.cpp
//  dommyTool
//
//  Created by yuzo on 2015/02/09.
//
//

#include "DPointToSurfaceTransformNode.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "general_ids.h"

#include <cmath>
#include <limits>

const MTypeId DPointToSurfaceTransformNode::typeId(0x0060A);

MObject DPointToSurfaceTransformNode::inputPositionX;
MObject DPointToSurfaceTransformNode::inputPositionY;
MObject DPointToSurfaceTransformNode::inputPositionZ;
MObject DPointToSurfaceTransformNode::inputPosition;

MObject DPointToSurfaceTransformNode::targetDirectionX;
MObject DPointToSurfaceTransformNode::targetDirectionY;
MObject DPointToSurfaceTransformNode::targetDirectionZ;
MObject DPointToSurfaceTransformNode::targetDirection;

MObject DPointToSurfaceTransformNode::inputMesh;

MObject DPointToSurfaceTransformNode::offset;

MObject DPointToSurfaceTransformNode::outputPositionX;
MObject DPointToSurfaceTransformNode::outputPositionY;
MObject DPointToSurfaceTransformNode::outputPositionZ;
MObject DPointToSurfaceTransformNode::outputPosition;

MObject DPointToSurfaceTransformNode::outputNormalX;
MObject DPointToSurfaceTransformNode::outputNormalY;
MObject DPointToSurfaceTransformNode::outputNormalZ;
MObject DPointToSurfaceTransformNode::outputNormal;

void* DPointToSurfaceTransformNode::creator() {
    return ::dommy::maya::createObject("DPointToSurfaceTransformNode", [&]() {
        return new DPointToSurfaceTransformNode();
    });
}

MStatus DPointToSurfaceTransformNode::compute(const MPlug& plug, MDataBlock& block) try {
    const MObject requestedAttribute = plug.attribute();
    const bool positionRequested =
        requestedAttribute == outputPosition || requestedAttribute == outputPositionX ||
        requestedAttribute == outputPositionY || requestedAttribute == outputPositionZ;
    const bool normalRequested =
        requestedAttribute == outputNormal || requestedAttribute == outputNormalX ||
        requestedAttribute == outputNormalY || requestedAttribute == outputNormalZ;
    if (!positionRequested && !normalRequested) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    MDataHandle meshHandle = block.inputValue(inputMesh, &status);
    if (!status)
        return status;
    if (meshHandle.type() != MFnData::kMesh)
        return MS::kInvalidParameter;

    const MObject inputMeshObject = meshHandle.data();
    if (inputMeshObject.isNull())
        return MS::kInvalidParameter;

    MDataHandle positionHandle = block.inputValue(inputPosition, &status);
    if (!status)
        return status;
    const double3& position = positionHandle.asDouble3();

    MDataHandle targetHandle = block.inputValue(targetDirection, &status);
    if (!status)
        return status;
    const double3& target = targetHandle.asDouble3();

    const MDataHandle offsetHandle = block.inputValue(offset, &status);
    if (!status)
        return status;
    const float offsetValue = offsetHandle.asFloat();

    const MPoint sourcePoint(position[0], position[1], position[2]);
    const MPoint targetPoint(target[0], target[1], target[2]);
    if (!dommy::maya::isFinitePoint(sourcePoint) || !dommy::maya::isFinitePoint(targetPoint) ||
        !std::isfinite(offsetValue)) {
        return MS::kInvalidParameter;
    }

    MFnMesh meshFn(inputMeshObject, &status);
    if (!status)
        return status;

    MPoint outputPoint = sourcePoint;
    MVector outputNormalValue = MVector::zero;
    MVector rayDirection;
    if (dommy::maya::normalizedVector(MVector(targetPoint - sourcePoint), rayDirection)) {
        MPointArray hitPoints;
        const bool intersected =
            meshFn.intersect(sourcePoint, rayDirection, hitPoints, kMFnMeshPointTolerance,
                             MSpace::kObject, nullptr, &status);
        if (!status)
            return status;

        if (intersected && hitPoints.length() > 0U) {
            double closestDistanceSquared = std::numeric_limits<double>::infinity();
            bool foundFiniteHit = false;
            for (unsigned int index = 0U; index < hitPoints.length(); ++index) {
                const MPoint& candidate = hitPoints[index];
                if (!dommy::maya::isFinitePoint(candidate))
                    continue;
                const MVector offsetFromSource = candidate - sourcePoint;
                const double distanceSquared = offsetFromSource * offsetFromSource;
                if (!std::isfinite(distanceSquared) || distanceSquared >= closestDistanceSquared) {
                    continue;
                }
                closestDistanceSquared = distanceSquared;
                outputPoint = candidate;
                foundFiniteHit = true;
            }
            if (!foundFiniteHit)
                return MS::kFailure;

            status = meshFn.getClosestNormal(outputPoint, outputNormalValue, MSpace::kObject);
            if (!status)
                return status;
            if (!dommy::maya::normalizedVector(outputNormalValue, outputNormalValue)) {
                return MS::kFailure;
            }
            outputPoint += outputNormalValue * offsetValue;
            if (!dommy::maya::isFinitePoint(outputPoint)) {
                return MS::kFailure;
            }
        }
    }

    MDataHandle positionOutput = block.outputValue(outputPosition, &status);
    if (!status)
        return status;
    MDataHandle normalOutput = block.outputValue(outputNormal, &status);
    if (!status)
        return status;

    // Neither output is changed if acquiring either handle fails.
    positionOutput.set(outputPoint.x, outputPoint.y, outputPoint.z);
    normalOutput.set(outputNormalValue.x, outputNormalValue.y, outputNormalValue.z);

    positionOutput.setClean();
    normalOutput.setClean();
    return MS::kSuccess;
}
DOMMY_COMPUTE_CATCH("DPointToSurfaceTransformNode")

MStatus DPointToSurfaceTransformNode::initialize() try {
    MStatus status;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;

    const auto createComponent = [&nAttr](MObject& attribute, const char* longName,
                                          const char* shortName, bool output) -> MStatus {
        MStatus localStatus;
        attribute = nAttr.create(longName, shortName, MFnNumericData::kDouble, 0.0, &localStatus);
        DOMMY_RETURN_IF_FAILURE(localStatus);
        DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(!output));
        DOMMY_RETURN_IF_FAILURE(nAttr.setWritable(!output));
        return nAttr.setStorable(!output);
    };
    const auto finishCompound = [&nAttr](bool output) -> MStatus {
        DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
        DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(!output));
        DOMMY_RETURN_IF_FAILURE(nAttr.setWritable(!output));
        return nAttr.setStorable(!output);
    };

    DOMMY_RETURN_IF_FAILURE(createComponent(inputPositionX, "inputPositionX", "ipx", false));
    DOMMY_RETURN_IF_FAILURE(createComponent(inputPositionY, "inputPositionY", "ipy", false));
    DOMMY_RETURN_IF_FAILURE(createComponent(inputPositionZ, "inputPositionZ", "ipz", false));
    inputPosition = nAttr.create("inputPosition", "ip", inputPositionX, inputPositionY,
                                 inputPositionZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(finishCompound(false));

    DOMMY_RETURN_IF_FAILURE(createComponent(targetDirectionX, "targetDirectionX", "tdx", false));
    DOMMY_RETURN_IF_FAILURE(createComponent(targetDirectionY, "targetDirectionY", "tdy", false));
    DOMMY_RETURN_IF_FAILURE(createComponent(targetDirectionZ, "targetDirectionZ", "tdz", false));
    targetDirection = nAttr.create("targetDirection", "td", targetDirectionX, targetDirectionY,
                                   targetDirectionZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(finishCompound(false));

    inputMesh = gAttr.create("inputMesh", "im", &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));

    offset = nAttr.create("offset", "o", MFnNumericData::kFloat, 0.0f, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));

    DOMMY_RETURN_IF_FAILURE(createComponent(outputPositionX, "outputPositionX", "opx", true));
    DOMMY_RETURN_IF_FAILURE(createComponent(outputPositionY, "outputPositionY", "opy", true));
    DOMMY_RETURN_IF_FAILURE(createComponent(outputPositionZ, "outputPositionZ", "opz", true));
    outputPosition = nAttr.create("outputPosition", "op", outputPositionX, outputPositionY,
                                  outputPositionZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(finishCompound(true));

    DOMMY_RETURN_IF_FAILURE(createComponent(outputNormalX, "outputNormalX", "onx", true));
    DOMMY_RETURN_IF_FAILURE(createComponent(outputNormalY, "outputNormalY", "ony", true));
    DOMMY_RETURN_IF_FAILURE(createComponent(outputNormalZ, "outputNormalZ", "onz", true));
    outputNormal =
        nAttr.create("outputNormal", "on", outputNormalX, outputNormalY, outputNormalZ, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    DOMMY_RETURN_IF_FAILURE(finishCompound(true));

    DOMMY_RETURN_IF_FAILURE(addAttribute(inputPosition));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputMesh));
    DOMMY_RETURN_IF_FAILURE(addAttribute(targetDirection));
    DOMMY_RETURN_IF_FAILURE(addAttribute(offset));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputPosition));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputNormal));

    const MObject inputs[] = {inputPosition, inputMesh, targetDirection};
    for (const MObject& input : inputs) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(input, outputPosition));
        DOMMY_RETURN_IF_FAILURE(attributeAffects(input, outputNormal));
    }
    DOMMY_RETURN_IF_FAILURE(attributeAffects(offset, outputPosition));

    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPointToSurfaceTransformNode", "initialize", std::current_exception());
}
