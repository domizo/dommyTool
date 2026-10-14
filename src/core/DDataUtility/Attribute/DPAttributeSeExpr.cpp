//
//  DPAttributeSeExpr.cpp
//  dommyTool
//
//  Created by yuzo on 5/22/17.
//
//

#include "DPAttributeSeExpr.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DExpressionResult.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DNumericValidation.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"

#include <cmath>
#include <cstddef>




const MTypeId DPAttributeSeExpr::typeId( ID_DP_ATTR_SEEXPR );


MObject DPAttributeSeExpr::inDD;

MObject DPAttributeSeExpr::aAttributeName;
MObject DPAttributeSeExpr::aAttributeType;

MObject DPAttributeSeExpr::aExpression;

MObject DPAttributeSeExpr::aUserFloat1;
MObject DPAttributeSeExpr::aUserFloat2;
MObject DPAttributeSeExpr::aUserFloat3;
MObject DPAttributeSeExpr::aUserFloat4;

MObject DPAttributeSeExpr::aUserVector1;
MObject DPAttributeSeExpr::aUserVector2;
MObject DPAttributeSeExpr::aUserVector3;
MObject DPAttributeSeExpr::aUserVector4;

MObject DPAttributeSeExpr::aUserColor1;
MObject DPAttributeSeExpr::aUserColor2;
MObject DPAttributeSeExpr::aUserColor3;
MObject DPAttributeSeExpr::aUserColor4;

MObject DPAttributeSeExpr::outDD;

namespace {

struct SeExprParameters {
    double userFloats[4];
    MVector userVectors[4];
    MVector userColors[4];
    std::string expression;
};

bool hasFiniteParameters(const SeExprParameters& parameters) {
    for (std::size_t index = 0U; index < 4U; ++index) {
        if (!std::isfinite(parameters.userFloats[index]) ||
            !dommy::maya::isFiniteVector(parameters.userVectors[index]) ||
            !dommy::maya::isFloatRepresentableVector(parameters.userColors[index])) {
            return false;
        }
    }
    return true;
}

MStatus evaluateSeExpr(const DPointList* points, std::vector<Imath::V3d>& results,
                       const MTime& evaluationTime, const SeExprParameters& parameters,
                       DPData* pointData = nullptr) {
    if (!points || !hasFiniteParameters(parameters)) {
        return MS::kInvalidParameter;
    }

    DPointSeExpr expression(parameters.expression);
    if (!expression.isValid() || (!pointData && expression.usesPointDataFunctions())) {
        if (!parameters.expression.empty()) {
            DOMMY_MAYA_WARNING("DPAttributeSeExpr",
                               "expression failed: " << expression.parseError());
        }
        return MS::kInvalidParameter;
    }

    DOMMY_RETURN_IF_FAILURE(dommy::maya::setExpressionTime(expression, evaluationTime));
    if (pointData)
        expression.setDPData(pointData);

    expression.setUserFloat1(parameters.userFloats[0]);
    expression.setUserFloat2(parameters.userFloats[1]);
    expression.setUserFloat3(parameters.userFloats[2]);
    expression.setUserFloat4(parameters.userFloats[3]);

    if (!dommy::expression::setUserVectors(expression, parameters.userVectors))
        return MS::kInvalidParameter;

    expression.setUserColor1(DVec3Data(parameters.userColors[0][0], parameters.userColors[0][1],
                                       parameters.userColors[0][2]));
    expression.setUserColor2(DVec3Data(parameters.userColors[1][0], parameters.userColors[1][1],
                                       parameters.userColors[1][2]));
    expression.setUserColor3(DVec3Data(parameters.userColors[2][0], parameters.userColors[2][1],
                                       parameters.userColors[2][2]));
    expression.setUserColor4(DVec3Data(parameters.userColors[3][0], parameters.userColors[3][1],
                                       parameters.userColors[3][2]));

    return dommy::expression::evaluateResults(expression, *points, results) ? MS::kSuccess
                                                                            : MS::kFailure;
}

template <typename Owner>
MStatus evaluateAttribute(Owner& owner, const std::string& name, int dimension, const MTime& time,
                          const SeExprParameters& parameters, DPData* pointData = nullptr) {
    std::vector<Imath::V3d> results;
    DOMMY_RETURN_IF_FAILURE(
        evaluateSeExpr(owner.getPositionPtrConst(), results, time, parameters, pointData));
    return dommy::expression::publishResult(owner, name, dimension, results)
               ? MS::kSuccess
               : MS::kInvalidParameter;
}

} // namespace

