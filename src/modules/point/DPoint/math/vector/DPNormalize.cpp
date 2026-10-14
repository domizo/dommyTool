//
//  DPNormalize.cpp
//  dommyTool
//
//  Created by yuzo on 2/20/17.
//
//

#include "DPNormalize.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "math/DMayaUnaryMath.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


#include "DNodeOperation.h"


#include "DPMathUtility.h"


#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>



const MTypeId DPNormalize::typeId( ID_DP_MATH_VEC_NORMALIZE );


MObject DPNormalize::inDD;
MObject DPNormalize::aSourceAttribute;
MObject DPNormalize::inSourceDataArray;


MObject DPNormalize::aIsOutAttribute;
MObject DPNormalize::aOutAttribute;
MObject DPNormalize::aIsDeleteOriginal;
MObject DPNormalize::outDD;

MObject DPNormalize::outDataArray;


void *DPNormalize::creator() {
    return ::dommy::maya::createObject("DPNormalize", [&]() {
        return new DPNormalize();
    });
}



MStatus DPNormalize::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != outDD && plug != outDataArray) return MS::kUnknownParameter;


	MStatus stat;
    const dommy::math::NormalizeOperation operation;

	
	
	
	if( plug == outDD) {
		
#pragma mark Get attribute
		
		MDataHandle sourceHandle = data.inputValue(aSourceAttribute, &stat);
        if (!stat) return stat;
        MDataHandle outputNameHandle = data.inputValue(aOutAttribute, &stat);
        if (!stat) return stat;
        MDataHandle deleteHandle = data.inputValue(aIsDeleteOriginal, &stat);
        if (!stat) return stat;
        MDataHandle createHandle = data.inputValue(aIsOutAttribute, &stat);
        if (!stat) return stat;
        const std::string srcAttrName = sourceHandle.asString().asChar();
		
		
		const std::string outAttrName = outputNameHandle.asString().asChar();
		const bool isDeleteOrg = deleteHandle.asBool();
		const bool isOutAttr = createHandle.asBool();
		
		if (isOutAttr && outAttrName.empty()) {
			EchoWarning(name().asChar(), "output attribute name is empty.");
			return MS::kFailure;
		}
		
		
#pragma mark inDDの取得
		
		
		
		MDataHandle inputDataHandle = data.inputValue(inDD, &stat);
        if (!stat) return stat;
        MObject dataObj = inputDataHandle.data();
		
		if (dataObj.isNull()) {
			EchoWarning(name().asChar(), "input data is null.");
			return MS::kFailure;
		}
		
		
		if(inputDataHandle.typeId().id() == ID_DP_DATA) {
			
#pragma mark DPoint
			const DPData* inputDPData = nullptr;
			DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
				inputDataHandle, DPData::id, inputDPData));
			if (inputDPData && !inputDPData->hasValidPointTopology()) {
				return MS::kInvalidParameter;
			}
			
			if (inputDPData) {
				
				MObject outputDataObject;
				DPData* outputDPData = nullptr;
				stat = dommy::maya::createPluginDataObject<DPData>(
					DPData::id, outputDataObject, outputDPData);
				if (!stat) return stat;
				if (!outputDPData) return MS::kFailure;
				
				outputDPData->setDPData(*inputDPData);
				
				
				uint count = outputDPData->numberOfPoints();
				
				
				
				if (outputDPData->isExistAttr(srcAttrName)) {
					
					DP_ATTR_DATA_TYPE type = outputDPData->getAttr(srcAttrName)->getAttrType();
					
					if (isVector3DAttrType(type)) {
						
						DMathVec3DataList result;
						DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outputDPData, srcAttrName, count, result));
						if (!operation(result)) return MS::kInvalidParameter;
						
						DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
							*outputDPData, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
						
					} else if (isVector2DAttrType(type)) {
						DMathVec2DataList result;
						DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outputDPData, srcAttrName, count, result));
						if (!operation(result)) return MS::kInvalidParameter;
						
						DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
							*outputDPData, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
						
					} else {
						EchoWarning(name().asChar(), "Not support Source attribute type.");
						
					}
				}
				
				
				//set output
				MDataHandle output = data.outputValue(outDD, &stat);
				if (!stat) return stat;
				DOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(
					output, outputDataObject, *outputDPData));
				
				return data.setClean(plug);
			}
			
		} else if (inputDataHandle.typeId().id() == ID_DC_DATA) {
			
#pragma mark DCurve
			
			const DCurveData* inputDCData = nullptr;
			DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
				inputDataHandle, DCurveData::id, inputDCData));
			if (inputDCData && !inputDCData->hasValidCurveTopology()) {
				return MS::kInvalidParameter;
			}
			
			if (inputDCData) {
				MObject outputDataObject;
				DCurveData* outDCrvs = nullptr;
				stat = dommy::maya::createPluginDataObject<DCurveData>(
					DCurveData::id, outputDataObject, outDCrvs);
				if (!stat) return stat;
				
				
				if(outDCrvs) {
					DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));
					
					uint curveCount = outDCrvs->numberOfCurves();
					if (curveCount) {
						
						if (outDCrvs->isExistAttr(srcAttrName)) {
							
							//primitive attribute
							
							
							if (outDCrvs->isExistAttr(srcAttrName)) {
								DP_ATTR_DATA_TYPE type = outDCrvs->getAttr(srcAttrName)->getAttrType();
								
								
								if (isVector3DAttrType(type)) {
									DMathVec3DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDCrvs, srcAttrName, curveCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*outDCrvs, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
								} else if (isVector2DAttrType(type)) {
									DMathVec2DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDCrvs, srcAttrName, curveCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*outDCrvs, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								} else {
									EchoWarning(name().asChar(), "Not support Source attribute type.");
								}
							}
							
							
						} else if (outDCrvs->isExistPointAttr(srcAttrName)) {
							
							//point attribute
							
							for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
								
								size_t cvCount = (*cIt)->numberOfPoints();
								
								
								DP_ATTR_DATA_TYPE type = (*cIt)->getAttr(srcAttrName)->getAttrType();
								
								if (isVector2DAttrType(type)) {
									DMathVec2DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, cvCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								} else if (isVector3DAttrType(type)) {
									DMathVec3DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, cvCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								}
								
								
							} //curve loop
							
							
							
						}
						
					}
					
					//set output
					MDataHandle output = data.outputValue(outDD, &stat);
					if (!stat) return stat;
					DOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(
						output, outputDataObject, *outDCrvs));
					return data.setClean(plug);
				}
				
			}
			
		} else if (inputDataHandle.typeId().id() == ID_DM_DATA) {
			
#pragma mark DMesh
			
			const DMeshData* inputDMData = nullptr;
			DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
				inputDataHandle, DMeshData::id, inputDMData));
			if (inputDMData && !inputDMData->hasValidMeshTopology()) {
				return MS::kInvalidParameter;
			}
			
			if (inputDMData) {
				MObject outputDataObject;
				DMeshData* outDMeshes = nullptr;
				stat = dommy::maya::createPluginDataObject<DMeshData>(
					DMeshData::id, outputDataObject, outDMeshes);
				if (!stat) return stat;
				
				if(outDMeshes) {
					
					DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));
					
					
					uint meshCount = outDMeshes->numberOfMesh();
					
					
					
					if (meshCount) {
						
						if (outDMeshes->isExistAttr(srcAttrName)) {
							
							//primitive attribute
							
							if (outDMeshes->isExistAttr(srcAttrName)) {
								DP_ATTR_DATA_TYPE type = outDMeshes->getAttr(srcAttrName)->getAttrType();
								
								if (isVector3DAttrType(type)) {
									DMathVec3DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDMeshes, srcAttrName, meshCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*outDMeshes, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
								} else if (isVector2DAttrType(type)) {
									DMathVec2DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDMeshes, srcAttrName, meshCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*outDMeshes, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
									
								} else {
									EchoWarning(name().asChar(), "Not support Source attribute type.");
									
								}
								
							}
							
						} else if (outDMeshes->isExistPointAttr(srcAttrName)) {
							
							//point attribute
							
							
							for (auto cIt=outDMeshes->beginMeshes(); cIt!=outDMeshes->endMeshes(); ++cIt) {
								
								size_t cvCount = (*cIt)->numberOfPoints();
								
								
								DP_ATTR_DATA_TYPE type = (*cIt)->getAttr(srcAttrName)->getAttrType();
								
								
								
								if (isVector2DAttrType(type)) {
									DMathVec2DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, cvCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
								} else if (isVector3DAttrType(type)) {
									DMathVec3DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, cvCount, result));
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(
										*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
								}
								
								
							} //mesh loop
							
							
							
						}//attribute check
						
					}//mesh count check
					
					
					//set output
					MDataHandle output = data.outputValue(outDD, &stat);
					if (!stat) return stat;
					DOMMY_RETURN_IF_FAILURE(dommy::maya::publishPluginDataObject(
						output, outputDataObject, *outDMeshes));
					return data.setClean(plug);
				}
			}
			
		}
        return MS::kInvalidParameter;
	}
	
    if (plug == outDataArray) {
        return dommy::maya::mapMayaUnaryArray(data, plug, inSourceDataArray,
            outDataArray, operation, true, true);
    }

    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPNormalize")


