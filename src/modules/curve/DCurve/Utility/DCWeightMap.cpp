//
//  DCWeightMap.cpp
//  dommyTool
//
//  Created by yuzo on 2017/02/22.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCWeightMap.h"
#include "common/utility/DMayaDefaultRamp.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"





#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "DDataUtility.h"

#include <cmath>
#include <limits>
#include "DDataOp.h"
#include "common/utility/DMayaRampSampling.h"
#include "common/utility/DSignalMath.h"


const MTypeId DCWeightMap::typeId( ID_DC_WEIGHT_MAP );


MObject DCWeightMap::inDC;

MObject DCWeightMap::aParamType;
MObject DCWeightMap::aBaseParam;
MObject DCWeightMap::aParamAttributeName;

MObject DCWeightMap::aDistance;
MObject DCWeightMap::aOffset;
MObject DCWeightMap::aWrapMode;

MObject DCWeightMap::aWeightAttributeName;
MObject DCWeightMap::aWeightRamp;
MObject DCWeightMap::aWeightScale;
MObject DCWeightMap::aRepeatWeight;

MObject DCWeightMap::outDC;


void *DCWeightMap::creator()
{
    return ::dommy::maya::createObject("DCWeightMap", [&]() {
        return new DCWeightMap();
    });
}

void DCWeightMap::postConstructor() {
    dommy::maya::initializeNodeRamps("DCWeightMap", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aWeightRamp,
                                                                    {0.0f, 1.0f}, {1.0f, 0.0f}));
        return MS::kSuccess;
    });
}


MStatus DCWeightMap::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;
    const DCurveData* input = nullptr;
    MStatus status = getValidInputDC(inDC, data, input);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MDataHandle handle = data.inputValue(aParamType, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short paramType = handle.asShort();
    handle = data.inputValue(aBaseParam, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double baseParam = handle.asFloat();
    handle = data.inputValue(aParamAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string paramAttribute = handle.asString().asChar();
    handle = data.inputValue(aDistance, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double affectDist = handle.asFloat();
    handle = data.inputValue(aOffset, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double offset = handle.asFloat();
    handle = data.inputValue(aWrapMode, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const short wrapMode = handle.asShort();
    handle = data.inputValue(aWeightAttributeName, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    std::string weightAttribute = handle.asString().asChar();
    handle = data.inputValue(aWeightScale, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double weightScale = handle.asFloat();
    handle = data.inputValue(aRepeatWeight, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool repeatWeight = handle.asBool();
    if (paramType < 0 || paramType > 1 || wrapMode < 0 || wrapMode > 2 ||
        !std::isfinite(baseParam) || !std::isfinite(offset) || !std::isfinite(weightScale) ||
        !std::isfinite(affectDist) || affectDist <= std::numeric_limits<float>::epsilon())
        return MS::kInvalidParameter;
    if (weightAttribute.empty()) weightAttribute = "weight";

    std::vector<double> parameters(input->numberOfCurves(), baseParam);
    if (paramType == 1 && input->isExistAttr(paramAttribute) &&
        !dommy::attribute::convertScalarAttribute(input->getAttr(paramAttribute),
            input->numberOfCurves(), parameters)) return MS::kInvalidParameter;

    MFnPluginData pluginData;
    DCurveData* outputCurves = nullptr;
    status = dommy::maya::createPluginData(pluginData, DCurveData::id, outputCurves);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!outputCurves) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(outputCurves->copyFrom(*input));
    MPlug rampPlug(thisMObject(), aWeightRamp);
    MRampAttribute ramp(rampPlug, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    std::size_t index = 0U;
    for (auto curve = outputCurves->beginCurves(); curve != outputCurves->endCurves(); ++curve, ++index) {
        double position = 0.0;
        if (!dommy::signal::weightMapPosition(parameters[index], offset, wrapMode, position))
            return MS::kInvalidParameter;
        DScalarDataList weights((*curve)->numberOfPoints(), 0.0f);
        for (std::size_t point = 0U; point < weights.size(); ++point) {
            double distance = 0.0;
            float value = 0.0f;
            if (!dommy::signal::weightMapDistance(position, point, weights.size(),
                    affectDist, repeatWeight, distance) ||
                !dommy::maya::sampleFiniteRamp(ramp, distance, value) ||
                !dommy::numeric::checkedFloatCast(double(value) * weightScale, weights[point]))
                return MS::kInvalidParameter;
        }
        DOMMY_RETURN_IF_FAILURE(DCPAddAttrScalar(
            *curve, true, false, weightAttribute, "", weights));
    }
    MDataHandle output = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(dommy::maya::publishTypedPluginDataObject<DCurveData>(
        output, pluginData.object(), DCurveData::id));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCWeightMap")



MStatus DCWeightMap::initialize() {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    MFnEnumAttribute prmEnum;
    aParamType = prmEnum.create("ParamType", "pty", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(prmEnum.addField("Base Param", 0));
    DOMMY_RETURN_IF_FAILURE(prmEnum.addField("From Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(prmEnum.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aParamType));

    aBaseParam = nAttr.create("BaseParam", "bprm", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBaseParam));

    const MObject aParamAttributeNameDefault = fnStringData.create("param", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aParamAttributeName = tAttr.create("ParamAttributeName", "prmattrn", MFnData::kString,
                                       aParamAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aParamAttributeName));

    aDistance = nAttr.create("Distance", "dst", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(2.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDistance));

    aOffset = nAttr.create("Offset", "off", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    MFnEnumAttribute wrapEnumFn;
    aWrapMode = wrapEnumFn.create("WrapMode", "wmd", 1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField("None", 0));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField("Clamp", 1));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.addField("Loop", 2));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(wrapEnumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWrapMode));

    const MObject aWeightAttributeNameDefault = fnStringData.create("weight", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWeightAttributeName = tAttr.create("WeightAttributeName", "weiattrn", MFnData::kString,
                                        aWeightAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttributeName));

    aWeightRamp = MRampAttribute::createCurveRamp("WeightRamp", "weir", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightRamp));

    aWeightScale = nAttr.create("WeightScale", "wscl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightScale));

    aRepeatWeight = nAttr.create("RepeatWeight", "repw", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRepeatWeight));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aParamType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBaseParam, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aParamAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aDistance, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWrapMode, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWeightAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWeightRamp, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWeightScale, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aRepeatWeight, outDC));
    return MS::kSuccess;
}
