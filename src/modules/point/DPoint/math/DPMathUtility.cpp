//
//  DPMathUtility.cpp
//  dommyTool
//
//  Created by yuzo on 2/15/17.
//
//


#include "DPMathUtility.h"
#include "DMathArrayConversion.h"
#include "common/utility/DAttributeMathInput.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DAttributePublication.h"

#include <algorithm>
#include <array>

namespace {

bool isValidMathOperation(const D_CALC_MATH_ENUM operation)
{
    return operation >= D_CAL_ADD && operation < D_CAL_COUNT;
}

bool isValidSingleMathOperation(const D_SRC_SINGLE_MATH_ENUM operation)
{
    return operation >= D_SRC_NEGATIVE && operation < D_SRC_COUNT;
}


using MayaMathVector = std::array<double, 3>;

MStatus validateMathOperandBudget(MDataBlock& data, const MObject& constants,
                                  const MObject& inputs, const MObject& attributes,
                                  std::size_t count, std::size_t partitions = 1U)
{
    const MObject plugs[] = { constants, inputs, attributes };
    std::size_t operands = 0U;
    for (const MObject& attribute : plugs) {
        MStatus status;
        MArrayDataHandle array = data.inputArrayValue(attribute, &status);
        if (!status) return status;
        const unsigned int elements = array.elementCount(&status);
        if (!status) return status;
        if (!dommy::generation::checkedSum(operands, elements,
                dommy::generation::kMaxWorkItemCount, operands))
            return MS::kInvalidParameter;
    }
    return dommy::math::validMathBudget(count, operands, partitions)
        ? MS::kSuccess : MS::kInvalidParameter;
}

template <typename Value>
bool finiteMathValue(const Value& value) {
    using Components = dommy::numeric::ValueComponents<Value>;
    for (unsigned int axis = 0U; axis < Components::count; ++axis)
        if (!std::isfinite(Components::get(value, axis))) return false;
    return true;
}


template <typename Owner, typename Values>
MStatus readMathAttribute(const Owner& owner, const std::string& name, Values& values)
{
    return dommy::attribute::readNumericAttributeValues(owner, name, values.size(), values)
        ? MS::kSuccess : MS::kInvalidParameter;
}

template <typename Owner, typename Values>
MStatus replaceMathAttribute(Owner& owner, const std::string& name, const Values& values)
{
    return dommy::attribute::replaceNumericAttributeValues(owner, name, values)
        ? MS::kSuccess : MS::kInvalidParameter;
}


template <typename Owner, typename Values>
MStatus publishMathAttribute(Owner& owner, const std::string& sourceName,
                             const std::string& outputName, const Values& values,
                             bool deleteOriginal)
{
    return DPublishAttributeValues(owner, true, deleteOriginal, outputName, sourceName, values);
}

template <typename Operand, typename Result>
MStatus applyProceduralMathOperands(const std::vector<Operand>& operands,
                                    Result& result, int operation)
{
    for (const auto& value : result)
        if (!finiteMathValue(value)) return MS::kInvalidParameter;
    for (const Operand& operand : operands) {
        if (operand.size() != 1U && operand.size() != result.size())
            return MS::kInvalidParameter;
        for (const auto& value : operand)
            if (!finiteMathValue(value)) return MS::kInvalidParameter;
        const bool applied = dommy::math::applyMathOperand(result, operand,
            [operation](typename Result::value_type& lhs,
                        const typename Operand::value_type& rhs) {
                return dommy::math::applyProceduralBinary(operation, lhs, rhs);
            });
        if (!applied) return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

struct MayaMathValues {
    bool scalar = false;
    std::vector<MayaMathVector> values;
};

MStatus readMayaMathValues(const MDataHandle& handle, std::size_t expectedCount,
                           MayaMathValues& result)
{
    MayaMathValues candidate;
    candidate.scalar = dommy::maya::isScalarNumericInput(handle);
    MStatus status;
    if (candidate.scalar) {
        std::vector<double> scalar;
        status = dommy::maya::readScalarInput(handle, expectedCount, scalar, true);
        if (!status) return status;
        if (!dommy::math::validMathBudget(scalar.size(), 0U)) return MS::kInvalidParameter;
        candidate.values.reserve(scalar.size());
        for (double value : scalar)
            candidate.values.push_back(MayaMathVector{{value, value, value}});
    } else {
        status = dommy::maya::readVectorInput(handle, expectedCount, candidate.values);
        if (!status) return status;
        if (!dommy::math::validMathBudget(candidate.values.size(), 0U)) return MS::kInvalidParameter;
    }
    result = std::move(candidate);
    return MS::kSuccess;
}

MStatus applyMayaMathOperand(int operation, const MayaMathValues& operand,
                            MayaMathValues& result)
{
    const unsigned int components = result.scalar ? 1U : 3U;
    const bool applied = dommy::math::applyMathOperand(result.values, operand.values,
        [&](MayaMathVector& lhs, const MayaMathVector& rhs) {
            for (unsigned int axis = 0U; axis < components; ++axis) {
                if (!dommy::math::checkedMathBinary(operation, lhs[axis],
                        rhs[operand.scalar ? 0U : axis], lhs[axis])) return false;
            }
            return true;
        });
    return applied ? MS::kSuccess : MS::kInvalidParameter;
}

template <typename Array, typename Number>
MStatus convertMayaMathScalars(const MayaMathValues& values, Array& output)
{
    return dommy::math::writeScalarMathArray<Number>(values.values.size(),
        [&](std::size_t i) { return values.values[i][0]; }, output);
}

MStatus publishMayaMathValues(MFnData::Type type, const MayaMathValues& values,
                              MDataBlock& data, const MPlug& plug,
                              const MObject& outputAttribute)
{
    MStatus status;
    if (values.scalar && type != MFnData::kFloatArray &&
        type != MFnData::kDoubleArray && type != MFnData::kIntArray)
        type = MFnData::kNumeric;
    switch (type) {
    case MFnData::kNumeric: {
        if (values.values.size() != 1U || !std::isfinite(values.values[0][0]))
            return MS::kInvalidParameter;
        MDataHandle output = data.outputValue(outputAttribute, &status);
        if (!status) return status;
        bool numeric = false, null = false;
        if (output.isGeneric(numeric, null)) output.setGenericDouble(values.values[0][0], true);
        else output.setDouble(values.values[0][0]);
        output.setClean();
        return data.setClean(plug);
    }
    case MFnData::kFloatArray: {
        MFloatArray output;
        DOMMY_RETURN_IF_FAILURE((convertMayaMathScalars<MFloatArray, float>(values, output)));
        return DOutputDataMayaFloatArray(data, plug, outputAttribute, output);
    }
    case MFnData::kDoubleArray: {
        MDoubleArray output;
        DOMMY_RETURN_IF_FAILURE((convertMayaMathScalars<MDoubleArray, double>(values, output)));
        return DOutputDataMayaDoubleArray(data, plug, outputAttribute, output);
    }
    case MFnData::kIntArray: {
        MIntArray output;
        DOMMY_RETURN_IF_FAILURE((convertMayaMathScalars<MIntArray, int>(values, output)));
        return DOutputDataMayaIntArray(data, plug, outputAttribute, output);
    }
    case MFnData::kVectorArray: {
        MVectorArray output;
        DOMMY_RETURN_IF_FAILURE(output.setLength(static_cast<unsigned int>(values.values.size())));
        for (unsigned int i = 0U; i < output.length(); ++i) {
            const auto& value = values.values[i];
            output[i] = MVector(value[0], value[1], value[2]);
        }
        return DOutputDataMayaVectorArray(data, plug, outputAttribute, output);
    }
    case MFnData::kPointArray: {
        MPointArray output;
        DOMMY_RETURN_IF_FAILURE(output.setLength(static_cast<unsigned int>(values.values.size())));
        for (unsigned int i = 0U; i < output.length(); ++i) {
            const auto& value = values.values[i];
            output[i] = MPoint(value[0], value[1], value[2]);
        }
        return DOutputDataMayaPointArray(data, plug, outputAttribute, output);
    }
    default: return MS::kInvalidParameter;
    }
}

MStatus evaluateMayaMathInputs(MDataBlock& data, const MObject& constantAttribute,
                               const MObject& inputAttribute, int operation,
                               MayaMathValues& result)
{
    MStatus status;
    MArrayDataHandle constants = data.inputArrayValue(constantAttribute, &status);
    if (!status) return status;
    const unsigned int constantCount = constants.elementCount(&status);
    if (!status) return status;
    MArrayDataHandle inputs = data.inputArrayValue(inputAttribute, &status);
    if (!status) return status;
    const unsigned int inputCount = inputs.elementCount(&status);
    if (!status) return status;
    std::size_t operands = 0U;
    if (!dommy::generation::checkedSum(constantCount, inputCount,
            dommy::generation::kMaxWorkItemCount, operands) ||
        !dommy::math::validMathBudget(result.values.size(), operands))
        return MS::kInvalidParameter;

    // Preserve the historical grouping: scalar inputs, vector constants,
    // then vector inputs. Stream one operand at a time, including constants.
    for (unsigned int phase = 0U; phase < 3U; ++phase) {
        MArrayDataHandle& array = phase == 1U ? constants : inputs;
        const unsigned int count = phase == 1U ? constantCount : inputCount;
        for (unsigned int i = 0U; i < count; ++i) {
            DOMMY_RETURN_IF_FAILURE(array.jumpToArrayElement(i));
            const MDataHandle handle = array.inputValue(&status);
            if (!status) return status;
            MayaMathValues operand;
            if (phase == 1U) {
                const double3& value = handle.asDouble3();
                if (!std::isfinite(value[0]) || !std::isfinite(value[1]) ||
                    !std::isfinite(value[2])) return MS::kInvalidParameter;
                operand.scalar = false;
                operand.values.push_back(MayaMathVector{{value[0], value[1], value[2]}});
            } else {
                const bool scalar = dommy::maya::isScalarNumericInput(handle);
                const bool vector = isInputPlugDataForVector(handle.type());
                if (!scalar && !vector) return MS::kInvalidParameter;
                if (scalar != (phase == 0U)) continue;
                DOMMY_RETURN_IF_FAILURE(readMayaMathValues(
                    handle, result.values.size(), operand));
            }
            DOMMY_RETURN_IF_FAILURE(applyMayaMathOperand(operation, operand, result));
        }
    }
    return MS::kSuccess;
}

MStatus readConstantVectorTarget(MDataBlock& data,
                                 const MObject& plug,
                                 const size_t count,
                                 DVec3DataList& values)
{
    MStatus status;
    const MDataHandle handle = data.inputValue(plug, &status);
    if (!status) return status;
    const double3& value = handle.asDouble3();
    if (!dommy::math::validMathBudget(count, 1U)) return MS::kInvalidParameter;
    DVec3Data converted;
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!dommy::numeric::checkedFloatCast(value[axis], converted[axis]))
            return MS::kInvalidParameter;
    values.assign(count, converted);
    return MS::kSuccess;
}


MStatus publishMathDataObject(MDataBlock& data,
                              const MObject& outputAttribute,
                              const MObject& dataObject,
                              const MPlug& requestedPlug)
{
    MStatus status;
    MDataHandle output = data.outputValue(outputAttribute, &status);
    if (!status) return status;

    status = output.set(dataObject);
    if (!status) return status;

    output.setClean();
    return data.setClean(requestedPlug);
}

} // namespace

// Keep one immutable dispatch table per plug-in binary.  The operation
// implementations remain inline in the header because they are small,
// overloaded hot-path helpers used directly by several math nodes.
DScalarData (* const ScalarMathFncForSourceOps[])(DScalarData) = {
    DNegative, DSqrt, DFloor, DRound, DCeil, DExponential, DAbsolute
};
DVec2Data (* const Vec2DMathFncForSourceOps[])(DVec2Data) = {
    DNegative, DSqrt, DFloor, DRound, DCeil, DExponential, DAbsolute
};
DVec3Data (* const Vec3DMathFncForSourceOps[])(DVec3Data) = {
    DNegative, DSqrt, DFloor, DRound, DCeil, DExponential, DAbsolute
};

DScalarData (* const ScalarMathFncForScalarOps[])(const DScalarData&, const DScalarData&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};
DScalarData (* const ScalarMathFncForVec2Ops[])(const DScalarData&, const DVec2Data&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};
DScalarData (* const ScalarMathFncForVectorOps[])(const DScalarData&, const DVec3Data&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};

DVec2Data (* const Vec2DMathFncForScalarOps[])(const DVec2Data&, const DScalarData&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};
DVec2Data (* const Vec2DMathFncForVec2DOps[])(const DVec2Data&, const DVec2Data&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};
DVec2Data (* const Vec2DMathFncForVectorOps[])(const DVec2Data&, const DVec3Data&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};

DVec3Data (* const Vec3DMathFncForScalarOps[])(const DVec3Data&, const DScalarData&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};
DVec3Data (* const Vec3DMathFncForVec2DOps[])(const DVec3Data&, const DVec2Data&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};
DVec3Data (* const Vec3DMathFncForVectorOps[])(const DVec3Data&, const DVec3Data&) = {
    DAdd, DSub, DMult, DDiv, DMod, DPow, DMin, DMax
};

DScalarData (* const Vec3DMathFncForSingleOps[])(DVec3Data) = {
    DLength, DMinComponent, DMaxComponent, DAverageComponent
};
DScalarData (* const MVec3DMathFncForSingleOps[])(MVector) = {
    DLength, DMinComponent, DMaxComponent, DAverageComponent
};


#pragma mark Get constant value


MStatus DPMGetConstantValues(
    MDataBlock& data, const MObject& plug, const size_t& count,
    std::vector<DMathVec3DataList>& targetVectorList)
{
    MStatus status;
    MArrayDataHandle inputs = data.inputArrayValue(plug, &status);
    if (!status) return status;
    const unsigned int elementCount = inputs.elementCount(&status);
    if (!status) return status;
    std::size_t operandCount = 0U;
    if (!dommy::generation::checkedSum(targetVectorList.size(), elementCount,
            dommy::generation::kMaxWorkItemCount, operandCount) ||
        !dommy::math::validMathBudget(count, operandCount))
        return MS::kInvalidParameter;
    for (unsigned int i = 0U; i < elementCount; ++i) {
        DOMMY_RETURN_IF_FAILURE(inputs.jumpToArrayElement(i));
        const MDataHandle handle = inputs.inputValue(&status);
        if (!status) return status;
        const double3& raw = handle.asDouble3();
        DMathVec3Data value;
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!dommy::numeric::checkedNumericCast(raw[axis], value[axis]))
                return MS::kInvalidParameter;
        // One value is explicitly broadcast by the math evaluator.
        targetVectorList.push_back(DMathVec3DataList(1U, value));
    }
    return MS::kSuccess;
}

