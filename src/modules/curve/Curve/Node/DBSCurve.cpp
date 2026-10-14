//
//  DBSCurve.cpp
//  dommyTool
//
//  Created by yuzo on 2018/03/27.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DBSCurve.h"
#include "DCurveResampleUtils.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DStatusOperation.h"
#include "general_ids.h"

#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnGenericAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>

const MTypeId DBSCurve::typeId(ID_CURVE_BLEND_SHAPE_CURVE);

MObject DBSCurve::inBaseCurve;
MObject DBSCurve::inTargetCurve;

MObject DBSCurve::aBlend;
MObject DBSCurve::aSampling;

MObject DBSCurve::outCurve;

void* DBSCurve::creator() {
    return ::dommy::maya::createObject("DBSCurve", [&]() {
        return new DBSCurve();
    });
}

MStatus DBSCurve::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outCurve) {
        return MS::kUnknownParameter;
    }

    MStatus status;
    double blend = 0.0;
    status = dommy::maya::readFiniteDoubleInput(data, aBlend, blend);
    if (status != MS::kSuccess) {
        MGlobal::displayError("DBSCurve: unable to read a finite Blend value.");
        return status;
    }

    const MDataHandle samplingHandle = data.inputValue(aSampling, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawSampling = samplingHandle.asInt();
    std::size_t sampleCount = 0;
    if (!dommy::curve::checkedCurveSampleCount(rawSampling, sampleCount)) {
        MGlobal::displayError("DBSCurve: Sampling is outside the supported range.");
        return MS::kInvalidParameter;
    }

    MDataHandle baseCurveHandle = data.inputValue(inBaseCurve, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (baseCurveHandle.type() != MFnData::kNurbsCurve || baseCurveHandle.data().isNull()) {
        MGlobal::displayError("DBSCurve: inBaseCurve must contain a NURBS curve.");
        return MS::kInvalidParameter;
    }
    const MObject baseCurveObject = baseCurveHandle.data();

    MDataHandle targetCurveHandle = data.inputValue(inTargetCurve, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (targetCurveHandle.type() != MFnData::kNurbsCurve || targetCurveHandle.data().isNull()) {
        MGlobal::displayError("DBSCurve: inTargetCurve must contain a NURBS curve.");
        return MS::kInvalidParameter;
    }
    const MObject targetCurveObject = targetCurveHandle.data();

    MObject newCurveData;
    status = dommy::curve::buildLengthBlendedCurve(baseCurveObject, targetCurveObject, blend,
                                                   sampleCount, newCurveData);
    if (status != MS::kSuccess) {
        MGlobal::displayError("DBSCurve: unable to build the blended curve.");
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
DOMMY_COMPUTE_CATCH("DBSCurve")

MStatus DBSCurve::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;

    inBaseCurve = gAttr.create("inBaseCurve", "inbc", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inBaseCurve));

    inTargetCurve = gAttr.create("inTargetCurve", "intc", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNurbsCurve));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inTargetCurve));

    aBlend = nAttr.create("Blend", "bld", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBlend));

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

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inBaseCurve, outCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inTargetCurve, outCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBlend, outCurve));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSampling, outCurve));

    return MS::kSuccess;
}
