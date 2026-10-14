//
//  DPSort.cpp
//  dommyTool
//
//  Created by yuzo on 6/30/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPSort.h"
#include "common/utility/DAttributeSort.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DIndexTransform.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DDirectionMath.h"

#include "DMayaHeaders.h"

#include "DPointDataHeaders.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

const MTypeId DPSort::typeId( ID_DP_SORT );

MObject DPSort::inDP;

MObject DPSort::aSortType;

MObject DPSort::aIsReverse;

MObject DPSort::aSeed;
MObject DPSort::aOffset;
MObject DPSort::aTargetPoint;
MObject DPSort::aAlongVector;

MObject DPSort::aAttributeName;
MObject DPSort::aAttrComponent;

MObject DPSort::outDP;

void *DPSort::creator() {
    return ::dommy::maya::createObject("DPSort", [&]() {
        return new DPSort();
    });
}

MStatus DPSort::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP) return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle inputHandle = data.inputValue(inDP, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
    if (!input || !input->hasValidPointTopology() ||
        !dommy::attribute::validArrayConversionBudget(
            input->numberOfPoints(), sizeof(double) + 2U * sizeof(std::size_t)))
        return MS::kInvalidParameter;
    const MDataHandle sortHandle = data.inputValue(aSortType, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const short sortType = sortHandle.asShort();
    if (sortType < 0 || sortType > 9) return MS::kInvalidParameter;

    const std::size_t pointCount = input->numberOfPoints();
    const bool reorder = pointCount != 0U && sortType != 0;
    DPIndexList indices;
    if (reorder) {
        const DPointList& points = *input->getPositionPtrConst();
        std::vector<double> keys(pointCount);
        indices.resize(pointCount);
        std::iota(indices.begin(), indices.end(), 0U);
        bool sortValues = false, descending = true;
        if (sortType >= 1 && sortType <= 3) {
            for (std::size_t i = 0U; i < pointCount; ++i) keys[i] = points[i][sortType - 1];
            sortValues = true;
        } else if (sortType == 4) {
            std::reverse(indices.begin(), indices.end());
        } else if (sortType == 5) {
            const MDataHandle seedHandle = data.inputValue(aSeed, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const long seed = seedHandle.asInt();
            // Preserve the historical engine and shuffle sequence on a given runtime.
            std::default_random_engine engine(seed);
            std::shuffle(indices.begin(), indices.end(), engine);
        } else if (sortType == 6) {
            const MDataHandle offsetHandle = data.inputValue(aOffset, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const std::int64_t offset = static_cast<std::int64_t>(offsetHandle.asInt());
            std::size_t normalizedOffset = 0U;
            if (!dommy::container::normalizeCyclicOffset(offset, pointCount, normalizedOffset))
                return MS::kInvalidParameter;
            if (normalizedOffset != 0U)
                std::rotate(indices.begin(), indices.end() - normalizedOffset, indices.end());
        } else if (sortType == 7) {
            const MDataHandle targetHandle = data.inputValue(aTargetPoint, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const double3& target = targetHandle.asDouble3();
            // Keep both the double input and the difference in double precision.
            for (std::size_t i = 0U; i < pointCount; ++i) {
                dommy::direction::Delta delta;
                if (!dommy::direction::difference(points[i], target, delta))
                    return MS::kInvalidParameter;
                keys[i] = delta.length;
            }
            sortValues = true;
            descending = false;
        } else if (sortType == 8) {
            const MDataHandle alongHandle = data.inputValue(aAlongVector, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const double3& alongValues = alongHandle.asDouble3();
            const MVector along(alongValues[0], alongValues[1], alongValues[2]);
            const double length = along.length();
            if (!std::isfinite(along.x) || !std::isfinite(along.y) ||
                !std::isfinite(along.z) || !std::isfinite(length) || length <= 0.0)
                return MS::kInvalidParameter;
            // Preserve the native rotate-to-Y convention.
            const MQuaternion rotation = along.rotateTo(MVector(0.0, 1.0, 0.0));
            for (std::size_t i = 0U; i < pointCount; ++i)
                keys[i] = MVector(points[i][0], points[i][1], points[i][2]).rotateBy(rotation).y;
            sortValues = true;
        } else {
            const MDataHandle componentHandle = data.inputValue(aAttrComponent, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const int component = componentHandle.asInt();
            const MDataHandle nameHandle = data.inputValue(aAttributeName, &status);
            DOMMY_RETURN_IF_FAILURE(status);
            const MString name = nameHandle.asString();
            if (name.length() > dommy::generation::kMaxGeneratedStringByteCount)
                return MS::kInvalidParameter;
            const auto attribute = input->getAttr(name.asChar());
            if (!attribute || attribute->attrSize() != pointCount ||
                !dommy::attribute::sortAttributeIndices(attribute, component, descending, indices))
                return MS::kInvalidParameter;
        }
        if (sortValues && !dommy::attribute::sortValueIndices(keys, 0, descending, indices))
            return MS::kInvalidParameter;
        const MDataHandle reverseHandle = data.inputValue(aIsReverse, &status);
        DOMMY_RETURN_IF_FAILURE(status);
        if (reverseHandle.asBool()) std::reverse(indices.begin(), indices.end());
    }
    status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [input, reorder, &indices](DPData& output) -> MStatus {
            output.setDPData(*input);
            if (reorder && !output.sortPointFromIds(indices)) return MS::kFailure;
            return MS::kSuccess;
        });
    return status ? data.setClean(plug) : status;
}
DOMMY_COMPUTE_CATCH("DPSort")

MStatus DPSort::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnEnumAttribute enumFn;
    aSortType = enumFn.create("SortType", "sot", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("No Change", 0));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("By X", 1));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("By Y", 2));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("By Z", 3));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Reverse", 4));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Random", 5));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Shift", 6));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Proximity to Point", 7));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("Along Vector", 8));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField("By Attribute", 9));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSortType));

    aIsReverse = nAttr.create("IsReverse", "rev", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsReverse));

    aSeed = nAttr.create("Seed", "se", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));

    aOffset = nAttr.create("Offset", "off", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));

    aTargetPoint = nAttr.create("TargetPoint", "tp", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetPoint));

    aAlongVector = nAttr.create("AlongVector", "av", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAlongVector));

    MFnStringData fnStringData;
    MObject attrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aAttributeName = tAttr.create("AttributeName", "attrn", MFnData::kString, attrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));

    aAttrComponent = nAttr.create("AttrComponent", "acm", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAttrComponent));

    // Output data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDP, outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSortType, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsReverse, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aSeed, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aOffset, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aTargetPoint, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAlongVector, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAttributeName, outDP));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aAttrComponent, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPSort", "initialize", std::current_exception());
}
