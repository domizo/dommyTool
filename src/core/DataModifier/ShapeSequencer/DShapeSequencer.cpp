//
//  DShapeSequencer.cpp
//  dommyTool
//
//  Created by yuzo on 4/29/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DShapeSequencer.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnTypedAttribute.h>

#include <maya/MDataHandle.h>
#include <maya/MArrayDataHandle.h>
#include <maya/MFnData.h>
#include <maya/MGlobal.h>

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsCurveData.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnNurbsSurfaceData.h>

#include <vector>
#include <algorithm>
#include <cmath>

const MTypeId DShapeSequencer::typeId(0x0080A);

MObject DShapeSequencer::inputObjects;

MObject DShapeSequencer::seaquence;

MObject DShapeSequencer::outputObject;

void* DShapeSequencer::creator() {
    return ::dommy::maya::createObject("DShapeSequencer", [&]() {
        return new DShapeSequencer();
    });
}

MStatus DShapeSequencer::compute(const MPlug& plug, MDataBlock& block) try {
    MStatus stat;

    if (plug == outputObject) {
        const MDataHandle sequenceHandle = block.inputValue(seaquence, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const double sequenceVal = sequenceHandle.asDouble();
        if (!std::isfinite(sequenceVal) || sequenceVal < 0.0 || sequenceVal > 1.0) {
            MGlobal::displayError(
                "DShapeSequencer sequence must be finite and between zero and one.");
            return MS::kFailure;
        }

#pragma mark input Object 入力オブジェクトの取得
        std::vector<MObject> inputObjectArray;

        MArrayDataHandle arrayHandle = block.inputArrayValue(inputObjects, &stat);
        if (!stat)
            return stat;

        const unsigned int count = arrayHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (static_cast<std::size_t>(count) > dommy::generation::kMaxConnectedGeometryCount) {
            MGlobal::displayError("DShapeSequencer input count exceeds the safety limit.");
            return MS::kFailure;
        }
        inputObjectArray.reserve(static_cast<std::size_t>(count));

        MFnData::Type baseDataType = MFnData::kInvalid;

        for (unsigned int i = 0; i < count; ++i) {
            stat = arrayHandle.jumpToArrayElement(i);
            if (!stat)
                return stat;
            MDataHandle dupObjHnd = arrayHandle.inputValue(&stat);
            if (!stat)
                return stat;
            const MObject inputObject = dupObjHnd.data();
            if (inputObject.isNull()) {
                MGlobal::displayError("DShapeSequencer received an empty input object.");
                return MS::kInvalidParameter;
            }
            if (i == 0) {
                baseDataType = dupObjHnd.type();
            } else {
                if (baseDataType != dupObjHnd.type()) {
                    MGlobal::displayError("DShapeSequencer input object types must match.");
                    return MS::kInvalidParameter;
                }
            }

            inputObjectArray.push_back(inputObject);
        }

#pragma mark Get Output data index 出力オブジェクトのindexの取得
        MObject outputDataObject;

        if (inputObjectArray.size() == 0) {
            MGlobal::displayWarning("No input object!");
            return MS::kFailure;
        } else if (inputObjectArray.size() == 1) {

            outputDataObject = inputObjectArray[0];

        } else if (inputObjectArray.size() == 2) {

            outputDataObject = sequenceVal < 0.5 ? inputObjectArray[0] : inputObjectArray[1];

        } else {
            const std::size_t outIndex =
                std::min(static_cast<std::size_t>(std::llround(
                             static_cast<double>(inputObjectArray.size() - 1U) * sequenceVal)),
                         inputObjectArray.size() - 1U);
            outputDataObject = inputObjectArray.at(outIndex);
        }

#pragma mark Create Output data 出力データの作成
        MDataHandle outputHnd = block.outputValue(outputObject, &stat);
        if (!stat)
            return stat;

        if (baseDataType == MFnData::kNurbsCurve) {
            //			MObject crv = outputHnd.asNurbsCurve();
            MFnNurbsCurveData crvDataFn;
            MObject newOutCrvData = crvDataFn.create(&stat);
            if (!stat)
                return stat;

            MFnNurbsCurve newCrvFn;
            newCrvFn.copy(outputDataObject, newOutCrvData, &stat);
            if (!stat)
                return stat;

            stat = outputHnd.set(newOutCrvData);
            if (!stat)
                return stat;
        } else if (baseDataType == MFnData::kNurbsSurface) {
            //			MObject nurbs = outputHnd.asNurbsSurface();
            MFnNurbsSurfaceData nurbsDataFn;
            MObject newOutNurbsData = nurbsDataFn.create(&stat);
            if (!stat)
                return stat;

            MFnNurbsSurface newSurfFn;
            newSurfFn.copy(outputDataObject, newOutNurbsData, &stat);
            if (!stat)
                return stat;

            stat = outputHnd.set(newOutNurbsData);
            if (!stat)
                return stat;
        } else if (baseDataType == MFnData::kMesh) {
            //			MObject mesh = outputHnd.asMesh();
            MFnMeshData meshDataFn;
            MObject newOutMeshData = meshDataFn.create(&stat);
            if (!stat)
                return stat;

            MFnMesh newMeshFn;
            newMeshFn.copy(outputDataObject, newOutMeshData, &stat);
            if (!stat)
                return stat;

            stat = outputHnd.set(newOutMeshData);
            if (!stat)
                return stat;

        } else {
            MGlobal::displayWarning("Input object is not support type");
            return MS::kFailure;
        }

        outputHnd.setClean();

        return block.setClean(plug);

    } else {
        stat = MS::kUnknownParameter;
    }

    return stat;
}
DOMMY_COMPUTE_CATCH("DShapeSequencer")

MStatus DShapeSequencer::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;

    inputObjects = gAttr.create("inputObjects", "iobj", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputObjects));

    seaquence = nAttr.create("seaquence", "seq", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(seaquence));

    MFnGenericAttribute outAttr;
    outputObject = outAttr.create("outputObject", "oobj", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(outAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(outAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(outAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(outAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(outAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outputObject));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputObjects, outputObject));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(seaquence, outputObject));

    return MS::kSuccess;
}
