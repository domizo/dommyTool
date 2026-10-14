//
//  DCCharactor.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/26.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCCharactor.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DCurveData.h"
#include "DCurveUtil.h"
#include "dcurve_ids.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DNumericValidation.h"

#include <cmath>
#include <random>
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DAttributePublication.h"
#include <memory>
#include "math/DRemapMath.h"

const MTypeId DCCharactor::typeId(ID_DC_CHARACTOR);

MObject DCCharactor::inDC;

MObject DCCharactor::aAddLengthAttribute;
MObject DCCharactor::aLengthAttributeName;

MObject DCCharactor::aAddWidthAttribute;
MObject DCCharactor::aWidthAttributeName;
MObject DCCharactor::aWidthType;

MObject DCCharactor::aBaseWidth;
MObject DCCharactor::aTipWidth;

MObject DCCharactor::aWidthRamp;

MObject DCCharactor::aIsRandom;
MObject DCCharactor::aSeed;
MObject DCCharactor::aWidthRange;
MObject DCCharactor::aWidthScale;

MObject DCCharactor::aAddParamAttribute;
MObject DCCharactor::aParamAttributeName;
MObject DCCharactor::aIsNormalizedParam;

MObject DCCharactor::aIsChangeKnot;
MObject DCCharactor::aKnotType;

MObject DCCharactor::outDC;

void* DCCharactor::creator() {
    return ::dommy::maya::createObject("DCCharactor", [&]() {
        return new DCCharactor();
    });
}

void DCCharactor::postConstructor() try {
    const auto initializeWidthRamp = [&]() -> MStatus {
        MStatus status;
        MPlug widthPlug(thisMObject(), aWidthRamp);
        MCurveAttribute widthAttr(widthPlug, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        const unsigned int count = widthAttr.getNumEntries(&status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (count != 0U)
            return MS::kSuccess;
        MFloatArray endpoints;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            2U,
            [](unsigned int i, float& value) -> MStatus {
                value = static_cast<float>(i);
                return MS::kSuccess;
            },
            endpoints));
        // The legacy default uses matching 0/1 positions and values.
        return widthAttr.setCurve(endpoints, endpoints);
    };
    const MStatus status = initializeWidthRamp();
    if (!status)
        status.pAPIerror("DCCharactor::postConstructor", __LINE__);
} catch (...) {
    (void)dommy::maya::reportEntryPointException("DCCharactor", "postConstructor",
                                                 std::current_exception());
}