void* DPAttributeSeExpr::creator() {
    return ::dommy::maya::createObject("DPAttributeSeExpr", [&]() {
        return new DPAttributeSeExpr();
    });
}

MStatus DPAttributeSeExpr::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDD)
        return MS::kUnknownParameter;
    MStatus status;
    const MDataHandle typeHandle = data.inputValue(aAttributeType, &status);
    if (!status)
        return status;
    const short attributeType = typeHandle.asShort();
    if (attributeType < DP_ATTR_DIM_SCALAR || attributeType > DP_ATTR_DIM_VEC3D)
        return MS::kInvalidParameter;
    const int dimension = attributeType == DP_ATTR_DIM_SCALAR  ? 1
                          : attributeType == DP_ATTR_DIM_VEC2D ? 2
                                                               : 3;
    const MDataHandle nameHandle = data.inputValue(aAttributeName, &status);
    if (!status)
        return status;
    const std::string attributeName = nameHandle.asString().asChar();
    if (!dommy::attribute::validAttributeName(attributeName) ||
        attributeName.size() > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;

    SeExprParameters parameters = {};
    const MDataHandle expressionHandle = data.inputValue(aExpression, &status);
    if (!status)
        return status;
    parameters.expression = expressionHandle.asString().asChar();
    if (parameters.expression.empty() ||
        parameters.expression.size() > dommy::generation::kMaxGeneratedStringByteCount)
        return MS::kInvalidParameter;
    const MObject floatAttributes[] = {aUserFloat1, aUserFloat2, aUserFloat3, aUserFloat4};
    const MObject vectorAttributes[] = {aUserVector1, aUserVector2, aUserVector3, aUserVector4};
    const MObject colorAttributes[] = {aUserColor1, aUserColor2, aUserColor3, aUserColor4};
    for (std::size_t i = 0U; i < 4U; ++i) {
        const MDataHandle floatHandle = data.inputValue(floatAttributes[i], &status);
        if (!status)
            return status;
        parameters.userFloats[i] = floatHandle.asDouble();
        const MDataHandle vectorHandle = data.inputValue(vectorAttributes[i], &status);
        if (!status)
            return status;
        parameters.userVectors[i] = vectorHandle.asVector();
        const MDataHandle colorHandle = data.inputValue(colorAttributes[i], &status);
        if (!status)
            return status;
        const MFloatVector color = colorHandle.asFloatVector();
        parameters.userColors[i] = MVector(color.x, color.y, color.z);
    }
    if (!hasFiniteParameters(parameters))
        return MS::kInvalidParameter;
    // Validate syntax even for an empty input collection.
    DPointSeExpr syntax(parameters.expression);
    if (!syntax.isValid())
        return MS::kInvalidParameter;
    MTime evaluationTime;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, evaluationTime));
    MDataHandle inputHandle = data.inputValue(inDD, &status);
    if (!status)
        return status;
    const MTypeId inputType = inputHandle.typeId();
    if (inputType == DPData::id) {
        const DPData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DPData::id, input));
        if (!input || !input->hasValidPointTopology() ||
            !dommy::attribute::validArrayConversionBudget(input->numberOfPoints(),
                                                          sizeof(Imath::V3d)))
            return MS::kInvalidParameter;
        status = dommy::maya::setPluginDataOutput<DPData>(
            data, outDD, DPData::id, [&](DPData& output) -> MStatus {
                output.setDPData(*input);
                return evaluateAttribute(output, attributeName, dimension, evaluationTime,
                                         parameters, &output);
            });
    } else if (inputType == DCurveData::id) {
        const DCurveData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DCurveData::id, input));
        if (!input || !input->hasValidCurveTopology())
            return MS::kInvalidParameter;
        std::size_t count = 0U;
        for (auto it = input->beginPrims(); it != input->endPrims(); ++it)
            if (!*it ||
                !dommy::generation::checkedSum(count, (*it)->numberOfPoints(),
                                               dommy::generation::kMaxGeneratedPointCount, count))
                return MS::kInvalidParameter;
        if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)))
            return MS::kInvalidParameter;
        status = dommy::maya::setPluginDataOutput<DCurveData>(
            data, outDD, DCurveData::id, [&](DCurveData& output) -> MStatus {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input));
                for (auto it = output.beginPrims(); it != output.endPrims(); ++it)
                    DOMMY_RETURN_IF_FAILURE(evaluateAttribute(**it, attributeName, dimension,
                                                              evaluationTime, parameters));
                return MS::kSuccess;
            });
    } else if (inputType == DMeshData::id) {
        const DMeshData* input = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(inputHandle, DMeshData::id, input));
        if (!input || !input->hasValidMeshTopology())
            return MS::kInvalidParameter;
        std::size_t count = 0U;
        for (auto it = input->beginPrims(); it != input->endPrims(); ++it)
            if (!*it ||
                !dommy::generation::checkedSum(count, (*it)->numberOfPoints(),
                                               dommy::generation::kMaxGeneratedPointCount, count))
                return MS::kInvalidParameter;
        if (!dommy::attribute::validArrayConversionBudget(count, sizeof(Imath::V3d)))
            return MS::kInvalidParameter;
        status = dommy::maya::setPluginDataOutput<DMeshData>(
            data, outDD, DMeshData::id, [&](DMeshData& output) -> MStatus {
                DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input));
                for (auto it = output.beginPrims(); it != output.endPrims(); ++it)
                    DOMMY_RETURN_IF_FAILURE(evaluateAttribute(**it, attributeName, dimension,
                                                              evaluationTime, parameters));
                return MS::kSuccess;
            });
    } else
        return MS::kInvalidParameter;
    if (!status)
        return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPAttributeSeExpr")

