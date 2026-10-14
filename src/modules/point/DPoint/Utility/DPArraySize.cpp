//
//  DPArraySize.cpp
//  dommyTool
//
//  Created by yuzo on 3/5/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPArraySize.h"
#include "common/utility/DComputeGuard.h"

#include "DPointCommon.h"
#include "DCurveData.h"
#include "DMeshData.h"
#include "dpoint_ids.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DStatusOperation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

const MTypeId DPArraySize::typeId(ID_DP_ARRAY_SIZE);

MObject DPArraySize::inputPointArray;
MObject DPArraySize::inDD;

MObject DPArraySize::inObject;
MObject DPArraySize::aCountType;
MObject DPArraySize::resultCount;

void* DPArraySize::creator() {
    return ::dommy::maya::createObject("DPArraySize", [&]() {
        return new DPArraySize();
    });
}

MStatus DPArraySize::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != resultCount)
        return MS::kUnknownParameter;
    MStatus status;
    std::size_t count = 0U;
    const MPlug customPlug(thisMObject(), inDD);
    const MPlug pointPlug(thisMObject(), inputPointArray);
    const MPlug objectPlug(thisMObject(), inObject);
    const bool customConnected = customPlug.isConnected(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (customConnected) {
        const MDataHandle input = data.inputValue(inDD, &status);
        if (!status)
            return status;
        const MTypeId type = input.typeId();
        if (type == DPData::id) {
            const DPData* points = nullptr;
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::getPluginDataFromHandle(input, DPData::id, points));
            if (!points || !points->hasValidPointTopology())
                return MS::kInvalidParameter;
            count = points->numberOfPoints();
        } else if (type == DCurveData::id || type == DMeshData::id) {
            const MDataHandle countTypeHandle = data.inputValue(aCountType, &status);
            if (!status)
                return status;
            const short countType = countTypeHandle.asShort();
            if (countType < 0 || countType > 1)
                return MS::kInvalidParameter;
            if (type == DCurveData::id) {
                const DCurveData* curves = nullptr;
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::getPluginDataFromHandle(input, DCurveData::id, curves));
                if (!curves || !curves->hasValidCurveTopology())
                    return MS::kInvalidParameter;
                if (countType == 0) {
                    if (!curves->getCheckedTotalCvs(count))
                        return MS::kInvalidParameter;
                } else
                    count = curves->numberOfCurves();
            } else {
                const DMeshData* meshes = nullptr;
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::getPluginDataFromHandle(input, DMeshData::id, meshes));
                if (!meshes || !meshes->hasValidMeshTopology())
                    return MS::kInvalidParameter;
                if (countType == 0) {
                    if (!meshes->getCheckedTotalVertex(count))
                        return MS::kInvalidParameter;
                } else
                    count = meshes->numberOfMesh();
            }
        } else
            return MS::kInvalidParameter;
    } else {
        const bool pointConnected = pointPlug.isConnected(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (pointConnected) {
            MPointArray points;
            DOMMY_RETURN_IF_FAILURE(getPointArrayFromData(data, inputPointArray, points));
            count = points.length();
        } else {
            const bool objectConnected = objectPlug.isConnected(&status);
            DOMMY_RETURN_IF_FAILURE(status);
            if (objectConnected) {
                MDataHandle input = data.inputValue(inObject, &status);
                if (!status) return status;
                const MFnData::Type type = input.type();
                const MObject object = input.data();
                if (object.isNull()) return MS::kFailure;
                if (type == MFnData::kNurbsCurve) {
                    MFnNurbsCurve curve(object, &status);
                    if (!status) return status;
                    count = curve.numCVs(&status);
                    if (!status) return status;
                } else if (type == MFnData::kNurbsSurface) {
                    MFnNurbsSurface surface(object, &status);
                    if (!status) return status;
                    MPointArray cvs;
                    DOMMY_RETURN_IF_FAILURE(surface.getCVs(cvs));
                    count = cvs.length();
                } else if (type == MFnData::kMesh) {
                    MFnMesh mesh(object, &status);
                    if (!status) return status;
                    count = mesh.numVertices(&status);
                    if (!status) return status;
                } else {
                    return MS::kInvalidParameter;
                }
            }
        }
    }
    // No connected input still represents a valid empty count.
    MInt64 result = 0;
    if (!dommy::numeric::checkedNumericCast(count, result))
        return MS::kInvalidParameter;
    MDataHandle output = data.outputValue(resultCount, &status);
    if (!status)
        return status;
    output.setInt64(result);
    output.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPArraySize")

MStatus DPArraySize::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    MFnGenericAttribute outAttr;

    // ----------------------- INPUTS --------------------------
    inDD = gAttr.create("inDD", "idd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));

    inObject = gAttr.create("inObject", "ino", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMesh));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsSurface));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inObject));

    inputPointArray = gAttr.create("inputPointArray", "ipa", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputPointArray));

    MFnEnumAttribute enumFn;
    aCountType = enumFn.create("CountType", "cnttyp", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Point", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Primitive", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCountType));

    resultCount = nAttr.create("resultCount", "rc", MFnNumericData::kInt64, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setWritable(false));

    DOMMY_RETURN_IF_FAILURE(addAttribute(resultCount));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDD, resultCount));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inputPointArray, resultCount));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inObject, resultCount));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCountType, resultCount));

    return MS::kSuccess;
}
