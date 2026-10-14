//
//  DPRemap.cpp
//  dommyTool
//
//  Created by yuzo on 2/23/17.
//
//

#include "DPRemap.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DMayaDefaultRamp.h"
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


#include "math/DRemapMath.h"


#include <algorithm>
#include <cmath>


const MTypeId DPRemap::typeId( ID_DP_REMAP );


MObject DPRemap::inDD;

MObject DPRemap::inSourceDataArray;
MObject DPRemap::aSourceAttribute;


MObject DPRemap::aRamp1D;
MObject DPRemap::aRamp2D;
MObject DPRemap::aRamp3D;

MObject DPRemap::aRange1D;
MObject DPRemap::aRange2D;
MObject DPRemap::aRange3D;

MObject DPRemap::aKeepRange;

MObject DPRemap::aIsOutAttribute;
MObject DPRemap::aIsDeleteOriginal;
MObject DPRemap::aOutAttribute;

MObject DPRemap::outDD;

MObject DPRemap::outDataArray;

namespace {

struct RemapSettings {
    float minimum[3];
    float maximum[3];
    bool keepRange;
};

MStatus readRemapSettings(MDataBlock& data, RemapSettings& settings)
{
    MStatus status;
    const MDataHandle range1DHandle = data.inputValue(DPRemap::aRange1D, &status);
    if (!status) return status;
    const float2& range1D = range1DHandle.asFloat2();

    const MDataHandle range2DHandle = data.inputValue(DPRemap::aRange2D, &status);
    if (!status) return status;
    const float2& range2D = range2DHandle.asFloat2();

    const MDataHandle range3DHandle = data.inputValue(DPRemap::aRange3D, &status);
    if (!status) return status;
    const float2& range3D = range3DHandle.asFloat2();

    const MDataHandle keepRangeHandle = data.inputValue(DPRemap::aKeepRange, &status);
    if (!status) return status;

    settings.minimum[0] = range1D[0];
    settings.maximum[0] = range1D[1];
    settings.minimum[1] = range2D[0];
    settings.maximum[1] = range2D[1];
    settings.minimum[2] = range3D[0];
    settings.maximum[2] = range3D[1];
    settings.keepRange = keepRangeHandle.asBool();

    for (unsigned int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(settings.minimum[axis]) ||
            !std::isfinite(settings.maximum[axis])) {
            return MS::kInvalidParameter;
        }
        if (settings.maximum[axis] < settings.minimum[axis]) {
            std::swap(settings.minimum[axis], settings.maximum[axis]);
        }
    }
    return MS::kSuccess;
}









struct RemapOperation {
    RemapSettings settings;
    MRampAttribute* ramps[3];
    template <typename Values> bool operator()(Values& values) const {
        return dommy::math::remapValues(values, settings,
            [&](unsigned int axis, float position, float& sampled) {
                MStatus status;
                ramps[axis]->getValueAtPosition(position, sampled, &status);
                return status == MS::kSuccess;
            });
    }
};

} // namespace




void *DPRemap::creator() {
    return ::dommy::maya::createObject("DPRemap", [&]() {
        return new DPRemap();
    });
}


void DPRemap::postConstructor() {
    dommy::maya::initializeNodeRamps("DPRemap", [&]() -> MStatus {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aRamp1D,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aRamp2D,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        DOMMY_RETURN_IF_FAILURE(dommy::maya::initializeDefaultCurve(thisMObject(), aRamp3D,
                                                                    {0.0f, 1.0f}, {0.0f, 1.0f}));
        return MS::kSuccess;
    });
}


MStatus DPRemap::compute( const MPlug& plug, MDataBlock& data ) try
{
    if (plug != outDD && plug != outDataArray) return MS::kUnknownParameter;


	MStatus stat;
    RemapSettings settings = {{0.0f, 0.0f, 0.0f},
                              {1.0f, 1.0f, 1.0f},
                              false};
    if (plug == outDD || plug == outDataArray) {
        DOMMY_RETURN_IF_FAILURE(readRemapSettings(data, settings));
    }
    MPlug ramp1Plug(thisMObject(), aRamp1D);
    MRampAttribute ramp1(ramp1Plug, &stat);
    if (!stat) return stat;
    MPlug ramp2Plug(thisMObject(), aRamp2D);
    MRampAttribute ramp2(ramp2Plug, &stat);
    if (!stat) return stat;
    MPlug ramp3Plug(thisMObject(), aRamp3D);
    MRampAttribute ramp3(ramp3Plug, &stat);
    if (!stat) return stat;
    const RemapOperation operation = {settings, {&ramp1, &ramp2, &ramp3}};
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
DOMMY_COMPUTE_CATCH("DPRemap")


MStatus DPRemap::initialize() try {
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
	
	
	
	aRamp1D = MRampAttribute::createCurveRamp("Ramp1D", "r1d", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRamp1D));
	
	aRamp2D = MRampAttribute::createCurveRamp("Ramp2D", "r2d", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRamp2D));
	
	aRamp3D = MRampAttribute::createCurveRamp("Ramp3D", "r3d", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRamp3D));
    
    
    aRange1D = nAttr.create( "Range1D", "rng1d", MFnNumericData::k2Float , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRange1D));
    
    aRange2D = nAttr.create( "Range2D", "rng2d", MFnNumericData::k2Float , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRange2D));
    
    aRange3D = nAttr.create( "Range3D", "rng3d", MFnNumericData::k2Float , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRange3D));
    
    aKeepRange = nAttr.create( "KeepRange", "kprng", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aKeepRange));
	
	
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
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRamp1D, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRamp2D, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRamp3D, outDD ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRange1D, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRange2D, outDD ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRange3D, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aKeepRange, outDD ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsOutAttribute, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsDeleteOriginal, outDD ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutAttribute, outDD ));
	
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inSourceDataArray, outDataArray ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRamp1D, outDataArray ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRamp2D, outDataArray ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRamp3D, outDataArray ));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRange1D, outDataArray ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRange2D, outDataArray ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRange3D, outDataArray ));
	
    
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPRemap", "initialize", std::current_exception());
}