MStatus DPAttributeSeExpr::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnStringData fnStringData;
	MFnGenericAttribute outAttr;
	
	
    // ----------------------- INPUTS --------------------------
    inDD = gAttr.create( "inDD", "idd",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
	
	
	const MObject aAttributeNameDefaultData = fnStringData.create("newAttr", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aAttributeName = tAttr.create("AttributeName", "attrnm", MFnData::kString, aAttributeNameDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeName));
	
	
	MFnEnumAttribute enumFn;
	aAttributeType = enumFn.create( "AttributeType", "attrty", DP_ATTR_DIM_SCALAR, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Scalar", DP_ATTR_DIM_SCALAR ));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Vector 2D", DP_ATTR_DIM_VEC2D));
	DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Vector 3D", DP_ATTR_DIM_VEC3D ));
	DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeType));
	
	const MObject aExpressionDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aExpression = tAttr.create("Expression", "expr", MFnData::kString, aExpressionDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aExpression));
	
	
	aUserFloat1 = nAttr.create( "UserFloat1", "uf1", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserFloat1));
	
	aUserFloat2 = nAttr.create( "UserFloat2", "uf2", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserFloat2));
	
	aUserFloat3 = nAttr.create( "UserFloat3", "uf3", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserFloat3));
	
	aUserFloat4 = nAttr.create( "UserFloat4", "uf4", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserFloat4));
    
    
    aUserVector1 = nAttr.create( "UserVector1", "uv1", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUserVector1));
    
    aUserVector2 = nAttr.create( "UserVector2", "uv2", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUserVector2));
    
    aUserVector3 = nAttr.create( "UserVector3", "uv3", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUserVector3));
    
    aUserVector4 = nAttr.create( "UserVector4", "uv4", MFnNumericData::k3Double , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUserVector4));
    
	
	
	aUserColor1 = nAttr.createColor("UserColor1", "ucol1", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserColor1));
	
	aUserColor2 = nAttr.createColor("UserColor2", "ucol2", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserColor2));
	
	aUserColor3 = nAttr.createColor("UserColor3", "ucol3", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserColor3));
	
	aUserColor4 = nAttr.createColor("UserColor4", "ucol4", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0, 1.0, 1.0));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aUserColor4));
	
	
	
    //create custom data
    outDD = gAttr.create( "outDD", "odd", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeName, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeType, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aExpression, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserFloat1, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserFloat2, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserFloat3, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserFloat4, outDD ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserVector1, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserVector2, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserVector3, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserVector4, outDD ));
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserColor1, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserColor2, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserColor3, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aUserColor4, outDD ));
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPAttributeSeExpr", "initialize", std::current_exception());
}