MStatus DPMGetInputDataValues(
    MDataBlock& data, const MObject& plug, const size_t& count,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec3DataList>& targetVectorList)
{
    MStatus status;
    MArrayDataHandle inputs = data.inputArrayValue(plug, &status);
    if (!status) return status;
    const unsigned int elementCount = inputs.elementCount(&status);
    if (!status) return status;
    std::size_t operands = 0U;
    if (!dommy::generation::checkedSum(targetScalarList.size(), targetVectorList.size(),
            dommy::generation::kMaxWorkItemCount, operands) ||
        !dommy::generation::checkedSum(operands, elementCount,
            dommy::generation::kMaxWorkItemCount, operands) ||
        !dommy::math::validMathBudget(count, operands))
        return MS::kInvalidParameter;
    for (unsigned int i = 0U; i < elementCount; ++i) {
        DOMMY_RETURN_IF_FAILURE(inputs.jumpToArrayElement(i));
        const MDataHandle handle = inputs.inputValue(&status);
        if (!status) return status;
        if (dommy::maya::isScalarNumericInput(handle)) {
            DMathScalarDataList values;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::readScalarInput(
                handle, count, values, true));
            targetScalarList.push_back(std::move(values));
        } else if (isInputPlugDataForVector(handle.type())) {
            DMathVec3DataList values;
            DOMMY_RETURN_IF_FAILURE(dommy::maya::readVectorInput(handle, count, values));
            targetVectorList.push_back(std::move(values));
        } else {
            return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}


#pragma mark Get DPoint attribute


MStatus DPMGetDPointAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    const size_t& count,
    DPData& dpdata,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec2DataList>& targetVec2DList,
    std::vector<DMathVec3DataList>& targetVectorList)
{
    MStatus stat;
    
    MArrayDataHandle targetAttributeHnd = data.inputArrayValue(plug, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
				
    const unsigned int elementCount =
        targetAttributeHnd.elementCount(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (unsigned int i = 0U; i < elementCount; ++i) {
        DOMMY_RETURN_IF_FAILURE(
            targetAttributeHnd.jumpToArrayElement(i));
        MDataHandle hnd = targetAttributeHnd.inputValue(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        const std::string tarAttrName = hnd.asString().asChar();
        
        
        if(dpdata.isExistAttr(tarAttrName)) {
            
            const auto checkedAttribute = dpdata.getAttr(tarAttrName);
            if (!checkedAttribute) return MS::kInvalidParameter;
            const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
            
            if (isVector3DAttrType(type)) {
                
                DMathVec3DataList values(count, DMathVec3Data(0,0,0));
                DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, tarAttrName, values));
                
                targetVectorList.push_back(values);
                
            } else if (isVector2DAttrType(type)) {
                
                DMathVec2DataList values(count, DMathVec2Data(0,0));
                
                DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, tarAttrName, values));
                targetVec2DList.push_back(values);
                
            } else if (isSingleRealAttrType(type)) {
                
                DMathScalarDataList values(count, 0.0);
                
                DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, tarAttrName, values));
                targetScalarList.push_back(values);
                
            } else {
                EchoWarning(tarAttrName, "Not Suppot target Attribute type.");
            }
        }
    }
    return MS::kSuccess;
}


#pragma mark Get DCurve prim attribute


MStatus DPMGetDCurvePrimAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DCurveData& dcdata,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec2DataList>& targetVec2DList,
    std::vector<DMathVec3DataList>& targetVectorList)
{
    MStatus stat;
    
    MArrayDataHandle targetAttributeHnd = data.inputArrayValue(plug, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    std::vector<std::string> notSupportType;
    
    
    const unsigned int elementCount =
        targetAttributeHnd.elementCount(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    for (unsigned int i = 0U; i < elementCount; ++i) {
        DOMMY_RETURN_IF_FAILURE(
            targetAttributeHnd.jumpToArrayElement(i));
        MDataHandle hnd = targetAttributeHnd.inputValue(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        const std::string tarAttrName = hnd.asString().asChar();
        
        if(dcdata.isExistAttr(tarAttrName)) {
            
            uint count = dcdata.numberOfCurves();
            
            const auto checkedAttribute = dcdata.getAttr(tarAttrName);
            if (!checkedAttribute) return MS::kInvalidParameter;
            const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
            
            
            if (isVector3DAttrType(type)) {
                
                //add vector type
                
                DMathVec3DataList values(count, DMathVec3Data(0,0,0));
                DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, tarAttrName, values));
                
                targetVectorList.push_back(values);
                
                
            } else if (isVector2DAttrType(type)) {
                
                //add 2d vector type
                DMathVec2DataList values(count, DMathVec2Data(0,0));
                DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, tarAttrName, values));
                
                targetVec2DList.push_back(values);
                
                
            } else if (isSingleRealAttrType(type)) {
                
                //add scalar type
                DMathScalarDataList values(count, 0.0);
                DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, tarAttrName, values));
                
                targetScalarList.push_back(values);
                
            } else {
                notSupportType.push_back(tarAttrName);
                
            }
            
        }
    }
    
    if (notSupportType.size()) {
        for (size_t n=0; n<notSupportType.size(); ++n) {
            EchoWarning(notSupportType[n], "Not Suppot target Attribute type.");
        }
        
    }
    return MS::kSuccess;
}


#pragma mark Get DCurve point attribute

MStatus DPMGetDCurvePointAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DCurveData& dcdata,
    std::vector<std::vector<DMathScalarDataList> >& targetScalarList,
    std::vector<std::vector<DMathVec2DataList> >& targetVec2DList,
    std::vector<std::vector<DMathVec3DataList> >& targetVectorList)
{
    MStatus stat;
    
	MArrayDataHandle targetAttributeHnd = data.inputArrayValue(plug, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	std::vector<std::string> notSupportType;
	
	
    const unsigned int elementCount =
        targetAttributeHnd.elementCount(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	for (unsigned int i = 0U; i < elementCount; ++i) {
		DOMMY_RETURN_IF_FAILURE(
            targetAttributeHnd.jumpToArrayElement(i));
		MDataHandle hnd = targetAttributeHnd.inputValue(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		
		const std::string tarAttrName = hnd.asString().asChar();
		
		
		if(dcdata.isExistPointAttr(tarAttrName)) {
			
			size_t curveOffset = 0;
			for (auto cIt=dcdata.beginCurves(),ecIt=dcdata.endCurves(); cIt!=ecIt; ++cIt) {
				
				const auto checkedAttribute = (*cIt)->getAttr(tarAttrName);
				if (!checkedAttribute) return MS::kInvalidParameter;
				const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
				
				//curve data
                
				uint count = (*cIt)->numberOfPoints();
				
				if (isVector3DAttrType(type)) {
					
					//add vector type
					
					DMathVec3DataList values(count, DMathVec3Data(0,0,0));
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), tarAttrName, values));
					
					targetVectorList[curveOffset].push_back(values);
					
					
                } else if (isVector2DAttrType(type)) {
                    
                    //add vector type
                    
                    DMathVec2DataList values(count, DMathVec2Data(0,0));
                    DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), tarAttrName, values));
                    
                    targetVec2DList[curveOffset].push_back(values);
                    
                    
                } else if (isSingleRealAttrType(type)) {
					
					//add scalar type
					
					DMathScalarDataList values(count, 0.0);
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), tarAttrName, values));
					
					targetScalarList[curveOffset].push_back(values);
					
				} else {
					notSupportType.push_back(tarAttrName);
					
				}
				
                
				curveOffset++;
			}
			
		}
	}
	
	if (notSupportType.size()) {
		for (size_t n=0; n<notSupportType.size(); ++n) {
			EchoWarning(notSupportType[n], "Not Suppot target Attribute type.");
		}
		
	}
	return MS::kSuccess;
}


#pragma mark DMesh Prim Attribute

MStatus DPMGetDMeshPrimAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DMeshData& dmdata,
    std::vector<DMathScalarDataList>& targetScalarList,
    std::vector<DMathVec2DataList>& targetVec2DList,
    std::vector<DMathVec3DataList>& targetVectorList)
{
    MStatus stat;
	
	MArrayDataHandle targetAttributeHnd = data.inputArrayValue(plug, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	std::vector<std::string> notSupportType;
	
	
    const unsigned int elementCount =
        targetAttributeHnd.elementCount(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	for (unsigned int i = 0U; i < elementCount; ++i) {
		DOMMY_RETURN_IF_FAILURE(
            targetAttributeHnd.jumpToArrayElement(i));
		MDataHandle hnd = targetAttributeHnd.inputValue(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		
		const std::string tarAttrName = hnd.asString().asChar();
		
		if(dmdata.isExistAttr(tarAttrName)) {
			
			uint count = dmdata.numberOfMesh();
			
			const auto checkedAttribute = dmdata.getAttr(tarAttrName);
			if (!checkedAttribute) return MS::kInvalidParameter;
			const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
			
			
			if (isVector3DAttrType(type)) {
				
				//add vector type
				
				DMathVec3DataList values(count, DMathVec3Data(0,0,0));
				DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, tarAttrName, values));
				
				targetVectorList.push_back(values);
				
				
            } else if (isVector2DAttrType(type)) {
                
                //add vector type
                
                DMathVec2DataList values(count, DMathVec2Data(0,0));
                DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, tarAttrName, values));
                
                targetVec2DList.push_back(values);
                
                
            } else if (isSingleRealAttrType(type)) {
				
				//add scalar type
				
				DMathScalarDataList values(count, 0.0);
				DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, tarAttrName, values));
				
				targetScalarList.push_back(values);
				
			} else {
				notSupportType.push_back(tarAttrName);
				
			}
			
		}
	}
	
	if (notSupportType.size()) {
		for (size_t n=0; n<notSupportType.size(); ++n) {
			EchoWarning(notSupportType[n], "Not Suppot target Attribute type.");
		}
		
	}
	return MS::kSuccess;
}


#pragma mark DMesh Point Attribute

MStatus DPMGetDMeshPointAttributeValues(
    MDataBlock& data,
    const MObject& plug,
    DMeshData& dmdata,
    std::vector<std::vector<DMathScalarDataList> >& targetScalarList,
    std::vector<std::vector<DMathVec2DataList> >& targetVec2DList,
    std::vector<std::vector<DMathVec3DataList> >& targetVectorList)
{
	MStatus stat;
	
	MArrayDataHandle targetAttributeHnd = data.inputArrayValue(plug, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	
	std::vector<std::string> notSupportType;
	
	
    const unsigned int elementCount =
        targetAttributeHnd.elementCount(&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	for (unsigned int i = 0U; i < elementCount; ++i) {
		DOMMY_RETURN_IF_FAILURE(
            targetAttributeHnd.jumpToArrayElement(i));
		MDataHandle hnd = targetAttributeHnd.inputValue(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
		
		const std::string tarAttrName = hnd.asString().asChar();
		
		
		if(dmdata.isExistPointAttr(tarAttrName)) {
			
			size_t meshOffset = 0;
			for (auto cIt=dmdata.beginMeshes(); cIt!=dmdata.endMeshes(); ++cIt) {
				
				const auto checkedAttribute = (*cIt)->getAttr(tarAttrName);
				if (!checkedAttribute) return MS::kInvalidParameter;
				const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
				
				//curve data
				
				uint count = (*cIt)->numberOfPoints();
				
				if (isVector3DAttrType(type)) {
					
					//add vector type
					
					DMathVec3DataList values(count, DMathVec3Data(0,0,0));
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), tarAttrName, values));
					
					targetVectorList[meshOffset].push_back(values);
					
					
                } else if (isVector2DAttrType(type)) {
                    
                    //add vector type
                    
                    DMathVec2DataList values(count, DMathVec2Data(0,0));
                    DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), tarAttrName, values));
                    
                    targetVec2DList[meshOffset].push_back(values);
                    
                    
                } else if (isSingleRealAttrType(type)) {
					
					//add scalar type
					
					DMathScalarDataList values(count, 0.0);
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), tarAttrName, values));
					
					targetScalarList[meshOffset].push_back(values);
					
				} else {
					notSupportType.push_back(tarAttrName);
				}
				
				
				meshOffset++;
			}
			
		}
	}
	
	if (notSupportType.size()) {
		for (size_t n=0; n<notSupportType.size(); ++n) {
			EchoWarning(notSupportType[n], "Not Suppot target Attribute type.");
		}
		
	}
	return MS::kSuccess;
}


#pragma mark DPoint single arg process

