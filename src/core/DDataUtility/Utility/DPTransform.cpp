//
//  DPTransform.cpp
//  dommyTool
//
//  Created by yuzo on 3/25/15.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPTransform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DPointPayloadBudget.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DPointDataAccess.h"
#include "dpoint_ids.h"
#include "DCurveData.h"
#include "DCurveUtil.h"
#include "DMeshData.h"
#include "DMeshUtil.h"
#include "dmesh_ids.h"

#include <cmath>

#pragma mark DTransformManip

MTypeId DPTransformManip::id(ID_DP_TRANSFORM_MANIP);

void* DPTransformManip::creator() {
    return ::dommy::maya::createObject("DPTransformManip", [&]() {
        return new DPTransformManip();
    });
}

MStatus DPTransformManip::initialize() {
    return MPxManipContainer::initialize();
}

MStatus DPTransformManip::createChildren() {
    fTranslateManip = addFreePointTriadManip("translateManip", "trans");
    fRotateManip = addRotateManip("rotateManip", "rot");
    fScaleManip = addScaleManip("scaleManip", "scale");

    if (!fTranslateManip.isValid() || !fRotateManip.isValid() || !fScaleManip.isValid()) {
        return MS::kFailure;
    }
    return MS::kSuccess;
}

MStatus DPTransformManip::connectToDependNode(const MObject& node) {
    MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status)
        return status;

    MFnFreePointTriadManip transManipFn(fTranslateManip, &status);
    if (!status)
        return status;
    MPlug transPlug = nodeFn.findPlug("Translate", true, &status);
    if (!status)
        return status;
    status = transManipFn.connectToPointPlug(transPlug);
    if (!status)
        return status;

    MPlug rotatePlug = nodeFn.findPlug("Rotate", true, &status);
    if (!status)
        return status;
    MPlug rotatePivotPlug = nodeFn.findPlug("RotatePivot", true, &status);
    if (!status)
        return status;

    double rotation[3] = {0.0, 0.0, 0.0};
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        MPlug child = rotatePlug.child(axis, &status);
        if (!status)
            return status;
        status = child.getValue(rotation[axis]);
        if (!status || !std::isfinite(rotation[axis])) {
            return MS::kFailure;
        }
    }

    MFnRotateManip rotateManipFn(fRotateManip, &status);
    if (!status)
        return status;
    status = rotateManipFn.connectToRotationPlug(rotatePlug);
    if (!status)
        return status;
    status = rotateManipFn.connectToRotationCenterPlug(rotatePivotPlug);
    if (!status)
        return status;
    status =
        rotateManipFn.setInitialRotation(MEulerRotation(rotation[0], rotation[1], rotation[2]));
    if (!status)
        return status;
    status = rotateManipFn.setRotateMode(MFnRotateManip::kWorldSpace);
    if (!status)
        return status;
    status = rotateManipFn.setSnapMode(false);
    if (!status)
        return status;

    MFnScaleManip scaleManipFn(fScaleManip, &status);
    if (!status)
        return status;
    MPlug scalePlug = nodeFn.findPlug("Scale", true, &status);
    if (!status)
        return status;
    status = scaleManipFn.connectToScalePlug(scalePlug);
    if (!status)
        return status;
    status = scaleManipFn.connectToScaleCenterPlug(transPlug);
    if (!status)
        return status;

    status = finishAddingManips();
    if (!status)
        return status;
    return MPxManipContainer::connectToDependNode(node);
}

#pragma mark DPTransform

MTypeId DPTransform::typeId(ID_DP_TRANSFORM);

MObject DPTransform::inDD;

MObject DPTransform::aTranslate;
MObject DPTransform::aRotateX;
MObject DPTransform::aRotateY;
MObject DPTransform::aRotateZ;
MObject DPTransform::aRotate;
MObject DPTransform::aRotatePivot;
MObject DPTransform::aScale;

MObject DPTransform::transformMatrix;

MObject DPTransform::outDD;

void* DPTransform::creator() {
    return ::dommy::maya::createObject("DPTransform", [&]() {
        return new DPTransform();
    });
}

