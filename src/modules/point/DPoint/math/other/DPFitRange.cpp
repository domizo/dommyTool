//
//  DPFitRange.cpp
//  dommyTool
//
//  Created by yuzo on 2/23/17.
//
//

#include "DPFitRange.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "math/DMayaUnaryMath.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaPluginDataOutput.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"


#include "DNodeOperation.h"


#include "DType.h"
#include "math/DRemapMath.h"

#include <cmath>




const MTypeId DPFitRange::typeId( ID_DP_FIT_RANGE );


MObject DPFitRange::inDD;

MObject DPFitRange::inSourceDataArray;
MObject DPFitRange::aSourceAttribute;


MObject DPFitRange::aSourceMin;
MObject DPFitRange::aSourceMax;

MObject DPFitRange::aDistinationMin;
MObject DPFitRange::aDistinationMax;


MObject DPFitRange::aIsOutAttribute;
MObject DPFitRange::aIsDeleteOriginal;
MObject DPFitRange::aOutAttribute;

MObject DPFitRange::outDD;

MObject DPFitRange::outDataArray;




void *DPFitRange::creator() {
    return ::dommy::maya::createObject("DPFitRange", [&]() {
        return new DPFitRange();
    });
}

namespace {



MStatus validateFitRanges(const float3& sourceMin,
                          const float3& sourceMax,
                          const float3& destinationMin,
                          const float3& destinationMax)
{
    for (unsigned int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(sourceMin[axis]) ||
            !std::isfinite(sourceMax[axis]) ||
            !std::isfinite(destinationMin[axis]) ||
            !std::isfinite(destinationMax[axis])) {
            return MS::kInvalidParameter;
        }
    }
    return MS::kSuccess;
}

} // namespace









