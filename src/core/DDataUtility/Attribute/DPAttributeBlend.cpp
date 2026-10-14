//
//  DPAttributeBlend.cpp
//  dommyTool
//
//  Created by yuzo on 2016/11/24.
//
//

#include "DPAttributeBlend.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DPointPayloadBudget.h"



#include "DPData.h"
#include "DPointCommon.h"
#include "DPAttributeCommon.h"
#include "dpoint_ids.h"

#include "DMayaHeaders.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

template <typename Value>
bool blendScalarAttribute(const DPData& input, const std::string& sourceName,
                          const std::string& targetName, const MDoubleArray& weights,
                          std::vector<Value>& result) {
    const std::vector<Value>& source = input.attrListValue<Value>(sourceName);
    const std::vector<Value>& target = input.attrListValue<Value>(targetName);
    if (source.size() != target.size() ||
        source.size() != static_cast<std::size_t>(weights.length())) {
        return false;
    }

    result.resize(source.size());
    for (std::size_t index = 0U; index < source.size(); ++index) {
        if (!dommy::attribute::checkedLinearBlend(source[index], target[index], weights[index],
                                                  result[index])) {
            return false;
        }
    }
    return true;
}

template <typename Vector, int ComponentCount>
bool blendVectorAttribute(const DPData& input, const std::string& sourceName,
                          const std::string& targetName, const MDoubleArray& weights,
                          std::vector<Vector>& result) {
    const std::vector<Vector>& source = input.attrListValue<Vector>(sourceName);
    const std::vector<Vector>& target = input.attrListValue<Vector>(targetName);
    if (source.size() != target.size() ||
        source.size() != static_cast<std::size_t>(weights.length())) {
        return false;
    }

    result.resize(source.size());
    for (std::size_t index = 0U; index < source.size(); ++index) {
        for (int component = 0; component < ComponentCount; ++component) {
            typename Vector::BaseType blended;
            if (!dommy::attribute::checkedLinearBlend(
                    source[index][component], target[index][component], weights[index], blended)) {
                return false;
            }
            result[index][component] = blended;
        }
    }
    return true;
}

bool blendMatrixAttribute(const DPData& input, const std::string& sourceName,
                          const std::string& targetName, const MDoubleArray& weights,
                          std::vector<MMatrix>& result) {
    const std::vector<MMatrix>& source = input.attrListValue<MMatrix>(sourceName);
    const std::vector<MMatrix>& target = input.attrListValue<MMatrix>(targetName);
    if (source.size() != target.size() ||
        source.size() != static_cast<std::size_t>(weights.length())) {
        return false;
    }

    result.resize(source.size());
    for (std::size_t index = 0U; index < source.size(); ++index) {
        for (unsigned int row = 0U; row < 4U; ++row) {
            for (unsigned int column = 0U; column < 4U; ++column) {
                if (!dommy::attribute::checkedLinearBlend(
                        source[index][row][column], target[index][row][column], weights[index],
                        result[index][row][column])) {
                    return false;
                }
            }
        }
    }
    return true;
}

template <typename Value>
bool addBlendedAttribute(DPData& output, const DPData& input, const std::string& outputName,
                         const std::string& sourceName, const std::string& targetName,
                         const std::vector<Value>& values, bool deleteOriginal) {
    return dommy::attribute::publishBlendedAttribute(
        output, sourceName, targetName, outputName,
        TypedDPAttributeData<Value>(values, input.attrDefaultValue<Value>(sourceName)),
        deleteOriginal);
}

} // namespace

const MTypeId DPAttributeBlend::typeId(ID_DP_ATTR_BLEND);

MObject DPAttributeBlend::inDP;

MObject DPAttributeBlend::aSourceAttribute;
MObject DPAttributeBlend::aTargetAttribute;
MObject DPAttributeBlend::aWeight;
MObject DPAttributeBlend::aWeightAttribute;

MObject DPAttributeBlend::aOutputAttribute;
MObject DPAttributeBlend::aIsDeleteOriginal;

MObject DPAttributeBlend::outDP;

void* DPAttributeBlend::creator() {
    return ::dommy::maya::createObject("DPAttributeBlend", [&]() {
        return new DPAttributeBlend();
    });
}

