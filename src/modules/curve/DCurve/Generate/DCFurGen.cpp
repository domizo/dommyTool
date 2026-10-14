//
//  DCFurGen.cpp
//  dommyTool
//
//  Created by yuzo on 2017/03/12.
//
//

#include "DCFurGen.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DFurGenerationPlan.h"
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DMayaNameList.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMayaValueValidation.h"
#include "DPoint/Common/DPAttributeAccess.h"
#include "DPoint/math/DVectorMath.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPData.h"
#include "DPointCommon.h"
#include "DCurveUtil.h"
#include "DPAttributeCommon.h"
#include "dcurve_ids.h"

#include "dommyMath.h"

#include <cmath>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
#include <utility>

namespace {

bool normalizeDirection(MVector& direction)
{
    MVector candidate;
    if (!dommy::math::normalized3OrZero(direction.x, direction.y, direction.z,
            candidate.x, candidate.y, candidate.z, 1.0e-12)) return false;
    direction = candidate;
    return true;
}

MStatus preflightFurTransfers(
    const DPData& input, const MString& transferAttributes, std::size_t pointCount,
    dommy::generation::CurveGenerationStorage& storage, MStringArray& transferNames)
{
    // Charge token/string-object capacity before splitting, including the worst
    // case of very short names. Keep Maya's space splitting and duplicate order.
    const std::size_t characters = transferAttributes.length();
    storage.add(characters, 64U);
    const std::size_t bytes = std::strlen(transferAttributes.asChar());
    if (bytes > characters) storage.add(bytes - characters, 64U);
    dommy::maya::NameListBudget nameBudget;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::splitCheckedNameList(
        transferAttributes, pointCount, nameBudget, transferNames));

    dommy::point::PointPayloadBudget attributes;
    for (unsigned int i = 0U; i < transferNames.length(); ++i) {
        const std::string name = transferNames[i].asChar();
        if (name.empty() || !input.isExistAttr(name)) continue;
        const std::pair<std::string, DPAttributeData::ConstPtr> field(name, input.getAttr(name));
        // Full source cardinality bounds any selected subset. The existing
        // four-copy allowance includes selection, temporary attribute and output
        // clones, with dynamic strings/int arrays and their default values.
        dommy::point::preflightAttributePayload(&field, &field + 1, pointCount, attributes);
    }
    storage.add(1U, attributes.estimatedBytes());
    return MS::kSuccess;
}

template <typename Value>
bool copySelectedAttribute(const DPData& input,
                           const std::string& name,
                           const std::vector<std::size_t>& selectedIndices,
                           DCurveData& output)
{
    const std::vector<Value>& source = input.attrListValue<Value>(name);
    if (source.size() != input.numberOfPoints()) return false;

    std::vector<Value> selected;
    selected.reserve(selectedIndices.size());
    for (std::size_t index = 0U; index < selectedIndices.size(); ++index) {
        if (selectedIndices[index] >= source.size()) return false;
        selected.push_back(source[selectedIndices[index]]);
    }
    output.addAttr(
        name,
        TypedDPAttributeData<Value>(
            selected, input.attrDefaultValue<Value>(name)));
    return true;
}