MStatus DCCharactor::compute(const MPlug& plug, MDataBlock& data) try {
    if (!(plug == outDC))
        return MS::kUnknownParameter;
    MStatus stat;

#pragma mark get input dcurve
    const DCurveData* inputDCData = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));
    if (!inputDCData)
        return MS::kFailure;
    if (inputDCData->numberOfCurves() > dommy::generation::kMaxGeneratedCurveCount ||
        inputDCData->getTotalCvs() > dommy::generation::kMaxGeneratedPointCount)
        return MS::kInvalidParameter;

    MObject outputDataObject;
    DCurveData* outDCrvs = nullptr;
    stat =
        dommy::maya::createPluginDataObject<DCurveData>(DCurveData::id, outputDataObject, outDCrvs);
    if (!stat)
        return stat;

    if (!outDCrvs)
        return MS::kFailure;

    DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));

    const std::size_t curveCount = outDCrvs->numberOfCurves();
    if (curveCount > dommy::generation::kMaxGeneratedCurveCount ||
        outDCrvs->getTotalCvs() > dommy::generation::kMaxGeneratedPointCount) {
        return MS::kInvalidParameter;
    }

    if (curveCount) {

        const MDataHandle addLengthAttrHandle = data.inputValue(aAddLengthAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool addLengthAttr = addLengthAttrHandle.asBool();
        const MDataHandle lengthAttrNameHandle = data.inputValue(aLengthAttributeName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string lengthAttrName = lengthAttrNameHandle.asString().asChar();
        const MDataHandle addWidthAttrHandle = data.inputValue(aAddWidthAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool addWidthAttr = addWidthAttrHandle.asBool();
        const MDataHandle widthAttrNameHandle = data.inputValue(aWidthAttributeName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string widthAttrName = widthAttrNameHandle.asString().asChar();
        const MDataHandle addParamAttrHandle = data.inputValue(aAddParamAttribute, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool addParamAttr = addParamAttrHandle.asBool();
        const MDataHandle paramAttrNameHandle = data.inputValue(aParamAttributeName, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const std::string paramAttrName = paramAttrNameHandle.asString().asChar();
        const MDataHandle isChangeKnotHandle = data.inputValue(aIsChangeKnot, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const bool isChangeKnot = isChangeKnotHandle.asBool();
        const bool writeLength = addLengthAttr && !lengthAttrName.empty();
        const bool writeWidth = addWidthAttr && !widthAttrName.empty();
        const bool writeParam = addParamAttr && !paramAttrName.empty();
        if ((writeLength && (!dommy::attribute::validAttributeName(lengthAttrName) ||
             lengthAttrName.size() > dommy::generation::kMaxGeneratedStringByteCount)) ||
            (writeWidth && (!dommy::attribute::validAttributeName(widthAttrName) ||
             widthAttrName.size() > dommy::generation::kMaxGeneratedStringByteCount)) ||
            (writeParam && (!dommy::attribute::validAttributeName(paramAttrName) ||
             paramAttrName.size() > dommy::generation::kMaxGeneratedStringByteCount)))
            return MS::kInvalidParameter;
        // Length belongs to Primitive scope; Width and Param share Point scope.
        // Equal names across different scopes are intentional and remain valid.
        if (writeWidth && writeParam && widthAttrName == paramAttrName)
            return MS::kInvalidParameter;
#pragma mark length
        if (addLengthAttr && !lengthAttrName.empty()) {

            DScalarDataList lengthValueArray(curveCount, 0.0);

            size_t offset = 0;
            for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves(); ++cIt) {

                MObject crvObj;
                stat = generateMCurveFromDcurve(*cIt, crvObj);
                if (!stat)
                    return stat;
                MFnNurbsCurve crvFn(crvObj, &stat);
                if (!stat)
                    return stat;

                const double curveLength = crvFn.length(kMFnNurbsEpsilon, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                if (!std::isfinite(curveLength) || curveLength < 0.0) {
                    return MS::kInvalidParameter;
                }
                if (!dommy::numeric::checkedFloatCast(curveLength, lengthValueArray[offset++]))
                    return MS::kInvalidParameter;
            }

            const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(lengthValueArray);
            if (!dommy::attribute::commitStagedAttribute(
                    *outDCrvs, lengthAttrName, candidate, curveCount)) return MS::kFailure;
        }

#pragma mark width
        if (addWidthAttr && !widthAttrName.empty()) {

            const MDataHandle widthTypeHandle = data.inputValue(aWidthType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const short widthType = widthTypeHandle.asShort();
            const MDataHandle isRandHandle = data.inputValue(aIsRandom, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            bool isRand = isRandHandle.asBool();
            const MDataHandle randRangeHandle = data.inputValue(aWidthRange, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double2& randRange = randRangeHandle.asDouble2();
            const MDataHandle widthScaleHandle = data.inputValue(aWidthScale, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const double widthScale = widthScaleHandle.asDouble();
            const MDataHandle seedHandle = data.inputValue(aSeed, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const int seed = seedHandle.asInt();
            if ((widthType != 0 && widthType != 1) || !std::isfinite(randRange[0]) ||
                !std::isfinite(randRange[1]) || !std::isfinite(widthScale)) {
                return MS::kInvalidParameter;
            }

            const int checkedSeed = dommy::random::requireOffsetNonnegativeSeed(seed, 0U);
            std::mt19937 eng(static_cast<std::mt19937::result_type>(checkedSeed));
            double minval = std::min(randRange[0], randRange[1]);
            double maxval = std::max(randRange[0], randRange[1]);
            // Equal bounds historically disable jitter, rather than add a fixed offset.
            isRand = isRand && minval != maxval;
            if (isRand)
                (void)dommy::random::prepareRealBounds(minval, maxval, false);
            std::uniform_real_distribution<double> distrib(isRand ? minval : 0.0,
                                                           isRand ? maxval : 1.0);
            switch (widthType) {
            case 0: // base and tip
            {

                const MDataHandle baseWidthHandle = data.inputValue(aBaseWidth, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const double baseWidth = baseWidthHandle.asDouble();
                const MDataHandle tipWidthHandle = data.inputValue(aTipWidth, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);
                const double tipWidth = tipWidthHandle.asDouble();
                if (!std::isfinite(baseWidth) || !std::isfinite(tipWidth)) {
                    return MS::kInvalidParameter;
                }

                for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves(); ++cIt) {

                    DScalarDataList widthArray;
                    const std::size_t pointCount = (*cIt)->numberOfPoints();

                    for (std::size_t i = 0U; i < pointCount; ++i) {

                        double val = 0.0;
                        const double weight =
                            pointCount > 1U ? double(i) / double(pointCount - 1U) : 0.0;
                        if (!dommy::attribute::checkedLinearBlend(baseWidth, tipWidth, weight, val))
                            return MS::kInvalidParameter;
                        const double randVal = isRand ? distrib(eng) : 0.0;
                        const double widthValue = (val + randVal) * widthScale;
                        DScalarData storedWidth = 0.0f;
                        if (!dommy::numeric::checkedFloatCast(widthValue, storedWidth))
                            return MS::kInvalidParameter;
                        widthArray.push_back(storedWidth);
                    }
                    const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(widthArray);
                    if (!dommy::attribute::commitStagedAttribute(
                            **cIt, widthAttrName, candidate, (*cIt)->numberOfPoints())) return MS::kFailure;
                }

            } break;
            case 1: // ramp
            {
                MPlug widthRampPlug(thisMObject(), aWidthRamp);
                MRampAttribute widthRampAttr(widthRampPlug, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);

                for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves(); ++cIt) {

                    DScalarDataList widthArray;
                    const unsigned int pointCount = (*cIt)->numberOfPoints();
                    MFloatArray values;
                    widthRampAttr.sampleValueRamp(pointCount, values, &stat);
                    if (!stat)
                        return stat;
                    if (values.length() != pointCount) {
                        return MS::kFailure;
                    }

                    for (unsigned int i = 0U; i < pointCount; ++i) {
                        const double val = values[i];
                        const double randVal = isRand ? distrib(eng) : 0.0;
                        const double widthValue = (val + randVal) * widthScale;
                        DScalarData storedWidth = 0.0f;
                        if (!dommy::numeric::checkedFloatCast(widthValue, storedWidth))
                            return MS::kInvalidParameter;
                        widthArray.push_back(storedWidth);
                    }

                    const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(widthArray);
                    if (!dommy::attribute::commitStagedAttribute(
                            **cIt, widthAttrName, candidate, (*cIt)->numberOfPoints())) return MS::kFailure;
                }

            } break;
            default:
                break;
            }
        }

#pragma mark param

        if (addParamAttr && !paramAttrName.empty()) {

            const MDataHandle isNormalizedHandle = data.inputValue(aIsNormalizedParam, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool isNormalized = isNormalizedHandle.asBool();
            for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves(); ++cIt) {

                MObject crvObj;
                stat = generateMCurveFromDcurve(*cIt, crvObj);
                if (!stat)
                    return stat;
                MFnNurbsCurve crvFn(crvObj, &stat);
                if (!stat)
                    return stat;

                const DPointList* ptPtr = (*cIt)->getPositionPtrConst();

                DScalarDataList params(ptPtr->size(), 0.0f);
                double domainStart = 0.0, domainEnd = 1.0;
                if (isNormalized) {
                    DOMMY_RETURN_IF_FAILURE(crvFn.getKnotDomain(domainStart, domainEnd));
                    if (!std::isfinite(domainStart) || !std::isfinite(domainEnd) ||
                        domainEnd <= domainStart)
                        return MS::kInvalidParameter;
                }
                size_t n = 0U;
                for (const auto& point : *ptPtr) {
                    double param = 0.0;
                    const MPoint closest =
                        crvFn.closestPoint(MPoint(point[0], point[1], point[2]), &param,
                                           kMFnNurbsEpsilon, MSpace::kObject, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    if (!dommy::maya::isFinitePoint(closest) || !std::isfinite(param))
                        return MS::kInvalidParameter;
                    // Normalize in double before narrowing into the float-backed attribute.
                    if (isNormalized &&
                        !dommy::math::normalizeRange(param, domainStart, domainEnd, param))
                        return MS::kInvalidParameter;
                    if (!dommy::numeric::checkedFloatCast(param, params[n++]))
                        return MS::kInvalidParameter;
                }
                const DPAttributeData::Ptr candidate = std::make_shared<DPFloatAttr>(params);
                if (!dommy::attribute::commitStagedAttribute(
                        **cIt, paramAttrName, candidate, (*cIt)->numberOfPoints())) return MS::kFailure;
            }
        }

#pragma mark knot
        if (isChangeKnot) {

            const MDataHandle knotTypeHandle = data.inputValue(aKnotType, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const short knotType = knotTypeHandle.asShort();
            if (knotType != 0 && knotType != 1)
                return MS::kInvalidParameter;
            for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves(); ++cIt) {
                MDoubleArray newKnots;
                GenerateDCurveKnots((*cIt)->numberOfPoints(), (*cIt)->getDegree(), newKnots,
                                    knotType == 0 ? true : false);

                if (!(*cIt)->replaceKnots(newKnots)) {
                    return MS::kFailure;
                }
            }
        }
    }

    outDCrvs->updateBBox();

    MDataHandle output = data.outputValue(outDC, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(output, outputDataObject, *outDCrvs));

    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCCharactor")

#pragma mark get attribute

MStatus DCCharactor::initialize() {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    aAddLengthAttribute =
        nAttr.create("AddLengthAttribute", "adlenattr", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddLengthAttribute));

    const MObject aLengthAttributeNameDefault = fnStringData.create("len", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aLengthAttributeName = tAttr.create("LengthAttributeName", "lenattrnm", MFnData::kString,
                                        aLengthAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthAttributeName));

    aAddWidthAttribute =
        nAttr.create("AddWidthAttribute", "adwattr", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddWidthAttribute));

    const MObject aWidthAttributeNameDefault = fnStringData.create("width", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWidthAttributeName = tAttr.create("WidthAttributeName", "wattrnm", MFnData::kString,
                                       aWidthAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthAttributeName));

    MFnEnumAttribute wEnumFn;
    aWidthType = wEnumFn.create("WidthType", "wty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(wEnumFn.addField("Base and Tip", 0));
    DOMMY_RETURN_IF_FAILURE(wEnumFn.addField("Ramp", 1));
    DOMMY_RETURN_IF_FAILURE(wEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthType));

    aBaseWidth = nAttr.create("BaseWidth", "bw", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.05));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBaseWidth));

    aTipWidth = nAttr.create("TipWidth", "tw", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTipWidth));

    aWidthRamp = MRampAttribute::createCurveRamp("WidthRamp", "wrmp", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthRamp));

    aIsRandom = nAttr.create("IsRandom", "isrnd", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRandom));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aWidthRange = nAttr.create("WidthRange", "wrng", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthRange));

    aWidthScale = nAttr.create("WidthScale", "wscl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthScale));

    aAddParamAttribute =
        nAttr.create("AddParamAttribute", "adprmattr", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAddParamAttribute));

    const MObject aParamAttributeNameDefault = fnStringData.create("param", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aParamAttributeName = tAttr.create("ParamAttributeName", "prattrnm", MFnData::kString,
                                       aParamAttributeNameDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aParamAttributeName));

    aIsNormalizedParam =
        nAttr.create("IsNormalizedParam", "isnrp", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsNormalizedParam));

    aIsChangeKnot = nAttr.create("IsChangeKnot", "ischk", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsChangeKnot));

    MFnEnumAttribute knotEnumFn;
    aKnotType = knotEnumFn.create("KnotType", "knty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(knotEnumFn.addField("Linear Step", 0));
    DOMMY_RETURN_IF_FAILURE(knotEnumFn.addField("Multi End Knot", 1));
    DOMMY_RETURN_IF_FAILURE(knotEnumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKnotType));

    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddLengthAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aLengthAttributeName, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddWidthAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWidthAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWidthType, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aBaseWidth, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTipWidth, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWidthRamp, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsRandom, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWidthRange, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWidthScale, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAddParamAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aParamAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsNormalizedParam, outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsChangeKnot, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aKnotType, outDC));

    return MS::kSuccess;
}