MStatus DPAttributeBlend::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP)
        return MS::kUnknownParameter;

    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kInvalidParameter;

    MStatus status;
    const MDataHandle sourceHandle = data.inputValue(aSourceAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string sourceName = sourceHandle.asString().asChar();
    const MDataHandle targetHandle = data.inputValue(aTargetAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string targetName = targetHandle.asString().asChar();
    const MDataHandle weightNameHandle = data.inputValue(aWeightAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const std::string weightName = weightNameHandle.asString().asChar();
    const MDataHandle outputNameHandle = data.inputValue(aOutputAttribute, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    std::string outputName = outputNameHandle.asString().asChar();
    const MDataHandle weightHandle = data.inputValue(aWeight, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const double defaultWeight = weightHandle.asDouble();
    const MDataHandle deleteHandle = data.inputValue(aIsDeleteOriginal, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    const bool deleteOriginal = deleteHandle.asBool();

    if (!std::isfinite(defaultWeight) || sourceName.empty() || targetName.empty() ||
        sourceName == targetName || !input->isExistAttr(sourceName) ||
        !input->isExistAttr(targetName)) {
        return MS::kInvalidParameter;
    }
    if (outputName.empty())
        outputName = sourceName;
    if (!dommy::attribute::validAttributeName(sourceName) ||
        !dommy::attribute::validAttributeName(targetName) ||
        !dommy::attribute::validAttributeName(outputName))
        return MS::kInvalidParameter;

    const DPAttributeData::ConstPtr sourceAttribute = input->getAttr(sourceName);
    const DPAttributeData::ConstPtr targetAttribute = input->getAttr(targetName);
    if (!sourceAttribute || !targetAttribute ||
        sourceAttribute->getAttrType() != targetAttribute->getAttrType()) {
        return MS::kInvalidParameter;
    }

    const DP_ATTR_DATA_TYPE type = sourceAttribute->getAttrType();
    if (!isSingleRealAttrType(type) && !isVector2DAttrType(type) &&
        !isVector3DAttrType(type) && !isMatrixAttrType(type)) {
        return MS::kInvalidParameter;
    }
    const bool hasWeightAttribute = !weightName.empty() && input->isExistAttr(weightName);
    if (hasWeightAttribute) {
        const DPAttributeData::ConstPtr weightAttribute = input->getAttr(weightName);
        if (!weightAttribute || !isSingleRealAttrType(weightAttribute->getAttrType())) {
            return MS::kInvalidParameter;
        }
    }

    const std::size_t pointCount = input->numberOfPoints();
    if (pointCount > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        return MS::kInvalidParameter;
    }
    // Retained metadata, weight conversion scratch and the generated field
    // share one budget before any bulk copy or result allocation.
    dommy::point::PointPayloadBudget workspace;
    dommy::point::preflightPointPayload(*input, sizeof(DPoint) + 2U * sizeof(double), workspace);
    dommy::point::preflightGeneratedPointAttribute(
        pointCount, outputName, dommy::point::pointAttributeStorageBytes(type), workspace);
    MDoubleArray weights(static_cast<unsigned int>(pointCount), defaultWeight);
    if (hasWeightAttribute) {
        getMDoubleArrayFromAttribute(*input, weightName, weights);
    }
    if (weights.length() != pointCount) return MS::kFailure;

    MObject outputObject;
    DPData* output = NULL;
    status = dommy::maya::createPluginDataObject<DPData>(DPData::id, outputObject, output);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (!output) return MS::kFailure;
    output->setDPData(*input);

    bool blended = false;
    switch (sourceAttribute->getAttrType()) {
    case DP_ATTR_INT: {
        std::vector<int32_t> values;
        blended = blendScalarAttribute(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_UINT32: {
        std::vector<uint32_t> values;
        blended = blendScalarAttribute(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_INT64: {
        std::vector<int64_t> values;
        blended = blendScalarAttribute(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_FLOAT: {
        std::vector<float> values;
        blended = blendScalarAttribute(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_DOUBLE: {
        std::vector<double> values;
        blended = blendScalarAttribute(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_VEC2I: {
        std::vector<Imath::V2i> values;
        blended =
            blendVectorAttribute<Imath::V2i, 2>(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_VEC2F: {
        std::vector<Imath::V2f> values;
        blended =
            blendVectorAttribute<Imath::V2f, 2>(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_VEC2D: {
        std::vector<Imath::V2d> values;
        blended =
            blendVectorAttribute<Imath::V2d, 2>(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_VEC3I: {
        std::vector<Imath::V3i> values;
        blended =
            blendVectorAttribute<Imath::V3i, 3>(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_VEC3F: {
        std::vector<Imath::V3f> values;
        blended =
            blendVectorAttribute<Imath::V3f, 3>(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_VEC3D: {
        std::vector<Imath::V3d> values;
        blended =
            blendVectorAttribute<Imath::V3d, 3>(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    case DP_ATTR_MATRIX: {
        std::vector<MMatrix> values;
        blended = blendMatrixAttribute(*input, sourceName, targetName, weights, values);
        if (blended)
            blended = addBlendedAttribute(*output, *input, outputName, sourceName, targetName,
                                          values, deleteOriginal);
        break;
    }
    default:
        return MS::kInvalidParameter;
    }

    if (!blended)
        return MS::kInvalidParameter;
    if (!output->hasValidPointTopology()) {
        return MS::kInvalidParameter;
    }

    MDataHandle outputHandle = data.outputValue(outDP, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    DOMMY_RETURN_IF_FAILURE(
        dommy::maya::publishPluginDataObject(outputHandle, outputObject, *output));
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPAttributeBlend")




MStatus DPAttributeBlend::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
    
    
    
    
    MObject srcAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aSourceAttribute = tAttr.create("SourceAttribute", "srcatr", MFnData::kString, srcAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceAttribute));
    
    MObject targetAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aTargetAttribute = tAttr.create("TargetAttribute", "taratr", MFnData::kString, targetAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTargetAttribute));
    
    aWeight = nAttr.create( "Weight", "wei", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(.5));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWeight));
	
	
	
	MObject weightAttrName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aWeightAttribute = tAttr.create("WeightAttribute", "weiatr", MFnData::kString, weightAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aWeightAttribute));
	
	
	
	MObject outAttrName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOutputAttribute = tAttr.create("OutputAttribute", "outatr", MFnData::kString, outAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOutputAttribute));
	
	
    aIsDeleteOriginal = nAttr.create( "IsDeleteOriginal", "idor", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDeleteOriginal));
    
    
    // ----------------------- OUTPUT --------------------------
    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceAttribute, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTargetAttribute, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aWeight, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aWeightAttribute, outDP ));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutputAttribute, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsDeleteOriginal, outDP ));
    
    
    
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPAttributeBlend", "initialize", std::current_exception());
}