MStatus DPTransform::process(const std::vector<DPointList*>& points, MDataBlock& data) {
    MStatus status;
    const MPlug matPlug(thisMObject(), transformMatrix);

    MMatrix transMat;
    const bool connected = matPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!connected) {
        MDataHandle translateHandle = data.inputValue(aTranslate, &status);
        if (!status)
            return status;
        MDataHandle rotateHandle = data.inputValue(aRotate, &status);
        if (!status)
            return status;
        MDataHandle rotatePivotHandle = data.inputValue(aRotatePivot, &status);
        if (!status)
            return status;
        MDataHandle scaleHandle = data.inputValue(aScale, &status);
        if (!status)
            return status;

        const double3& transVal = translateHandle.asDouble3();
        const double3& rotVal = rotateHandle.asDouble3();
        const double3& rotPivotVal = rotatePivotHandle.asDouble3();
        const double3& scaleVal = scaleHandle.asDouble3();

        const MVector translation(transVal[0], transVal[1], transVal[2]);
        const MVector rotation(rotVal[0], rotVal[1], rotVal[2]);
        const MPoint rotatePivot(rotPivotVal[0], rotPivotVal[1], rotPivotVal[2]);
        const MVector scale(scaleVal[0], scaleVal[1], scaleVal[2]);
        if (!dommy::maya::isFiniteVector(translation) || !dommy::maya::isFiniteVector(rotation) ||
            !dommy::maya::isFinitePoint(rotatePivot) || !dommy::maya::isFiniteVector(scale)) {
            return MS::kFailure;
        }

        MTransformationMatrix transformMat;
        status = transformMat.setScale(scaleVal, MSpace::kWorld);
        if (!status)
            return status;
        status = transformMat.setRotatePivot(rotatePivot, MSpace::kWorld, true);
        if (!status)
            return status;
        status = transformMat.setRotation(rotVal, MTransformationMatrix::kZYX);
        if (!status)
            return status;
        status = transformMat.setTranslation(translation, MSpace::kWorld);
        if (!status)
            return status;

        transMat = transformMat.asMatrix();
    } else {
        MDataHandle matrixHandle = data.inputValue(transformMatrix, &status);
        if (!status)
            return status;
        transMat = matrixHandle.asMatrix();
    }

    if (!dommy::maya::isFiniteMatrix(transMat)) {
        return MS::kFailure;
    }

    for (DPointList* pointList : points) {
        if (!pointList)
            return MS::kFailure;
        for (DPoint& point : *pointList) {
            DReal* values = &point[0];
            const MPoint transformed = MPoint(values[0], values[1], values[2]) * transMat;
            if (!dommy::maya::isFloatRepresentablePoint(transformed)) {
                return MS::kFailure;
            }
            values[0] = static_cast<DReal>(transformed.x);
            values[1] = static_cast<DReal>(transformed.y);
            values[2] = static_cast<DReal>(transformed.z);
        }
    }
    return MS::kSuccess;
}