MStatus DPFitRange::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != outDD && plug != outDataArray) return MS::kUnknownParameter;


	MStatus stat;
    dommy::math::FitOperation operation = {};
    float3 srcMin = {0.0f, 0.0f, 0.0f};
    float3 srcMax = {0.0f, 0.0f, 0.0f};
    float3 distMin = {0.0f, 0.0f, 0.0f};
    float3 distMax = {0.0f, 0.0f, 0.0f};
    if (plug == outDD || plug == outDataArray) {
        const MDataHandle sourceMinHandle = data.inputValue(aSourceMin, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MDataHandle sourceMaxHandle = data.inputValue(aSourceMax, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MDataHandle destinationMinHandle = data.inputValue(aDistinationMin, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MDataHandle destinationMaxHandle = data.inputValue(aDistinationMax, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

        const float3& sourceMinimum = sourceMinHandle.asFloat3();
        const float3& sourceMaximum = sourceMaxHandle.asFloat3();
        const float3& destinationMinimum = destinationMinHandle.asFloat3();
        const float3& destinationMaximum = destinationMaxHandle.asFloat3();
        for (unsigned int axis = 0; axis < 3; ++axis) {
            srcMin[axis] = sourceMinimum[axis];
            srcMax[axis] = sourceMaximum[axis];
            distMin[axis] = destinationMinimum[axis];
            distMax[axis] = destinationMaximum[axis];
        }
        DOMMY_RETURN_IF_FAILURE(
            validateFitRanges(srcMin, srcMax, distMin, distMax));
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            operation.sourceMin[axis] = srcMin[axis];
            operation.sourceMax[axis] = srcMax[axis];
            operation.destinationMin[axis] = distMin[axis];
            operation.destinationMax[axis] = distMax[axis];
        }
    }
	
	if( plug == outDD) {
		
#pragma mark Get attribute
		
        const MDataHandle sourceAttributeHandle = data.inputValue(aSourceAttribute, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MDataHandle outAttributeHandle = data.inputValue(aOutAttribute, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MDataHandle deleteOriginalHandle = data.inputValue(aIsDeleteOriginal, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        const MDataHandle isOutAttributeHandle = data.inputValue(aIsOutAttribute, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);

		const std::string srcAttrName = sourceAttributeHandle.asString().asChar();
		const std::string outAttrName = outAttributeHandle.asString().asChar();
		const bool isDeleteOrg = deleteOriginalHandle.asBool();
		const bool isOutAttr = isOutAttributeHandle.asBool();
		
//		double3 finMin, finMax;
//		CheckDouble3MinMaxRange(&distMin, &distMax, &finMin, &finMax);
		
		
		
		
		if (isOutAttr && outAttrName.empty()) {
			EchoWarning(name().asChar(), "output attribute name is empty.");
			return MS::kFailure;
		}
		
		
#pragma mark inDDの取得
		
		
		
        MDataHandle inputDataHandle = data.inputValue(inDD, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
		const MObject dataObj = inputDataHandle.data();
		
		if (dataObj.isNull()) {
			EchoWarning(name().asChar(), "input data is null.");
			return MS::kFailure;
		}
		
		
		if(inputDataHandle.typeId().id() == ID_DP_DATA) {
			
#pragma mark DPoint
			const DPData* inputDPData = nullptr;
			DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDD, data, inputDPData));
			if (!inputDPData) return MS::kFailure;
			{
				
				MFnPluginData pluginData;
                DPData* outputDPData = NULL;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
                    pluginData, DPData::id, outputDPData));
				
                if (outputDPData) {
                    
                    outputDPData->setDPData(*inputDPData);
                    
                    uint count = outputDPData->numberOfPoints();
                    
                    //get Source Data
                    if (outputDPData->isExistAttr(srcAttrName)) {
                        DP_ATTR_DATA_TYPE type = outputDPData->getAttr(srcAttrName)->getAttrType();
                        if (isSingleRealAttrType(type)) {
                            
                            //Source is Scalar type
                            DMathScalarDataList result;
                            DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outputDPData, srcAttrName, count, result));
                            
                            //run calc
                            if (!operation(result)) return MS::kInvalidParameter;
                            
                            DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outputDPData, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
                            
                            
                        } else if (isVector2DAttrType(type)) {
                            
                            
                            //Source is Vector type
                            DMathVec2DataList result;
                            DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outputDPData, srcAttrName, count, result));
                            
                            
                            //run calc
                            if (!operation(result)) return MS::kInvalidParameter;
                            
                            
                            DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outputDPData, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
                            
                            
                        } else if (isVector3DAttrType(type)) {
                            
                            
                            //Source is Vector type
                            DMathVec3DataList result;
                            DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outputDPData, srcAttrName, count, result));
                            
                            
                            //run calc
                            if (!operation(result)) return MS::kInvalidParameter;
                            
                            DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outputDPData, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
                            
                            
                        } else {
                            EchoWarning(name().asChar(), "Source attribute support vec3, vec2, scalar type.");
                        }
                    }
                    
                    
                    //set output
                    MDataHandle output = data.outputValue(outDD, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    DOMMY_RETURN_IF_FAILURE(
                        dommy::maya::publishTypedPluginDataObject<DPData>(
                            output, pluginData.object(), DPData::id));
                    
                    return data.setClean(plug);
                }
                
			}
			
		} else if (inputDataHandle.typeId().id() == ID_DC_DATA) {
			
#pragma mark DCurve
			
			const DCurveData* inputDCData = nullptr;
			DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDD, data, inputDCData));
			if (!inputDCData) return MS::kFailure;
			{
				MFnPluginData pluginData;
                DCurveData* outDCrvs = NULL;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
                    pluginData, DCurveData::id, outDCrvs));
				
				
				
				if(outDCrvs) {
					DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));
					
					uint curveCount = outDCrvs->numberOfCurves();
					if (curveCount) {
						
						if (outDCrvs->isExistAttr(srcAttrName)) {
							
							//primitive attribute
							
							
							
							DP_ATTR_DATA_TYPE type = outDCrvs->getAttr(srcAttrName)->getAttrType();
							
							//Get Source
							if (isSingleRealAttrType(type)) {
								
								//Source is Scalar type
								
								DMathScalarDataList result;
								DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDCrvs, srcAttrName, curveCount, result));
								
								//run calc
								if (!operation(result)) return MS::kInvalidParameter;
								
								//add
								DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outDCrvs, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								
								
								
							} else if (isVector2DAttrType(type)) {
								
								//Source is Vector type
								DMathVec2DataList result;
								DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDCrvs, srcAttrName, curveCount, result));
								
								//run calc
								if (!operation(result)) return MS::kInvalidParameter;
								
								DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outDCrvs, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								
								
							} else if (isVector3DAttrType(type)) {
								
								//Source is Vector type
								DMathVec3DataList result;
								DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDCrvs, srcAttrName, curveCount, result));
								
								//run calc
								if (!operation(result)) return MS::kInvalidParameter;
								
								DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outDCrvs, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								
							} else {
								EchoWarning(name().asChar(), "Source attribute support vec3 and scalar type.");
							}
							
							
							
							
							
						} else if (outDCrvs->isExistPointAttr(srcAttrName)) {
							
							//point attribute
							
							
							
							//get Source Data
							
							for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves(); ++cIt) {
								
								size_t cvCount = (*cIt)->numberOfPoints();
								
								
								DP_ATTR_DATA_TYPE type = (*cIt)->getAttr(srcAttrName)->getAttrType();
								
								
								if (isSingleRealAttrType(type)) {
									
									
									DMathScalarDataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, cvCount, result));
									
									//run calc
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
								} else if (isVector2DAttrType(type)) {
									
									
									DMathVec2DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, cvCount, result));
									
									//run calc
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
									
								} else if (isVector3DAttrType(type)) {
									
									
									
									DMathVec3DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, cvCount, result));
									
									
									//run calc
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
									
								} else {
									EchoWarning(name().asChar(), "Source attribute support vec3 and scalar type.");
									break;
								}//attr type check
								
								
							} //curve loop
							
						}
					}
					
					//set output
					MDataHandle output = data.outputValue(outDD, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
					DOMMY_RETURN_IF_FAILURE(
						dommy::maya::publishTypedPluginDataObject<DCurveData>(
							output, pluginData.object(), DCurveData::id));
					output.setClean();
					return data.setClean(plug);
				}
				
			}
			
		} else if (inputDataHandle.typeId().id() == ID_DM_DATA) {
			
#pragma mark DMesh
			
			const DMeshData* inputDMData = nullptr;
			DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDD, data, inputDMData));
			if (!inputDMData) return MS::kFailure;
			{
				MFnPluginData pluginData;
                DMeshData* outDMeshes = NULL;
                DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
                    pluginData, DMeshData::id, outDMeshes));
				
				
				
				if(outDMeshes) {
					
					DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));
					
					uint  meshCount = outDMeshes->numberOfMesh();
					
					if (outDMeshes->numberOfMesh()) {
						
						if (outDMeshes->isExistAttr(srcAttrName)) {
							
							//primitive attribute
							
							
							//Get Source
							
							DP_ATTR_DATA_TYPE type = outDMeshes->getAttr(srcAttrName)->getAttrType();
							
							if (isSingleRealAttrType(type)) {
								
								//Source is Scalar type
								
								DMathScalarDataList result;
								DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDMeshes, srcAttrName, meshCount, result));
								
								//run calc
								if (!operation(result)) return MS::kInvalidParameter;
								
								
								DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outDMeshes, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								
								
							} else if (isVector2DAttrType(type)) {
								
								
								//Source is Vector type
								DMathVec2DataList result;
								DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDMeshes, srcAttrName, meshCount, result));
								
								//run calc
								if (!operation(result)) return MS::kInvalidParameter;
								
								DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outDMeshes, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								
								
							} else if (isVector3DAttrType(type)) {
								
								//Source is Vector type
								DMathVec3DataList result;
								DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*outDMeshes, srcAttrName, meshCount, result));
								
								//run calc
								if (!operation(result)) return MS::kInvalidParameter;
								
								
								DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*outDMeshes, isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
								
							} else {
								EchoWarning(name().asChar(), "Source attribute support vec3 and scalar type.");
							}
							
							
						} else if (outDMeshes->isExistPointAttr(srcAttrName)) {
							
							//point attribute
							
							
							
							//get Source Data
							
							for (auto cIt=outDMeshes->beginMeshes(); cIt!=outDMeshes->endMeshes(); ++cIt) {
								
								size_t vtxCount = (*cIt)->numberOfPoints();
								
								
								DP_ATTR_DATA_TYPE type = (*cIt)->getAttr(srcAttrName)->getAttrType();
								
								
								if (isSingleRealAttrType(type)) {
									
									
									DMathScalarDataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, vtxCount, result));
									
									//run calc
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
									
								} else if (isVector2DAttrType(type)) {
									
									
									DMathVec2DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, vtxCount, result));
									
									//run calc
									if (!operation(result)) return MS::kInvalidParameter;
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
								} else if (isVector3DAttrType(type)) {
									
									
									DMathVec3DataList result;
									DOMMY_RETURN_IF_FAILURE(DReadUnaryMathAttribute(*(*cIt), srcAttrName, vtxCount, result));
									
									//run calc
									if (!operation(result)) return MS::kInvalidParameter;
									
									
									DOMMY_RETURN_IF_FAILURE(DPublishAttributeValues(*(*cIt), isOutAttr, isDeleteOrg, outAttrName, srcAttrName, result));
									
								} else {
									EchoWarning(name().asChar(), "Source attribute support vec3 and scalar type.");
									break;
								}//attr type check
								
								
							} //mesh loop
							
							
							
						}
						
					}
					
					
					//set output
					MDataHandle output = data.outputValue(outDD, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
					DOMMY_RETURN_IF_FAILURE(
						dommy::maya::publishTypedPluginDataObject<DMeshData>(
							output, pluginData.object(), DMeshData::id));
					output.setClean();
					return data.setClean(plug);
				}
			}
			
		}
		
	}
	
    if (plug == outDataArray) {
        return dommy::maya::mapMayaUnaryArray(data, plug, inSourceDataArray,
            outDataArray, operation);
    }

    // The requested output is recognized; the input payload is unsupported.
    return MS::kInvalidParameter;
}
DOMMY_COMPUTE_CATCH("DPFitRange")


