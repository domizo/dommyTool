//
//  DCUVMap.cpp
//  dommyTool
//
//  Created by yuzo on 1/29/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCUVMap.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DCurveUVMapping.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DDataUtility.h"



const MTypeId DCUVMap::typeId( ID_DC_UVMAP );


MObject DCUVMap::inDC;

MObject DCUVMap::aUVAttributeName;

MObject DCUVMap::aUVType;
MObject DCUVMap::aCalcType;

MObject DCUVMap::aUVRamp;
MObject DCUVMap::aUVRange;

MObject DCUVMap::aOffset;
MObject DCUVMap::aScale;

MObject DCUVMap::outDC;



void *DCUVMap::creator()
{
    return ::dommy::maya::createObject("DCUVMap", [&]() {
        return new DCUVMap();
    });
}



void DCUVMap::postConstructor() {
    dommy::maya::initializeNodeRamps("DCUVMap", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aUVRamp,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        return MS::kSuccess;
    });
}


MStatus DCUVMap::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const DCurveData* inputCurves = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputCurves));

    const MDataHandle nameHandle = data.inputValue(aUVAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string attributeName = nameHandle.asString().asChar();

    const MDataHandle mappingHandle = data.inputValue(aUVType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short mappingType = mappingHandle.asShort();
    const MDataHandle calculationHandle = data.inputValue(aCalcType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short calculationType = calculationHandle.asShort();
    const MDataHandle rangeHandle = data.inputValue(aUVRange, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double2& range = rangeHandle.asDouble2();
    const MDataHandle scaleHandle = data.inputValue(aScale, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double scaleValue = scaleHandle.asDouble();
    const MDataHandle offsetHandle = data.inputValue(aOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double offsetValue = offsetHandle.asDouble();

    dommy::curve::UVMappingPlan plan;
    if (!dommy::curve::buildUVMappingPlan(
            mappingType, calculationType, range[0], range[1], scaleValue,
            offsetValue, plan)) {
        MGlobal::displayError("DCUVMap: mapping settings must be valid and finite.");
        return MS::kInvalidParameter;
    }

    MObject outputObject;
    DCurveData* outputCurves = NULL;
    status = dommy::maya::createPluginDataObject<DCurveData>(
        DCurveData::id, outputObject, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputCurves) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*inputCurves));
    if (!outputCurves->hasValidCurveTopology() ||
        outputCurves->numberOfCurves() != inputCurves->numberOfCurves()) {
        return MS::kFailure;
    }

    if (!attributeName.empty() && outputCurves->numberOfCurves() != 0U) {
        MRampAttribute ramp;
        if (plan.useRamp) {
            ramp = MRampAttribute(MPlug(thisMObject(), aUVRamp), &status);
            CHECK_MSTATUS_AND_RETURN_IT(status);
        }

        checkDCurvePointAttribute(attributeName, *outputCurves, true);
        for (DCurveData::DCurveIter curve = outputCurves->beginCurves();
             curve != outputCurves->endCurves(); ++curve) {
            if (!*curve || !(*curve)->hasValidCurveTopology()) {
                return MS::kInvalidParameter;
            }

            const std::size_t pointCount = (*curve)->numberOfPoints();
            DScalarDataList values(pointCount, 0.0);
            MFnNurbsCurve curveFunction;
            MPointArray cvs;
            double totalLength = 0.0;
            if (plan.useArcLength) {
                MObject curveObject;
                status = generateMCurveFromDcurve(*curve, curveObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                status = curveFunction.setObject(curveObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                totalLength = curveFunction.length(kMFnNurbsEpsilon, &status);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                status = curveFunction.getCVs(cvs, MSpace::kObject);
                CHECK_MSTATUS_AND_RETURN_IT(status);
                if (cvs.length() != pointCount) return MS::kFailure;
            }

            for (std::size_t index = 0U; index < pointCount; ++index) {
                double position = 0.0;
                if (plan.useArcLength) {
                    double parameter = 0.0;
                    curveFunction.closestPoint(
                        cvs[static_cast<unsigned int>(index)], &parameter,
                        kMFnNurbsEpsilon, MSpace::kObject, &status);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    const double partialLength =
                        curveFunction.findLengthFromParam(parameter, &status);
                    CHECK_MSTATUS_AND_RETURN_IT(status);
                    if (!dommy::curve::arcLengthUVPosition(
                            partialLength, totalLength, position)) {
                        return MS::kInvalidParameter;
                    }
                } else if (!dommy::curve::pointUVPosition(
                               index, pointCount, position)) {
                    return MS::kInvalidParameter;
                }

                double normalizedValue = position;
                if (plan.useRamp) {
                    float rampValue = 0.0f;
                    if (!dommy::maya::sampleFiniteRamp(
                            ramp, position, rampValue)) {
                        return MS::kFailure;
                    }
                    normalizedValue = rampValue;
                }
                double mappedValue = 0.0;
                if (!dommy::curve::mapUVValue(
                        plan, normalizedValue, mappedValue)) {
                    return MS::kInvalidParameter;
                }
                if (!dommy::numeric::checkedFloatCast(
                        mappedValue, values[index])) {
                    return MS::kInvalidParameter;
                }
            }
            (*curve)->addAttrWithValues(attributeName, values);
            if (!(*curve)->hasValidCurveTopology()) return MS::kFailure;
        }
    }

    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(
            output, outputObject, *outputCurves));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCUVMap")




MStatus DCUVMap::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    const MObject aUVAttributeNameDefault = fnStringData.create("u", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aUVAttributeName =
        tAttr.create("UVAttributeName", "uvattr", MFnData::kString, aUVAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVAttributeName));

    MFnEnumAttribute uvEnumFn;
    aUVType = uvEnumFn.create("UVType", "uvty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(uvEnumFn.addField("Length Linear", 0));
    DOMMY_RETURN_IF_FAILURE(uvEnumFn.addField("From Ramp", 1));
    DOMMY_RETURN_IF_FAILURE(uvEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVType));

    MFnEnumAttribute calcEnumFn;
    aCalcType = calcEnumFn.create("CalcType", "calty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(calcEnumFn.addField("from Point", 0));
    DOMMY_RETURN_IF_FAILURE(calcEnumFn.addField("from Param(very slow, accurate)", 1));
    DOMMY_RETURN_IF_FAILURE(calcEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCalcType));

    aUVRamp = MRampAttribute::createCurveRamp("UVRamp", "uvrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVRamp));

    aUVRange = nAttr.create("UVRange", "uvrng", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUVRange));

    aOffset = nAttr.create("Offset", "off", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    aScale = nAttr.create("Scale", "scl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCalcType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVRange, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aUVRamp, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aScale, outDC));
    return MS::kSuccess;
}