MStatus processDPointMathFncSingle(DPData &dpdata,
                                const std::string &nodeName,
                                const std::string &sourceAttrName,
                                const std::string &outAttrName,
                                const bool &isOutAttr,
                                const bool &isDeleteOrg,
                                MDataBlock&,
                                const D_SRC_SINGLE_MATH_ENUM &calcType) {
	//get total count
    const std::size_t count = dpdata.numberOfPoints();
	
	//get Source Data
    if (const auto sourceAttribute = dpdata.getAttr(sourceAttrName)) {
        
        DP_ATTR_DATA_TYPE type = sourceAttribute->getAttrType();
        
        if (isSingleRealAttrType(type)) {
			
			
			//Source is Scalar type
			DMathScalarDataList result(count, 0.0);
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, sourceAttrName, result));
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcScalarValue(calcType, result));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dpdata, sourceAttrName, outAttrName, result, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dpdata, sourceAttrName, result));
				
			}
			
			
        } else if (isVector2DAttrType(type)) {
			
			
			//Source is Vector type
			DMathVec2DataList result(count, DMathVec2Data(0.0,0.0));
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, sourceAttrName, result));
			
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcVec2DValue(calcType, result));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dpdata, sourceAttrName, outAttrName, result, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dpdata, sourceAttrName, result));
				
			}


        }  else if (isVector3DAttrType(type)) {
			
			
			//Source is Vector type
			DMathVec3DataList result(count, DMathVec3Data(0.0,0.0,0.0));
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, sourceAttrName, result));
			
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcVec3DValue(calcType, result));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dpdata, sourceAttrName, outAttrName, result, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dpdata, sourceAttrName, result));
				
			}


		} else {
			EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
		}
	}


    return MS::kSuccess;
}


#pragma mark DCurve single arg process

MStatus processDCurveMathFncSingle(DCurveData &dcdata,
                                const std::string &nodeName,
                                const std::string &sourceAttrName,
                                const std::string &outAttrName,
                                const bool &isOutAttr,
                                const bool &isDeleteOrg,
                                MDataBlock&,
                                const D_SRC_SINGLE_MATH_ENUM &calcType) {

	
    size_t curveCount = dcdata.numberOfCurves();
	
    if (const auto sourceAttribute = dcdata.getAttr(sourceAttrName)) {
		
		//primitive attribute


		//Get Source
        if (isSingleRealAttrType(sourceAttribute->getAttrType())) {
			
			//Source is Scalar type
			DMathScalarDataList values(curveCount, 0.0);
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, sourceAttrName, values));
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcScalarValue(calcType, values));
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dcdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dcdata, sourceAttrName, values));
				
			}
			
			
        } else if (isVector2DAttrType(sourceAttribute->getAttrType())) {
			
			//Source is Vector type
			DMathVec2DataList values(curveCount, DMathVec2Data(0.0,0.0));
			
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, sourceAttrName, values));
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcVec2DValue(calcType, values));
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dcdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dcdata, sourceAttrName, values));
				
			}
			
			
        } else if (isVector3DAttrType(sourceAttribute->getAttrType())) {
			
			
			//Source is Vector type
			DMathVec3DataList values(curveCount, DMathVec3Data(0.0,0.0,0.0));
			
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, sourceAttrName, values));
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcVec3DValue(calcType, values));
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dcdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dcdata, sourceAttrName, values));
				
			}
			
		} else {
			EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
		}
		
		
    } else if (dcdata.isExistPointAttr(sourceAttrName)) {
		
		//point attribute


		//get Source Data
        if (dcdata.isExistPointAttr(sourceAttrName)) {
			
            for (auto cIt=dcdata.beginCurves(); cIt!=dcdata.endCurves(); ++cIt) {
				
				size_t cvCount = (*cIt)->numberOfPoints();
				
				
				const auto checkedAttribute = (*cIt)->getAttr(sourceAttrName);
				if (!checkedAttribute) return MS::kInvalidParameter;
				const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
				
				
				if (isSingleRealAttrType(type)) {
					
					
					DMathScalarDataList values(cvCount, 0.0);
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					//run calc
					DOMMY_RETURN_IF_FAILURE(sourceCalcScalarValue(calcType, values));
					
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
					
				} else if (isVector2DAttrType(type)) {


					DMathVec2DataList values(cvCount, DMathVec2Data(0.0,0.0));
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					
					//run calc
					DOMMY_RETURN_IF_FAILURE(sourceCalcVec2DValue(calcType, values));
					
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
				} else if (isVector3DAttrType(type)) {


					DMathVec3DataList values(cvCount, DMathVec3Data(0.0,0.0,0.0));
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					
					//run calc
					DOMMY_RETURN_IF_FAILURE(sourceCalcVec3DValue(calcType, values));
					
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
				} else {
					EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
					break;
				}//attr type check
				
			} //curve loop
			
		}
		
	}
    return MS::kSuccess;
}

#pragma mark DMesh single arg process

MStatus processDMeshMathFncSingle(DMeshData &dmdata,
                               const std::string &nodeName,
                               const std::string &sourceAttrName,
                               const std::string &outAttrName,
                               const bool &isOutAttr,
                               const bool &isDeleteOrg,
                               MDataBlock&,
                               const D_SRC_SINGLE_MATH_ENUM &calcType) {


    size_t meshCount = dmdata.numberOfMesh();
	
    if (const auto sourceAttribute = dmdata.getAttr(sourceAttrName)) {
		
		//primitive attribute


		//Get Source
        if (isSingleRealAttrType(sourceAttribute->getAttrType())) {
			
			//Source is Scalar type
			
			DMathScalarDataList values(meshCount, 0.0);
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, sourceAttrName, values));
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcScalarValue(calcType, values));
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dmdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dmdata, sourceAttrName, values));
				
			}
			
			
        } else if (isVector2DAttrType(sourceAttribute->getAttrType())) {
			
			
			//Source is Vector type
			DMathVec2DataList values(meshCount, DMathVec2Data(0.0,0.0));
			
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, sourceAttrName, values));
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcVec2DValue(calcType, values));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dmdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dmdata, sourceAttrName, values));
				
			}


        } else if (isVector3DAttrType(sourceAttribute->getAttrType())) {
			
			
			//Source is Vector type
			DMathVec3DataList values(meshCount, DMathVec3Data(0.0,0.0,0.0));
			
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, sourceAttrName, values));
			
			//run calc
			DOMMY_RETURN_IF_FAILURE(sourceCalcVec3DValue(calcType, values));
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dmdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dmdata, sourceAttrName, values));
				
			}
			
		} else {
			EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
		}


    } else if (dmdata.isExistPointAttr(sourceAttrName)) {
		
		//point attribute


		//get Source Data
        if (dmdata.isExistPointAttr(sourceAttrName)) {
			
			
            for (auto cIt=dmdata.beginMeshes(); cIt!=dmdata.endMeshes(); ++cIt) {
				
				size_t vtxCount = (*cIt)->numberOfPoints();
				
				
				const auto checkedAttribute = (*cIt)->getAttr(sourceAttrName);
				if (!checkedAttribute) return MS::kInvalidParameter;
				const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
				
				
				if (isSingleRealAttrType(type)) {
					
					
					DMathScalarDataList values(vtxCount, 0.0);
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					//run calc
					DOMMY_RETURN_IF_FAILURE(sourceCalcScalarValue(calcType, values));
					
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
					
				} else if (isVector2DAttrType(type)) {


					DMathVec2DataList values(vtxCount, DMathVec2Data(0.0,0.0));
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					
					//run calc
					DOMMY_RETURN_IF_FAILURE(sourceCalcVec2DValue(calcType, values));
					
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
				} else if (isVector3DAttrType(type)) {
					
					
                    DMathVec3DataList values(vtxCount, DMathVec3Data(0.0,0.0,0.0));
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					
					//run calc
					DOMMY_RETURN_IF_FAILURE(sourceCalcVec3DValue(calcType, values));
					
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
				} else {
					EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
					break;
				}//attr type check
				
				
			} //mesh loop
			
		}
		
	}
	
    return MS::kSuccess;
}