bool copySelectedAttribute(const DPData& input,
                           const std::string& name,
                           const std::vector<std::size_t>& selectedIndices,
                           DCurveData& output)
{
    const DPAttributeData::ConstPtr attribute = input.getAttr(name);
    if (!attribute) return false;

    switch (attribute->getAttrType()) {
        case DP_ATTR_BOOL:
            return copySelectedAttribute<bool>(
                input, name, selectedIndices, output);
        case DP_ATTR_INT:
            return copySelectedAttribute<int32_t>(
                input, name, selectedIndices, output);
        case DP_ATTR_UINT32:
            return copySelectedAttribute<uint32_t>(
                input, name, selectedIndices, output);
        case DP_ATTR_INT64:
            return copySelectedAttribute<int64_t>(
                input, name, selectedIndices, output);
        case DP_ATTR_FLOAT:
            return copySelectedAttribute<float>(
                input, name, selectedIndices, output);
        case DP_ATTR_DOUBLE:
            return copySelectedAttribute<double>(
                input, name, selectedIndices, output);
        case DP_ATTR_VEC2I:
            return copySelectedAttribute<Imath::V2i>(
                input, name, selectedIndices, output);
        case DP_ATTR_VEC2F:
            return copySelectedAttribute<Imath::V2f>(
                input, name, selectedIndices, output);
        case DP_ATTR_VEC2D:
            return copySelectedAttribute<Imath::V2d>(
                input, name, selectedIndices, output);
        case DP_ATTR_VEC3I:
            return copySelectedAttribute<Imath::V3i>(
                input, name, selectedIndices, output);
        case DP_ATTR_VEC3F:
            return copySelectedAttribute<Imath::V3f>(
                input, name, selectedIndices, output);
        case DP_ATTR_VEC3D:
            return copySelectedAttribute<Imath::V3d>(
                input, name, selectedIndices, output);
        case DP_ATTR_STRING:
            return copySelectedAttribute<std::string>(
                input, name, selectedIndices, output);
        case DP_ATTR_MATRIX:
            return copySelectedAttribute<MMatrix>(
                input, name, selectedIndices, output);
        case DP_ATTR_INT_ARRAY:
            return copySelectedAttribute<DIntArray>(
                input, name, selectedIndices, output);
        default:
            return false;
    }
}

} // namespace


const MTypeId DCFurGen::typeId( ID_DC_FUR_GEN );


MObject DCFurGen::inDP;

MObject DCFurGen::aSeed;
MObject DCFurGen::aThinning;

MObject DCFurGen::aSegment;

MObject DCFurGen::aLength;
MObject DCFurGen::aLengthRandom;
MObject DCFurGen::aLengthAttribute;


MObject DCFurGen::aNormalAttribute;

MObject DCFurGen::aBend;

MObject DCFurGen::aIsStruggle;
MObject DCFurGen::aStruggleRange;
MObject DCFurGen::aStruggleStrength;
MObject DCFurGen::aStruggleRamp;

MObject DCFurGen::aOffset;
MObject DCFurGen::aScale;
MObject DCFurGen::aTransferAttribute;

MObject DCFurGen::outDC;



void *DCFurGen::creator()
{
    return ::dommy::maya::createObject("DCFurGen", [&]() {
        return new DCFurGen();
    });
}