MStatus DPFitRange::initialize() try {
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
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
	DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inSourceDataArray));
	
	
	aSourceMin = nAttr.create( "SourceMin", "smin", MFnNumericData::k3Float , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceMin));
	
	aSourceMax = nAttr.create( "SourceMax", "smax", MFnNumericData::k3Float , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSourceMax));
	
	aDistinationMin = nAttr.create( "DistinationMin", "dmin", MFnNumericData::k3Float , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDistinationMin));
	
	aDistinationMax = nAttr.create( "DistinationMax", "dmax", MFnNumericData::k3Float , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setReadable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aDistinationMax));
	
	
	
	aIsOutAttribute = nAttr.create( "IsOutAttribute", "isout", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsOutAttribute));
	
	const MObject aOutAttributeDefaultData = fnStringData.create("result", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOutAttribute = tAttr.create("OutAttribute", "outattr", MFnData::kString, aOutAttributeDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOutAttribute));
	
	
	aIsDeleteOriginal = nAttr.create( "IsDeleteOriginal", "idor", MFnNumericData::kBoolean , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aIsDeleteOriginal));
	
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
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
	DOMMY_RETURN_IF_FAILURE(gAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDataArray));
	
	
	
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDD, outDD ));
	
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceAttribute, outDD ));
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsOutAttribute, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsDeleteOriginal, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutAttribute, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceMin, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceMax, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistinationMin, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistinationMax, outDD ));
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inSourceDataArray, outDataArray ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceMin, outDataArray ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSourceMax, outDataArray ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistinationMin, outDataArray ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aDistinationMax, outDataArray ));
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPFitRange", "initialize", std::current_exception());
}