MStatus processDDataMathOutputSingle(
    const MObject& inputDataAttribute,
    const MObject& outputDataAttribute,
    const std::string& nodeName,
    const std::string& sourceAttrName,
    const std::string& outAttrName,
    const bool isOutAttr,
    const bool isDeleteOrg,
    MDataBlock& data,
    const MPlug& requestedPlug,
    const D_SRC_SINGLE_MATH_ENUM calcType)
{
    if (!isValidSingleMathOperation(calcType)) {
        EchoWarning(nodeName, "Invalid single-input math operation.");
        return MS::kInvalidParameter;
    }

    MStatus status;
    MDataHandle input = data.inputValue(inputDataAttribute, &status);
    if (!status) return status;
    if (input.data().isNull()) {
        EchoWarning(nodeName, "Input data is null.");
        return MS::kInvalidParameter;
    }

    const unsigned int inputTypeId = input.typeId().id();
    if (inputTypeId == ID_DP_DATA) {
        const DPData* inputData = nullptr;
        status = getValidInputDP(inputDataAttribute, data, inputData);
        if (!status) return status;
        if (!inputData) return MS::kFailure;

        MObject outputObject;
        DPData* outputData = nullptr;
        status = dommy::maya::createPluginDataObject<DPData>(
            DPData::id, outputObject, outputData);
        if (!status) return status;
        if (!outputData) return MS::kFailure;

        outputData->setDPData(*inputData);
        DOMMY_RETURN_IF_FAILURE(processDPointMathFncSingle(
            *outputData, nodeName, sourceAttrName, outAttrName,
            isOutAttr, isDeleteOrg, data, calcType));
        if (!outputData->hasValidPointTopology()) return MS::kFailure;

        return publishMathDataObject(
            data, outputDataAttribute, outputObject, requestedPlug);
    }

    if (inputTypeId == ID_DC_DATA) {
        const DCurveData* inputData = nullptr;
        status = getValidInputDC(inputDataAttribute, data, inputData);
        if (!status) return status;
        if (!inputData) return MS::kFailure;

        MObject outputObject;
        DCurveData* outputData = nullptr;
        status = dommy::maya::createPluginDataObject<DCurveData>(
            DCurveData::id, outputObject, outputData);
        if (!status) return status;
        if (!outputData) return MS::kFailure;

        DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*inputData));
        if (outputData->numberOfCurves() != 0U) {
            DOMMY_RETURN_IF_FAILURE(processDCurveMathFncSingle(
                *outputData, nodeName, sourceAttrName, outAttrName,
                isOutAttr, isDeleteOrg, data, calcType));
        }
        if (!outputData->hasValidCurveTopology()) return MS::kFailure;

        return publishMathDataObject(
            data, outputDataAttribute, outputObject, requestedPlug);
    }

    if (inputTypeId == ID_DM_DATA) {
        const DMeshData* inputData = nullptr;
        status = getValidInputDM(inputDataAttribute, data, inputData);
        if (!status) return status;
        if (!inputData) return MS::kFailure;

        MObject outputObject;
        DMeshData* outputData = nullptr;
        status = dommy::maya::createPluginDataObject<DMeshData>(
            DMeshData::id, outputObject, outputData);
        if (!status) return status;
        if (!outputData) return MS::kFailure;

        DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*inputData));
        if (outputData->numberOfMesh() != 0U) {
            DOMMY_RETURN_IF_FAILURE(processDMeshMathFncSingle(
                *outputData, nodeName, sourceAttrName, outAttrName,
                isOutAttr, isDeleteOrg, data, calcType));
        }
        if (!outputData->hasValidMeshTopology()) return MS::kFailure;

        return publishMathDataObject(
            data, outputDataAttribute, outputObject, requestedPlug);
    }

    EchoWarning(nodeName, "Input is not supported procedural data.");
    return MS::kInvalidParameter;
}


#pragma mark InputData single arg process

MStatus processMayaNumericMathFuncSingle(MObject,
                                         const std::string &nodeName,
                                         const MObject &targetSrcDataPlug,
                                         const MObject &outdataPlug,
                                         MDataBlock& data,
                                         const MPlug& plug,
                                         const D_SRC_SINGLE_MATH_ENUM &calcType) {
    if (!isValidSingleMathOperation(calcType)) {
        EchoWarning(nodeName, "Invalid single-input math operation.");
        return MS::kInvalidParameter;
    }
    MStatus status;
    const MDataHandle input = data.inputValue(targetSrcDataPlug, &status);
    if (!status) return status;
    MayaMathValues result;
    DOMMY_RETURN_IF_FAILURE(readMayaMathValues(
        input, dommy::maya::kInferInputCount, result));
    if (!dommy::math::validMathBudget(result.values.size(), 0U))
        return MS::kInvalidParameter;
    const unsigned int components = result.scalar ? 1U : 3U;
    for (auto& value : result.values) {
        for (unsigned int axis = 0U; axis < components; ++axis) {
            if (!dommy::math::checkedMathUnary(calcType, value[axis], value[axis]))
                return MS::kInvalidParameter;
        }
    }
    return publishMayaMathValues(input.type(), result, data, plug, outdataPlug);
}


#pragma mark DPoint muliti arg process

MStatus processDPointMathFnc(DPData &dpdata,
						     const std::string &nodeName,
						     const std::string &sourceAttrName,
						     const std::string &outAttrName,
						     const bool &isOutAttr,
						     const bool &isDeleteOrg,
						     const MObject &targetConstPlug,
                             const MObject &targetInputDataPlug,
                             const MObject &targetAttrPlug,
						     MDataBlock& data,
						     const D_CALC_MATH_ENUM &calcType) {
    DOMMY_RETURN_IF_FAILURE(validateMathOperandBudget(data, targetConstPlug,
        targetInputDataPlug, targetAttrPlug, dpdata.numberOfPoints()));

	
	//get total count
    uint count = dpdata.numberOfPoints();
	
	
	//GetTarget
	std::vector<DMathScalarDataList> targetScalarList;
    std::vector<DMathVec2DataList> targetVec2DList;
	std::vector<DMathVec3DataList> targetVectorList;
	
	
	//get Constant
	DOMMY_RETURN_IF_FAILURE(
        DPMGetConstantValues(
            data, targetConstPlug, count, targetVectorList));
	
	
	//get target input dataArray
	DOMMY_RETURN_IF_FAILURE(
        DPMGetInputDataValues(
            data, targetInputDataPlug, count,
            targetScalarList, targetVectorList));
	
	
	//get target attribute
	DOMMY_RETURN_IF_FAILURE(
        DPMGetDPointAttributeValues(
            data, targetAttrPlug, count, dpdata,
            targetScalarList, targetVec2DList, targetVectorList));
	
	
	//get Source Data
    if (const auto sourceAttribute = dpdata.getAttr(sourceAttrName)) {
        
        DP_ATTR_DATA_TYPE type = sourceAttribute->getAttrType();
        if (isSingleRealAttrType(type)) {
			
			//Source is Scalar type
			DMathScalarDataList result(count, 0.0);
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, sourceAttrName, result));
			
			//run calc
            DOMMY_RETURN_IF_FAILURE(calcScalarValue(calcType, targetScalarList, targetVec2DList, targetVectorList, result));
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dpdata, sourceAttrName, outAttrName, result, isDeleteOrg));
			} else {
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dpdata, sourceAttrName, result));
			}
			
        } else if (isVector2DAttrType(type)) {
			
			//Source is Vector type
			DMathVec2DataList result(count, DMathVec2Data(0.0,0.0));
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, sourceAttrName, result));
			
			//run calc
            DOMMY_RETURN_IF_FAILURE(calc2DVecValue(calcType, targetScalarList, targetVec2DList, targetVectorList, result));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dpdata, sourceAttrName, outAttrName, result, isDeleteOrg));
			} else {
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dpdata, sourceAttrName, result));
			}


        } else if (isVector3DAttrType(type)) {
			
			
			//Source is Vector type
			DMathVec3DataList result(count, DMathVec3Data(0.0,0.0,0.0));
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dpdata, sourceAttrName, result));
			
			
			//run calc
            DOMMY_RETURN_IF_FAILURE(calcVectorValue(calcType, targetScalarList, targetVec2DList, targetVectorList, result));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dpdata, sourceAttrName, outAttrName, result, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dpdata, sourceAttrName, result));
				
			}
			
			
		} else {
			EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
		}
	}
	return MS::kSuccess;
}


#pragma mark DCurve muliti arg process