MStatus DPNormalize::initialize() try {
	MStatus stat;
	MFnNumericAttribute nAttr;
	MFnGenericAttribute gAttr;
	MFnTypedAttribute tAttr;
	MFnStringData fnStringData;
	
	// ----------------------- INPUTS --------------------------
	inDD = gAttr.create( "inDD", "idd",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDD));
	
	
	
	
	MObject emptyAttrName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aSourceAttribute = tAttr.create("SourceAttribute", "srcattr", MFnData::kString, emptyAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceAttribute));
	
	
	
	
	
	inSourceDataArray = gAttr.create( "inSourceDataArray", "insrcdt",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inSourceDataArray));
	
	
	
	
	aIsOutAttribute = nAttr.create( "IsOutAttribute", "isout", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOutAttribute));
	
	
	aIsDeleteOriginal = nAttr.create( "IsDeleteOriginal", "idor", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDeleteOriginal));
	
	
	const MObject aOutAttributeDefaultData = fnStringData.create("normalize", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOutAttribute = tAttr.create("OutAttribute", "outattr", MFnData::kString, aOutAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOutAttribute));
	
	
	
	//create custom data
	outDD = gAttr.create( "outDD", "odd", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DPData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DCurveData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.addAccept(DMeshData::id));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDD));
	
	
	outDataArray = gAttr.create( "outDataArray", "out",  &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));
	
	
	
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceAttribute, outDD ));

	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsOutAttribute, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsDeleteOriginal, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutAttribute, outDD ));
	
	
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inSourceDataArray, outDataArray ));
	
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPNormalize", "initialize", std::current_exception());
}