MStatus DPTransform::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDD) {
        return MS::kUnknownParameter;
    }

    MStatus stat;
    MDataHandle inputHandle = data.inputValue(inDD, &stat);
    if (!stat)
        return stat;
    const MObject inputObject = inputHandle.data();
    if (inputObject.isNull()) {
        EchoWarning(name().asChar(), "Input data is null.");
        return MS::kFailure;
    }

    const unsigned int inputType = inputHandle.typeId().id();
    if (inputType == ID_DP_DATA) {
        const DPData* inputData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDD, data, inputData));
        if (!inputData) return MS::kFailure;

        dommy::point::PointPayloadBudget workspace;
        dommy::point::preflightPointPayload(*inputData, sizeof(DPoint), workspace);
        workspace.add(1U, sizeof(DPointList*));
        DPointList points(*inputData->getPositionPtrConst());
        const std::vector<DPointList*> pointsList{&points};
        stat = process(pointsList, data);
        if (!stat)
            return stat;
        stat = dommy::maya::setPluginDataOutput<DPData>(
            data, outDD, DPData::id, [inputData, &points](DPData& output) -> MStatus {
                return output.copyRepeatedFrom(*inputData, 1U,
                    [&points](DPoint& point, std::size_t, std::size_t index) -> MStatus {
                        point = points[index];
                        return MS::kSuccess;
                    });
            });
        return stat ? data.setClean(plug) : stat;
    }

    if (inputType == ID_DC_DATA) {
        const DCurveData* inputData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDD, data, inputData));
        if (!inputData) return MS::kFailure;

        MFnPluginData pluginData;
        const MObject outputObject = pluginData.create(DCurveData::id, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (outputObject.isNull()) return MS::kFailure;

        DCurveData* outputData = nullptr;
        stat = dommy::maya::getPluginData(pluginData, outputData);
        if (!stat)
            return stat;
        DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*inputData));

        dommy::geometry::StagedPositionEdits<DCurveInternalData>
            positionEdits;
        if (!positionEdits.reserve(outputData->numberOfCurves())) {
            return MS::kFailure;
        }
        for (auto curve = outputData->beginCurves(); curve != outputData->endCurves(); ++curve) {
            if (!*curve || !positionEdits.add(**curve))
                return MS::kFailure;
        }
        std::vector<DPointList*> pointsList;
        if (!positionEdits.mutablePositionPointers(pointsList)) {
            return MS::kFailure;
        }
        stat = process(pointsList, data);
        if (!stat)
            return stat;
        if (!positionEdits.commit()) {
            return MS::kFailure;
        }
        outputData->updateBBox();

        MDataHandle output = data.outputValue(outDD, &stat);
        if (!stat)
            return stat;
        stat = dommy::maya::publishPluginDataObject(
            output, pluginData.object(), *outputData);
        if (!stat)
            return stat;
        return data.setClean(plug);
    }

    if (inputType == ID_DM_DATA) {
        const DMeshData* inputData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDD, data, inputData));
        if (!inputData) return MS::kFailure;

        MFnPluginData pluginData;
        const MObject outputObject = pluginData.create(DMeshData::id, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (outputObject.isNull()) return MS::kFailure;

        DMeshData* outputData = nullptr;
        stat = dommy::maya::getPluginData(pluginData, outputData);
        if (!stat)
            return stat;
        DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*inputData));

        dommy::geometry::StagedPositionEdits<DMeshInternalData>
            positionEdits;
        if (!positionEdits.reserve(outputData->numberOfMesh())) {
            return MS::kFailure;
        }
        for (auto mesh = outputData->beginMeshes(); mesh != outputData->endMeshes(); ++mesh) {
            if (!*mesh || !positionEdits.add(**mesh))
                return MS::kFailure;
        }
        std::vector<DPointList*> pointsList;
        if (!positionEdits.mutablePositionPointers(pointsList)) {
            return MS::kFailure;
        }
        stat = process(pointsList, data);
        if (!stat)
            return stat;
        if (!positionEdits.commit()) {
            return MS::kFailure;
        }
        outputData->updateBBox();

        MDataHandle output = data.outputValue(outDD, &stat);
        if (!stat)
            return stat;
        stat = dommy::maya::publishPluginDataObject(
            output, pluginData.object(), *outputData);
        if (!stat)
            return stat;
        return data.setClean(plug);
    }

    EchoWarning(name().asChar(), "Input data type is not supported.");
    return MS::kFailure;
}
DOMMY_COMPUTE_CATCH("DPTransform")

MStatus DPTransform::initialize() {
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnMatrixAttribute matAttr;

    MStatus stat;

    inDD = gAttr.create("inDD", "idd", &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));

    aTranslate = nAttr.create("Translate", "trans", MFnNumericData::k3Double, 0.0, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslate));

    MFnUnitAttribute uniAttr;
    aRotateX = uniAttr.create("RotateX", "rotx", MFnUnitAttribute::kAngle, 0.0, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(uniAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(uniAttr.setStorable(true));
    aRotateY = uniAttr.create("RotateY", "roty", MFnUnitAttribute::kAngle, 0.0, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(uniAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(uniAttr.setStorable(true));
    aRotateZ = uniAttr.create("RotateZ", "rotz", MFnUnitAttribute::kAngle, 0.0, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(uniAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(uniAttr.setStorable(true));

    aRotate = nAttr.create("Rotate", "rot", aRotateX, aRotateY, aRotateZ, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotate));

    aRotatePivot = nAttr.create("RotatePivot", "rotp", MFnNumericData::k3Double, 0.0, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotatePivot));

    aScale = nAttr.create("Scale", "sc", MFnNumericData::k3Double, 1.0, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    transformMatrix = matAttr.create("transformMatrix", "tm", MFnMatrixAttribute::kDouble, &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(matAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(matAttr.setReadable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(matAttr.setCached(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(transformMatrix));

    outDD = gAttr.create("outDD", "odd", &stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTranslate, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotate, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRotatePivot, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDD));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(transformMatrix, outDD));
    return DPTransformManip::addToManipConnectTable(typeId);
}