MStatus processDCurveMathFnc(DCurveData &dcdata,
                             const std::string &nodeName,
                             const std::string &sourceAttrName,
                             const std::string &outAttrName,
                             const bool &isOutAttr,
                             const bool &isDeleteOrg,
                             const MObject &targetConstPlug,
                             const MObject &targetInputDataPlug,
                             const MObject &targetAttrPlug,
                             MDataBlock& data,
						     const D_CALC_MATH_ENUM &calcType) {
    DOMMY_RETURN_IF_FAILURE(validateMathOperandBudget(data, targetConstPlug,
        targetInputDataPlug, targetAttrPlug,
        dcdata.isExistAttr(sourceAttrName) ? dcdata.numberOfCurves() : dcdata.getTotalCvs(),
        dcdata.numberOfCurves()));


    size_t curveCount = dcdata.numberOfCurves();
    
    if (const auto sourceAttribute = dcdata.getAttr(sourceAttrName)) {
        
        //primitive attribute


        //GetTarget
        std::vector<DMathScalarDataList> targetScalarList;
        std::vector<DMathVec2DataList> targetVec2DList;
        std::vector<DMathVec3DataList> targetVectorList;
        
        //get Constant
        DOMMY_RETURN_IF_FAILURE(
            DPMGetConstantValues(
                data, targetConstPlug, curveCount, targetVectorList));
        
        //get target input dataArray
        DOMMY_RETURN_IF_FAILURE(
            DPMGetInputDataValues(
                data, targetInputDataPlug, curveCount,
                targetScalarList, targetVectorList));


        DOMMY_RETURN_IF_FAILURE(
            DPMGetDCurvePrimAttributeValues(
                data, targetAttrPlug, dcdata,
                targetScalarList, targetVec2DList, targetVectorList));


        //Get Source
        DP_ATTR_DATA_TYPE type = sourceAttribute->getAttrType();
        if (isSingleRealAttrType(type)) {
            
            //Source is Scalar type
            
            DMathScalarDataList values(curveCount, 0.0);
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, sourceAttrName, values));
            
            //run calc
            DOMMY_RETURN_IF_FAILURE(calcScalarValue(calcType, targetScalarList, targetVec2DList, targetVectorList, values));
            
            if(isOutAttr) {
                DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                    dcdata, sourceAttrName, outAttrName, values, isDeleteOrg));
            } else {
                
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dcdata, sourceAttrName, values));
                
            }
            
            
        } else if (isVector2DAttrType(type)) {
            
            //Source is Vector type
            DMathVec2DataList values(curveCount, DMathVec2Data(0.0,0.0));
            
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, sourceAttrName, values));
            
            //run calc
            DOMMY_RETURN_IF_FAILURE(calc2DVecValue(calcType, targetScalarList, targetVec2DList, targetVectorList, values));
            
            
            if(isOutAttr) {
                DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                    dcdata, sourceAttrName, outAttrName, values, isDeleteOrg));
            } else {
                
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dcdata, sourceAttrName, values));
                
            }
            
            
        } else if (isVector3DAttrType(type)) {
            
            
            //Source is Vector type
            DMathVec3DataList values(curveCount, DMathVec3Data(0.0,0.0,0.0));
            
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dcdata, sourceAttrName, values));
            
            //run calc
            DOMMY_RETURN_IF_FAILURE(calcVectorValue(calcType, targetScalarList, targetVec2DList, targetVectorList, values));
            
            if(isOutAttr) {
                DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                    dcdata, sourceAttrName, outAttrName, values, isDeleteOrg));
            } else {
                
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dcdata, sourceAttrName, values));
                
            }


        } else {
            EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
        }


    } else if (dcdata.isExistPointAttr(sourceAttrName)) {
        
        //point attribute


        //GetTarget
        std::vector<DMathScalarDataList> targetScalarList;
        std::vector<DMathVec2DataList> targetVec2DList;
        std::vector<DMathVec3DataList> targetVectorList;
        
        uint totalCount = dcdata.getTotalCvs();
        
        
        //get Constant
        DOMMY_RETURN_IF_FAILURE(
            DPMGetConstantValues(
                data, targetConstPlug, totalCount, targetVectorList));
        
        
        //get target input dataArray
        DOMMY_RETURN_IF_FAILURE(
            DPMGetInputDataValues(
                data, targetInputDataPlug, totalCount,
                targetScalarList, targetVectorList));


        std::vector<std::vector<DMathScalarDataList> > pAttrScalarList(curveCount);
        std::vector<std::vector<DMathVec2DataList> > pAttr2DVecList(curveCount);
        std::vector<std::vector<DMathVec3DataList> > pAttrVectorList(curveCount);
        
        
        //get Dcurve point attribute
        DOMMY_RETURN_IF_FAILURE(
            DPMGetDCurvePointAttributeValues(
                data, targetAttrPlug, dcdata,
                pAttrScalarList, pAttr2DVecList, pAttrVectorList));


        //detect per curve point for target constant
        size_t totalOffset = 0;
        size_t curveOffset = 0;
        for (auto cIt=dcdata.beginCurves(),ecIt=dcdata.endCurves(); cIt!=ecIt; ++cIt) {
            
            size_t cvCount = (*cIt)->numberOfPoints();
            
            //scalar
            for (size_t n=0; n<targetScalarList.size(); n++) {
                DMathScalarDataList pSclar(targetScalarList[n].size() == 1U ? 1U : cvCount);
                for (std::size_t p = 0U; p < pSclar.size(); ++p) {
                    pSclar[p] = dommy::math::mathOperandAt(targetScalarList[n], p + totalOffset);
                }
                pAttrScalarList[curveOffset].push_back(pSclar);
            }
            
            //2d vector
            for (size_t n=0; n<targetVec2DList.size(); n++) {
                DMathVec2DataList pVector(targetVec2DList[n].size() == 1U ? 1U : cvCount);
                for (std::size_t p = 0U; p < pVector.size(); ++p) {
                    pVector[p] = dommy::math::mathOperandAt(targetVec2DList[n], p + totalOffset);
                }
                
                pAttr2DVecList[curveOffset].push_back(pVector);
            }
            
            //vector
            for (size_t n=0; n<targetVectorList.size(); n++) {
                DMathVec3DataList pVector(targetVectorList[n].size() == 1U ? 1U : cvCount);
                for (std::size_t p = 0U; p < pVector.size(); ++p) {
                    pVector[p] = dommy::math::mathOperandAt(targetVectorList[n], p + totalOffset);
                }
                
                pAttrVectorList[curveOffset].push_back(pVector);
            }
            totalOffset+=cvCount;
            
            curveOffset++;
        }


        //get Source Data
        if (dcdata.isExistPointAttr(sourceAttrName)) {


            size_t curveOffset = 0;
            for (auto cIt=dcdata.beginCurves(),ecIt=dcdata.endCurves(); cIt!=ecIt; ++cIt) {
                
                size_t cvCount = (*cIt)->numberOfPoints();
                
                
                const auto checkedAttribute = (*cIt)->getAttr(sourceAttrName);
                if (!checkedAttribute) return MS::kInvalidParameter;
                const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
                
                
                if (isSingleRealAttrType(type)) {
                    
                    
                    DMathScalarDataList values(cvCount, 0.0);
                    DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
                    
                    //run calc
                    DOMMY_RETURN_IF_FAILURE(calcScalarValue(calcType, pAttrScalarList[curveOffset], pAttr2DVecList[curveOffset], pAttrVectorList[curveOffset], values));
                    
                    if(isOutAttr) {
                        DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                            *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
                    } else {
                        
                        DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
                        
                    }
                    
                } else if (isVector2DAttrType(type)) {


                    DMathVec2DataList values(cvCount, DMathVec2Data(0.0,0.0));
                    DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
                    
                    
                    //run calc
                    DOMMY_RETURN_IF_FAILURE(calc2DVecValue(calcType, pAttrScalarList[curveOffset], pAttr2DVecList[curveOffset], pAttrVectorList[curveOffset], values));
                    
                    if(isOutAttr) {
                        DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                            *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
                    } else {
                        
                        DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
                        
                    }
                } else if (isVector3DAttrType(type)) {


                    DMathVec3DataList values(cvCount, DMathVec3Data(0.0,0.0,0.0));
                    DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
                    
                    
                    //run calc
                    DOMMY_RETURN_IF_FAILURE(calcVectorValue(calcType, pAttrScalarList[curveOffset], pAttr2DVecList[curveOffset], pAttrVectorList[curveOffset], values));
                    
                    if(isOutAttr) {
                        DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                            *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
                    } else {
                        
                        DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
                        
                    }
                } else {
                    EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
                    break;
                }//attr type check
                
                curveOffset++;
                
            } //curve loop
            
        }
        
    }
    return MS::kSuccess;
}


#pragma mark DMesh muliti arg process

