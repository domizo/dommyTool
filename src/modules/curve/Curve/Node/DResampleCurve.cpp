//
//  DResampleCurve.cpp
//  dommyTool
//
//  Created by yuzo on 2018/03/29.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DResampleCurve.h"
#include "DCurveResampleUtils.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "general_ids.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>

const MTypeId DResampleCurve::typeId(ID_CURVE_RESAMPLE_CURVE);

MObject DResampleCurve::inCurve;
MObject DResampleCurve::aSampling;
MObject DResampleCurve::outCurve;

void* DResampleCurve::creator() {
    return ::dommy::maya::createObject("DResampleCurve", [&]() {
        return new DResampleCurve();
    });
}

MStatus DResampleCurve::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outCurve) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    const MDataHandle samplingHandle = data.inputValue(aSampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSampling = samplingHandle.asInt();
    std::size_t sampleCount = 0;
    if (!dommy::curve::checkedCurveSampleCount(rawSampling, sampleCount)) {
        MGlobal::displayError("DResampleCurve: Sampling is outside the supported range.");
        return MS::kInvalidParameter;
    }

    MDataHandle input = data.inputValue(inCurve, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (input.type() != MFnData::kNurbsCurve || input.data().isNull()) {
        MGlobal::displayError("DResampleCurve: inCurve must contain a NURBS curve.");
        return MS::kInvalidParameter;
    }

    MObject newCurveData;
    status = dommy::curve::buildLengthResampledCurve(input.data(), sampleCount, newCurveData);
    if (status != MS::kSuccess) {
        MGlobal::displayError("DResampleCurve: unable to resample the curve.");
        return status;
    }

    MDataHandle output = data.outputValue(outCurve, &status);
    if (status != MS::kSuccess) {
        return status;
    }
    status = output.set(newCurveData);
    if (status != MS::kSuccess) {
        return status;
    }
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DResampleCurve")

MStatus DResampleCurve::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    inCurve = gAttr.create("inCurve", "inc", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inCurve));

    aSampling = nAttr.create("Sampling", "smp", MFnNumericData::kInt, 12, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(8));
    DOMMY_RETURN_IF_FAILURE(
        nAttr.setMax(static_cast<int>(dommy::generation::kMaxCurveSampleCount)));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSampling));

    outCurve = tAttr.create("outCurve", "oc", MFnData::kNurbsCurve, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outCurve));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inCurve, outCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outCurve));

    return MS::kSuccess;
}