MStatus DCFurGen::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug != outDC) return MS::kUnknownParameter;

    MStatus status;
    const MDataHandle seedHandle = data.inputValue(aSeed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int seed = seedHandle.asInt();
    const MDataHandle thinningHandle = data.inputValue(aThinning, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float thinning = thinningHandle.asFloat();
    const MDataHandle baseLengthHandle = data.inputValue(aLength, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float baseLength = baseLengthHandle.asFloat();
    const MDataHandle randomLengthHandle = data.inputValue(aLengthRandom, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float randomLength = randomLengthHandle.asFloat();
    const MDataHandle segmentCountHandle = data.inputValue(aSegment, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int segmentCount = segmentCountHandle.asInt();
    const MDataHandle bendHandle = data.inputValue(aBend, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float bend = bendHandle.asFloat();
    const MDataHandle addStruggleHandle = data.inputValue(aIsStruggle, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool addStruggle = addStruggleHandle.asBool();
    const MDataHandle struggleComponentsHandle = data.inputValue(aStruggleRange, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float* struggleComponentsRaw = struggleComponentsHandle.asFloat3();
    if (!struggleComponentsRaw) return MS::kFailure;
    const std::array<float, 3> struggleComponents = {{
        struggleComponentsRaw[0], struggleComponentsRaw[1], struggleComponentsRaw[2]}};
    const MDataHandle struggleStrengthHandle = data.inputValue(aStruggleStrength, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float struggleStrength = struggleStrengthHandle.asFloat();
    const MDataHandle scaleHandle = data.inputValue(aScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float scale = scaleHandle.asFloat();
    const MDataHandle offsetHandle = data.inputValue(aOffset, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float offset = offsetHandle.asFloat();

    if (segmentCount < 2 ||
        static_cast<std::size_t>(segmentCount) >=
            dommy::generation::kMaxCurveSampleCount ||
        !std::isfinite(thinning) || thinning < 0.0F || thinning > 1.0F ||
        !std::isfinite(baseLength) || baseLength < 0.0F ||
        !std::isfinite(randomLength) || randomLength < 0.0F ||
        !std::isfinite(bend) ||
        !std::isfinite(struggleStrength) || struggleStrength < 0.0F ||
        !std::isfinite(scale) || !std::isfinite(offset)) {
        return MS::kInvalidParameter;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(struggleComponents[axis]) ||
            struggleComponents[axis] < 0.0F) {
            return MS::kInvalidParameter;
        }
    }
    const double bendRadians = dommy::math::degreesToRadians(static_cast<double>(bend));
    if (!std::isfinite(bendRadians)) return MS::kInvalidParameter;

    const MDataHandle lengthAttributeHandle = data.inputValue(aLengthAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string lengthAttribute = lengthAttributeHandle.asString().asChar();
    const MDataHandle normalAttributeHandle = data.inputValue(aNormalAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const std::string normalAttribute = normalAttributeHandle.asString().asChar();
    const MDataHandle transferAttributesHandle = data.inputValue(aTransferAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const MString transferAttributes = transferAttributesHandle.asString();

    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kFailure;
    if (!lengthAttribute.empty() && input->isExistAttr(lengthAttribute) &&
        !isSingleRealAttrType(
            input->getAttr(lengthAttribute)->getAttrType())) {
        return MS::kInvalidParameter;
    }
    if (!normalAttribute.empty() && input->isExistAttr(normalAttribute) &&
        !isVector3DAttrType(
            input->getAttr(normalAttribute)->getAttrType())) {
        return MS::kInvalidParameter;
    }

    const std::size_t pointCount = input->numberOfPoints();
    dommy::generation::FurGenerationPlan plan;
    if (!dommy::generation::buildFurGenerationPlan(pointCount, segmentCount, plan)) {
        return MS::kInvalidParameter;
    }
    const std::size_t cvCount = plan.pointsPerCurve;
    const std::size_t maximumOutputPointCount = plan.totalPointCount;
    dommy::generation::CurveGenerationStorage storage(plan.estimatedBytes);
    MStringArray transferNames;
    DOMMY_RETURN_IF_FAILURE(
        preflightFurTransfers(*input, transferAttributes, pointCount, storage, transferNames));

    std::vector<float> thinningValues(pointCount, 1.0F);
    if (thinning > 0.0F) {
        dommy::random::generateFloatValues(
            static_cast<std::uint32_t>(seed), 0.0F, 1.0F,
            pointCount, thinningValues);
    }

    MVectorArray normals;
    if (!normalAttribute.empty() && input->isExistAttr(normalAttribute)) {
        if (!getCheckedMVectorArrayFromAttribute(*input, normalAttribute, pointCount, normals))
            return MS::kInvalidParameter;
    } else {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(pointCount,
            [](unsigned int, MVector& value) -> MStatus {
                value = MVector::yAxis;
                return MS::kSuccess;
            }, normals));
    }

    DScalarDataList lengthOffsets(pointCount, 0.0);
    if (!lengthAttribute.empty() && input->isExistAttr(lengthAttribute)) {
        if (!getCheckedDScalarArrayFromAttribute(
                *input, lengthAttribute, pointCount, lengthOffsets)) return MS::kInvalidParameter;
    }
    std::vector<float> randomLengths(pointCount, 0.0F);
    if (randomLength > 0.0F) {
        dommy::random::generateFloatValues(
            static_cast<std::uint32_t>(seed), 0.0F, randomLength,
            pointCount, randomLengths);
    }

    MFloatArray struggleRampValues;
    MRampAttribute struggleRamp(
        MPlug(thisMObject(), aStruggleRamp), &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    struggleRamp.sampleValueRamp(
        static_cast<unsigned int>(cvCount),
        struggleRampValues, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (struggleRampValues.length() != cvCount) {
        return MS::kFailure;
    }

    const short degree = segmentCount < 3 ? 1 : 3;
    MDoubleArray knots;
    GenerateDCurveKnots(
        static_cast<unsigned int>(cvCount), degree, knots);
    if (knots.length() != cvCount + static_cast<std::size_t>(degree - 1))
        return MS::kFailure;

    MObject outputObject;
    DCurveData* output = nullptr;
    status = dommy::maya::createPluginDataObject<DCurveData>(
        DCurveData::id, outputObject, output);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!output) return MS::kFailure;

    std::vector<std::size_t> selectedIndices;
    selectedIndices.reserve(pointCount);
    DCurveData::DCurveList generatedCurves;
    generatedCurves.reserve(pointCount);
    std::size_t pointIndex = 0U;
    for (ConstDPointIter point = input->beginPt();
         point != input->endPt(); ++point, ++pointIndex) {
        if (thinning > 0.0F &&
            thinningValues[pointIndex] < thinning) {
            continue;
        }

        const double generatedLength =
            (static_cast<double>(baseLength) +
             lengthOffsets[pointIndex] +
             static_cast<double>(randomLengths[pointIndex])) *
                static_cast<double>(scale) +
            static_cast<double>(offset);
        if (!std::isfinite(generatedLength)) {
            return MS::kInvalidParameter;
        }
        if (generatedLength <= 0.0) continue;
        const double stepLength =
            generatedLength / static_cast<double>(segmentCount);
        if (!std::isfinite(stepLength)) return MS::kInvalidParameter;

        MVector direction = normals[pointIndex];
        if (!normalizeDirection(direction)) {
            direction = MVector::yAxis;
        }
        MVector bendAxis = direction ^ MVector::yAxis;
        if (!normalizeDirection(bendAxis)) {
            bendAxis = direction ^ MVector::xAxis;
            if (!normalizeDirection(bendAxis)) {
                return MS::kInvalidParameter;
            }
        }

        std::vector<float> noise[3];
        for (int axis = 0; axis < 3; ++axis) {
            noise[axis].assign(cvCount, 0.0F);
            if (addStruggle && struggleComponents[axis] > 0.0F) {
                const std::uint32_t noiseSeed =
                    static_cast<std::uint32_t>(seed) +
                    static_cast<std::uint32_t>(pointIndex) +
                    static_cast<std::uint32_t>(axis);
                dommy::random::generateFloatValues(
                    noiseSeed, -struggleComponents[axis],
                    struggleComponents[axis], cvCount, noise[axis]);
            }
        }

        MPoint basePoint(
            (*point)[0], (*point)[1], (*point)[2]);
        if (!dommy::maya::isFloatRepresentablePoint(basePoint)) return MS::kInvalidParameter;
        MPointArray curvePoints;
        for (std::size_t cvIndex = 0U;
             cvIndex < cvCount; ++cvIndex) {
            if (cvIndex > 0U) {
                direction = direction.rotateBy(
                    MQuaternion(bendRadians, bendAxis));
                if (!normalizeDirection(direction)) {
                    return MS::kInvalidParameter;
                }
                basePoint += direction * stepLength;
            }

            const double rampValue = struggleRampValues[cvIndex];
            if (!std::isfinite(rampValue)) {
                return MS::kInvalidParameter;
            }
            MPoint curvePoint = basePoint + MVector(
                static_cast<double>(noise[0][cvIndex]) *
                    struggleStrength * rampValue,
                static_cast<double>(noise[1][cvIndex]) *
                    struggleStrength * rampValue,
                static_cast<double>(noise[2][cvIndex]) *
                    struggleStrength * rampValue);
            // DCurveInternalData stores float CVs: reject before narrowing.
            if (!dommy::maya::isFloatRepresentablePoint(curvePoint)) {
                return MS::kInvalidParameter;
            }
            DOMMY_RETURN_IF_FAILURE(curvePoints.append(curvePoint));
            if (curvePoints.length() != cvIndex + 1U) return MS::kFailure;
        }

        generatedCurves.push_back(DCItem(new DCurveInternalData(
            curvePoints, knots, degree)));
        selectedIndices.push_back(pointIndex);
    }

    // One transaction avoids revalidating all preceding curves for every root.
    // insert copies the batch; release staged curves before attribute transfer.
    if (!generatedCurves.empty() && !output->insert(generatedCurves)) return MS::kFailure;
    generatedCurves.clear();

    for (unsigned int index = 0U;
         index < transferNames.length(); ++index) {
        const std::string attributeName =
            transferNames[index].asChar();
        if (attributeName.empty() ||
            !input->isExistAttr(attributeName)) {
            continue;
        }
        if (!copySelectedAttribute(
                *input, attributeName, selectedIndices, *output)) {
            return MS::kInvalidParameter;
        }
    }

    output->updateBBox();
    if (output->numberOfCurves() != selectedIndices.size() ||
        !output->hasValidCurveTopology() ||
        output->getTotalCvs() > maximumOutputPointCount) {
        return MS::kInvalidParameter;
    }

    MDataHandle outputHandle = data.outputValue(outDC, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(
            outputHandle, outputObject, *output));
    outputHandle.setClean();
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCFurGen")

            


MStatus DCFurGen::initialize() try
{
    MStatus stat;
    
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnMessageAttribute msAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
	
	
	aSeed = nAttr.create( "Seed", "se", MFnNumericData::kInt, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(100));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSeed));
	
	aThinning = nAttr.create( "Thinning", "thin", MFnNumericData::kFloat, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aThinning));
	
    aSegment = nAttr.create( "Segment", "seg", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(20));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSegment));
    
    
    
    aLength = nAttr.create( "Length", "len", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLength));
    
    aLengthRandom = nAttr.create( "LengthRandom", "lenrnd", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthRandom));
    
    const MObject aLengthAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aLengthAttribute = tAttr.create("LengthAttribute", "lenattr", MFnData::kString, aLengthAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLengthAttribute));
    
    
    
    
    
    
    const MObject aNormalAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNormalAttribute = tAttr.create("NormalAttribute", "nrmattr", MFnData::kString, aNormalAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttribute));
	
	
	aBend = nAttr.create( "Bend", "be", MFnNumericData::kFloat, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aBend));
	
	
	aIsStruggle = nAttr.create( "IsStruggle", "isstr", MFnNumericData::kBoolean, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsStruggle));
	
	
	aStruggleRange = nAttr.create( "StruggleRange", "strrng", MFnNumericData::k3Float, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStruggleRange));
	
	aStruggleStrength = nAttr.create( "StruggleStrength", "strstr", MFnNumericData::kFloat, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStruggleStrength));
	
	aStruggleRamp = MRampAttribute::createCurveRamp("StruggleRamp", "strrmp", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aStruggleRamp));
	
	
	
	
	aOffset = nAttr.create( "Offset", "off", MFnNumericData::kFloat, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOffset));
	
	aScale = nAttr.create( "Scale", "scl", MFnNumericData::kFloat, 0.0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(5.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aScale));
    
    
    
    const MObject aTransferAttributeDefault = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTransferAttribute = tAttr.create("TransferAttribute", "transattr", MFnData::kString, aTransferAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTransferAttribute));
	
	
	
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( inDP, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aSeed, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aThinning, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aSegment, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aLength, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aLengthRandom, outDC ) );
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aLengthAttribute, outDC ) );
    
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aNormalAttribute, outDC ) );
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aBend, outDC ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsStruggle, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStruggleRange, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStruggleStrength, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aStruggleRamp, outDC ));
	
	
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aOffset, outDC ) );
	DOMMY_RETURN_IF_FAILURE( attributeAffects( aScale, outDC ) );
	
    DOMMY_RETURN_IF_FAILURE( attributeAffects( aTransferAttribute, outDC ) );
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DCFurGen", "initialize", std::current_exception());
}