MStatus processDMeshMathFnc(DMeshData &dmdata,
						    const std::string &nodeName,
						    const std::string &sourceAttrName,
						    const std::string &outAttrName,
						    const bool &isOutAttr,
						    const bool &isDeleteOrg,
						    const MObject &targetConstPlug,
                            const MObject &targetInputDataPlug,
                            const MObject &targetAttrPlug,
						    MDataBlock& data,
						    const D_CALC_MATH_ENUM &calcType)
{
    std::size_t budgetPoints = dmdata.numberOfMesh();
    if (!dmdata.isExistAttr(sourceAttrName)) dmdata.getTotalVertex(budgetPoints);
    DOMMY_RETURN_IF_FAILURE(validateMathOperandBudget(data, targetConstPlug,
        targetInputDataPlug, targetAttrPlug, budgetPoints, dmdata.numberOfMesh()));


    size_t meshCount = dmdata.numberOfMesh();
	
    if (const auto sourceAttribute = dmdata.getAttr(sourceAttrName)) {
		
		//primitive attribute


		//GetTarget
		std::vector<DMathScalarDataList> targetScalarList;
        std::vector<DMathVec2DataList> targetVec2DList;
		std::vector<DMathVec3DataList> targetVectorList;
		
		//get Constant
		DOMMY_RETURN_IF_FAILURE(
            DPMGetConstantValues(
                data, targetConstPlug, meshCount, targetVectorList));
		
		//get target input dataArray
		DOMMY_RETURN_IF_FAILURE(
            DPMGetInputDataValues(
                data, targetInputDataPlug, meshCount,
                targetScalarList, targetVectorList));


		DOMMY_RETURN_IF_FAILURE(
            DPMGetDMeshPrimAttributeValues(
                data, targetAttrPlug, dmdata,
                targetScalarList, targetVec2DList, targetVectorList));


		//Get Source
        if (isSingleRealAttrType(sourceAttribute->getAttrType())) {
			
			//Source is Scalar type
			
			DMathScalarDataList values(meshCount, 0.0);
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, sourceAttrName, values));
			
			//run calc
            DOMMY_RETURN_IF_FAILURE(calcScalarValue(calcType, targetScalarList, targetVec2DList, targetVectorList, values));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dmdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dmdata, sourceAttrName, values));
				
			}
			
			
        } else if (isVector2DAttrType(sourceAttribute->getAttrType())) {
            
            
            //Source is Vector type
            DMathVec2DataList values(meshCount, DMathVec2Data(0.0,0.0));
            
            DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, sourceAttrName, values));
            
            //run calc
            DOMMY_RETURN_IF_FAILURE(calc2DVecValue(calcType, targetScalarList, targetVec2DList, targetVectorList, values));
            
            
            if(isOutAttr) {
                DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                    dmdata, sourceAttrName, outAttrName, values, isDeleteOrg));
            } else {
                
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dmdata, sourceAttrName, values));
                
            }


        } else if (isVector3DAttrType(sourceAttribute->getAttrType())) {
			
			
			//Source is Vector type
			DMathVec3DataList values(meshCount, DMathVec3Data(0.0,0.0,0.0));
			
			DOMMY_RETURN_IF_FAILURE(readMathAttribute(dmdata, sourceAttrName, values));
			
			//run calc
            DOMMY_RETURN_IF_FAILURE(calcVectorValue(calcType, targetScalarList, targetVec2DList, targetVectorList, values));
			
			
			if(isOutAttr) {
			    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
			        dmdata, sourceAttrName, outAttrName, values, isDeleteOrg));
			} else {
				
                DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(dmdata, sourceAttrName, values));
				
			}


		} else {
			EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
		}


    } else if (dmdata.isExistPointAttr(sourceAttrName)) {
		
		//point attribute


		//GetTarget
		std::vector<DMathScalarDataList> targetScalarList;
        std::vector<DMathVec2DataList> targetVec2DList;
		std::vector<DMathVec3DataList> targetVectorList;
		
		size_t totalCount;
        dmdata.getTotalVertex(totalCount);
		
		
		//get Constant
		DOMMY_RETURN_IF_FAILURE(
            DPMGetConstantValues(
                data, targetConstPlug, totalCount, targetVectorList));
		
		
		//get target input dataArray
		DOMMY_RETURN_IF_FAILURE(
            DPMGetInputDataValues(
                data, targetInputDataPlug, totalCount,
                targetScalarList, targetVectorList));


		std::vector<std::vector<DMathScalarDataList> > pAttrScalarList(meshCount);
        std::vector<std::vector<DMathVec2DataList> > pAttr2DVecList(meshCount);
		std::vector<std::vector<DMathVec3DataList> > pAttrVectorList(meshCount);
		
		
		//get Dcurve point attribute
		DOMMY_RETURN_IF_FAILURE(
            DPMGetDMeshPointAttributeValues(
                data, targetAttrPlug, dmdata,
                pAttrScalarList, pAttr2DVecList, pAttrVectorList));


		//detect per curve point for target constant
		size_t totalOffset = 0;
		size_t meshOffset = 0;
        for (auto cIt=dmdata.beginMeshes(),ecIt=dmdata.endMeshes(); cIt!=ecIt; ++cIt) {
			
			size_t vtxCount = (*cIt)->numberOfPoints();
			
			//scalar
			for (size_t n=0,N=targetScalarList.size(); n<N; n++) {
				DMathScalarDataList pSclar(targetScalarList[n].size() == 1U ? 1U : vtxCount);
                for (std::size_t p = 0U; p < pSclar.size(); ++p) {
                    pSclar[p] = dommy::math::mathOperandAt(targetScalarList[n], p + totalOffset);
				}
				pAttrScalarList[meshOffset].push_back(pSclar);
			}
            
            //2d vector
            for (size_t n=0,N=targetVec2DList.size(); n<N; n++) {
                DMathVec2DataList pVector(targetVec2DList[n].size() == 1U ? 1U : vtxCount);
                for (std::size_t p = 0U; p < pVector.size(); ++p) {
                    pVector[p] = dommy::math::mathOperandAt(targetVec2DList[n], p + totalOffset);
                }
                
                pAttr2DVecList[meshOffset].push_back(pVector);
            }
            
			
			//vector
			for (size_t n=0,N=targetVectorList.size(); n<N; n++) {
				DMathVec3DataList pVector(targetVectorList[n].size() == 1U ? 1U : vtxCount);
                for (std::size_t p = 0U; p < pVector.size(); ++p) {
                    pVector[p] = dommy::math::mathOperandAt(targetVectorList[n], p + totalOffset);
				}
				
				pAttrVectorList[meshOffset].push_back(pVector);
			}
			totalOffset+=vtxCount;
			
			meshOffset++;
		} //mesh loop


		//get Source Data
        if (dmdata.isExistPointAttr(sourceAttrName)) {
			
			size_t meshOffset = 0;
            for (auto cIt=dmdata.beginMeshes(),ecIt=dmdata.endMeshes(); cIt!=ecIt; ++cIt) {
				
				size_t vtxCount = (*cIt)->numberOfPoints();
				
				
				const auto checkedAttribute = (*cIt)->getAttr(sourceAttrName);
				if (!checkedAttribute) return MS::kInvalidParameter;
				const DP_ATTR_DATA_TYPE type = checkedAttribute->getAttrType();
				
				
				if (isSingleRealAttrType(type)) {
					
					
					DMathScalarDataList values(vtxCount, 0.0);
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					//run calc
                    DOMMY_RETURN_IF_FAILURE(calcScalarValue(calcType, pAttrScalarList[meshOffset], pAttr2DVecList[meshOffset], pAttrVectorList[meshOffset], values));
					
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
					
                } else if (isVector2DAttrType(type)) {


                    DMathVec2DataList values(vtxCount, DMathVec2Data(0.0,0.0));
                    DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
                    
                    
                    //run calc
                    DOMMY_RETURN_IF_FAILURE(calc2DVecValue(calcType, pAttrScalarList[meshOffset], pAttr2DVecList[meshOffset], pAttrVectorList[meshOffset], values));
                    
                    if(isOutAttr) {
                        DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
                            *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
                    } else {
                        
                        DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
                        
                    }
                } else if (isVector3DAttrType(type)) {


					DMathVec3DataList values(vtxCount, DMathVec3Data(0.0,0.0,0.0));
					DOMMY_RETURN_IF_FAILURE(readMathAttribute(*(*cIt), sourceAttrName, values));
					
					
					//run calc
                    DOMMY_RETURN_IF_FAILURE(calcVectorValue(calcType,
                                    pAttrScalarList[meshOffset],
                                    pAttr2DVecList[meshOffset], pAttrVectorList[meshOffset],
                                    values));
                    
					if(isOutAttr) {
					    DOMMY_RETURN_IF_FAILURE(publishMathAttribute(
					        *(*cIt), sourceAttrName, outAttrName, values, isDeleteOrg));
					} else {
						
						DOMMY_RETURN_IF_FAILURE(replaceMathAttribute(*(*cIt), sourceAttrName, values));
						
					}
				} else {
					EchoWarning(nodeName, "Source attribute support vec3 and scalar type.");
					break;
				}//attr type check
				
				meshOffset++;
				
			} //mesh loop
			
		}
		
	}
	
	return MS::kSuccess;
}

MStatus processDDataMathOutput(
    const MObject& inputDataAttribute,
    const MObject& outputDataAttribute,
    const std::string& nodeName,
    const std::string& sourceAttrName,
    const std::string& outAttrName,
    const bool isOutAttr,
    const bool isDeleteOrg,
    const MObject& targetConstPlug,
    const MObject& targetInputDataPlug,
    const MObject& targetAttrPlug,
    MDataBlock& data,
    const MPlug& requestedPlug,
    const D_CALC_MATH_ENUM calcType)
{
    if (!isValidMathOperation(calcType)) {
        EchoWarning(nodeName, "Invalid math operation.");
        return MS::kInvalidParameter;
    }

    MStatus status;
    MDataHandle input = data.inputValue(inputDataAttribute, &status);
    if (!status) return status;
    if (input.data().isNull()) {
        EchoWarning(nodeName, "Input data is null.");
        return MS::kInvalidParameter;
    }

    const unsigned int inputTypeId = input.typeId().id();
    if (inputTypeId == ID_DP_DATA) {
        const DPData* inputData = nullptr;
        status = getValidInputDP(inputDataAttribute, data, inputData);
        if (!status) return status;
        if (!inputData) return MS::kFailure;

        MObject outputObject;
        DPData* outputData = nullptr;
        status = dommy::maya::createPluginDataObject<DPData>(
            DPData::id, outputObject, outputData);
        if (!status) return status;
        if (!outputData) return MS::kFailure;

        outputData->setDPData(*inputData);
        status = processDPointMathFnc(
            *outputData, nodeName, sourceAttrName, outAttrName,
            isOutAttr, isDeleteOrg, targetConstPlug,
            targetInputDataPlug, targetAttrPlug, data, calcType);
        if (!status) return status;
        if (!outputData->hasValidPointTopology()) return MS::kFailure;

        return publishMathDataObject(
            data, outputDataAttribute, outputObject, requestedPlug);
    }

    if (inputTypeId == ID_DC_DATA) {
        const DCurveData* inputData = nullptr;
        status = getValidInputDC(inputDataAttribute, data, inputData);
        if (!status) return status;
        if (!inputData) return MS::kFailure;

        MObject outputObject;
        DCurveData* outputData = nullptr;
        status = dommy::maya::createPluginDataObject<DCurveData>(
            DCurveData::id, outputObject, outputData);
        if (!status) return status;
        if (!outputData) return MS::kFailure;

        DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*inputData));
        if (outputData->numberOfCurves() != 0U) {
            status = processDCurveMathFnc(
                *outputData, nodeName, sourceAttrName, outAttrName,
                isOutAttr, isDeleteOrg, targetConstPlug,
                targetInputDataPlug, targetAttrPlug, data, calcType);
            if (!status) return status;
        }
        if (!outputData->hasValidCurveTopology()) return MS::kFailure;

        return publishMathDataObject(
            data, outputDataAttribute, outputObject, requestedPlug);
    }

    if (inputTypeId == ID_DM_DATA) {
        const DMeshData* inputData = nullptr;
        status = getValidInputDM(inputDataAttribute, data, inputData);
        if (!status) return status;
        if (!inputData) return MS::kFailure;

        MObject outputObject;
        DMeshData* outputData = nullptr;
        status = dommy::maya::createPluginDataObject<DMeshData>(
            DMeshData::id, outputObject, outputData);
        if (!status) return status;
        if (!outputData) return MS::kFailure;

        DOMMY_RETURN_IF_FAILURE(outputData->copyFrom(*inputData));
        if (outputData->numberOfMesh() != 0U) {
            status = processDMeshMathFnc(
                *outputData, nodeName, sourceAttrName, outAttrName,
                isOutAttr, isDeleteOrg, targetConstPlug,
                targetInputDataPlug, targetAttrPlug, data, calcType);
            if (!status) return status;
        }
        if (!outputData->hasValidMeshTopology()) return MS::kFailure;

        return publishMathDataObject(
            data, outputDataAttribute, outputObject, requestedPlug);
    }

    EchoWarning(nodeName, "Input is not supported procedural data.");
    return MS::kInvalidParameter;
}


#pragma mark InputData muliti arg process

MStatus processMayaNumericMathFunc(const MObject &,
								   const std::string &nodeName,
								    MObject &targetSrcDataPlug,
								    MObject &outdataPlug,
								    MObject &targetConstPlug,
                                    MObject &targetInputDataPlug,
								   MDataBlock& data,
								   const MPlug& plug,
								   const D_CALC_MATH_ENUM &calcType) {
    if (!isValidMathOperation(calcType)) {
        EchoWarning(nodeName, "Invalid math operation.");
        return MS::kInvalidParameter;
    }
    MStatus status;
    const MDataHandle input = data.inputValue(targetSrcDataPlug, &status);
    if (!status) return status;
    MayaMathValues result;
    DOMMY_RETURN_IF_FAILURE(readMayaMathValues(
        input, dommy::maya::kInferInputCount, result));
    DOMMY_RETURN_IF_FAILURE(evaluateMayaMathInputs(
        data, targetConstPlug, targetInputDataPlug, calcType, result));
    return publishMayaMathValues(input.type(), result, data, plug, outdataPlug);
}

MStatus processMayaNumericMathFuncForVector(
    const MObject&,
    const std::string& nodeName,
    const MObject& sourceDataPlug,
    const short targetType,
    const MObject& targetConstPlug,
    const MObject& targetInputDataPlug,
    DVec3DataList& sourceVectors,
    DVec3DataList& targetVectors,
    MDataBlock& data) {
    MStatus status;
    const MDataHandle input = data.inputValue(sourceDataPlug, &status);
    if (!status) return status;
    DVec3DataList source;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::readVectorInput(
        input, dommy::maya::kInferInputCount, source));
    if (!dommy::math::validMathBudget(source.size(), 1U)) return MS::kInvalidParameter;

    DVec3DataList target;
    if (targetType == 0) {
        DOMMY_RETURN_IF_FAILURE(readConstantVectorTarget(data, targetConstPlug, source.size(), target));
    } else if (targetType == 1) {
        EchoWarning(nodeName, "From Attribute is not available for Maya array output.");
        return MS::kInvalidParameter;
    } else if (targetType == 2) {
        const MDataHandle targetInput = data.inputValue(targetInputDataPlug, &status);
        if (!status) return status;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::readVectorInput(targetInput, source.size(), target));
    } else {
        EchoWarning(nodeName, "Unsupported vector target type.");
        return MS::kInvalidParameter;
    }
    sourceVectors.swap(source);
    targetVectors.swap(target);
    return MS::kSuccess;
}

#pragma mark multi arg calculate

MStatus calcScalarValue(const D_CALC_MATH_ENUM &calcType,
					 const std::vector<DMathScalarDataList> &targetScalarList,
                     const std::vector<DMathVec2DataList> &targetVec2DList,
                     const std::vector<DMathVec3DataList> &targetVectorList,
                     DMathScalarDataList &result)
{
    if (!isValidMathOperation(calcType)) return MS::kInvalidParameter;
    std::size_t count = 0U;
    if (!dommy::generation::checkedSum(targetScalarList.size(), targetVec2DList.size(),
            dommy::generation::kMaxWorkItemCount, count) ||
        !dommy::generation::checkedSum(count, targetVectorList.size(),
            dommy::generation::kMaxWorkItemCount, count) ||
        !dommy::math::validMathBudget(result.size(), count)) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(applyProceduralMathOperands(
        targetScalarList, result, calcType));
    DOMMY_RETURN_IF_FAILURE(applyProceduralMathOperands(
        targetVec2DList, result, calcType));
    return applyProceduralMathOperands(targetVectorList, result, calcType);
}


MStatus calc2DVecValue(const D_CALC_MATH_ENUM &calcType,
					const std::vector<DMathScalarDataList> &targetScalarList,
                    const std::vector<DMathVec2DataList> &targetVec2DList,
                    const std::vector<DMathVec3DataList> &targetVectorList,
                    DMathVec2DataList &result)
{
    if (!isValidMathOperation(calcType)) return MS::kInvalidParameter;
    std::size_t count = 0U;
    if (!dommy::generation::checkedSum(targetScalarList.size(), targetVec2DList.size(),
            dommy::generation::kMaxWorkItemCount, count) ||
        !dommy::generation::checkedSum(count, targetVectorList.size(),
            dommy::generation::kMaxWorkItemCount, count) ||
        !dommy::math::validMathBudget(result.size(), count)) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(applyProceduralMathOperands(
        targetScalarList, result, calcType));
    DOMMY_RETURN_IF_FAILURE(applyProceduralMathOperands(
        targetVec2DList, result, calcType));
    return applyProceduralMathOperands(targetVectorList, result, calcType);
}


MStatus calcVectorValue(const D_CALC_MATH_ENUM &calcType,
					 const std::vector<DMathScalarDataList> &targetScalarList,
                     const std::vector<DMathVec2DataList> &targetVec2DList,
                     const std::vector<DMathVec3DataList> &targetVectorList,
                     DMathVec3DataList &result)
{
    if (!isValidMathOperation(calcType)) return MS::kInvalidParameter;
    std::size_t count = 0U;
    if (!dommy::generation::checkedSum(targetScalarList.size(), targetVec2DList.size(),
            dommy::generation::kMaxWorkItemCount, count) ||
        !dommy::generation::checkedSum(count, targetVectorList.size(),
            dommy::generation::kMaxWorkItemCount, count) ||
        !dommy::math::validMathBudget(result.size(), count)) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(applyProceduralMathOperands(
        targetScalarList, result, calcType));
    DOMMY_RETURN_IF_FAILURE(applyProceduralMathOperands(
        targetVec2DList, result, calcType));
    return applyProceduralMathOperands(targetVectorList, result, calcType);
}


#pragma mark single arg calculate

MStatus sourceCalcScalarValue(const D_SRC_SINGLE_MATH_ENUM &calcType,
                           DMathScalarDataList &result)
{
    if (!isValidSingleMathOperation(calcType)) return MS::kInvalidParameter;

    for (auto &it:result) {
		if (!dommy::math::applyProceduralUnary(calcType, it)) return MS::kInvalidParameter;
	}
    return MS::kSuccess;
}


MStatus sourceCalcVec2DValue(const D_SRC_SINGLE_MATH_ENUM &calcType,
                          DMathVec2DataList &result)
{
    if (!isValidSingleMathOperation(calcType)) return MS::kInvalidParameter;

    for (auto &it:result) {
		if (!dommy::math::applyProceduralUnary(calcType, it)) return MS::kInvalidParameter;
	}
    return MS::kSuccess;
}

MStatus sourceCalcVec3DValue(const D_SRC_SINGLE_MATH_ENUM &calcType,
                          DMathVec3DataList &result)
{
    if (!isValidSingleMathOperation(calcType)) return MS::kInvalidParameter;

    for (auto &it:result) {
		if (!dommy::math::applyProceduralUnary(calcType, it)) return MS::kInvalidParameter;
	}
    return MS::kSuccess;
}
