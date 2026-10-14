//
//  DPAttributeCreate.cpp
//  dommyTool
//
//  Created by yuzo on 5/23/16.
//
//

#include "DPAttributeCreate.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginData.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DAttributeCreateNumeric.h"
#include "common/utility/DMayaNumericInput.h"
#include "common/utility/DMayaAttributeCreatePlan.h"

#include "DMayaHeaders.h"



#include "DPData.h"
#include "DPointCommon.h"
#include "DCurveUtil.h"
#include "DMeshUtil.h"
#include "dpoint_ids.h"





const MTypeId DPAttributeCreate::typeId( ID_DP_ATTR_CREATE );


MObject DPAttributeCreate::inDP;
MObject DPAttributeCreate::inDC;
MObject DPAttributeCreate::inDM;


MObject DPAttributeCreate::attributeDataList;
MObject DPAttributeCreate::attributeType;
MObject DPAttributeCreate::aAttributeClass;
MObject DPAttributeCreate::attributeName;

//MObject DPAttributeCreate::baseValueX;
//MObject DPAttributeCreate::baseValueY;
//MObject DPAttributeCreate::baseValueZ;
MObject DPAttributeCreate::defaultValue;
MObject DPAttributeCreate::aSetValue;
MObject DPAttributeCreate::aPrecision;
MObject DPAttributeCreate::existActionType;
MObject DPAttributeCreate::valueSize;
MObject DPAttributeCreate::inputData;


MObject DPAttributeCreate::outDP;
MObject DPAttributeCreate::outDC;
MObject DPAttributeCreate::outDM;


void *DPAttributeCreate::creator() {
    return ::dommy::maya::createObject("DPAttributeCreate", [&]() {
        return new DPAttributeCreate();
    });
}


MStatus DPAttributeCreate::compute( const MPlug& plug, MDataBlock& data ) try
{
    MStatus stat;

	
	
	
    if( plug == outDP ) {
#pragma mark ---------DP Plug---------
		
		
        
        const DPData* inputDPData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));
        if (!inputDPData) return MS::kFailure;
        dommy::point::PointPayloadBudget retained;
        dommy::point::preflightPointPayload(*inputDPData, sizeof(DPoint), retained);
        dommy::attribute::AttributeCreatePlan creationPlan(retained);
        creationPlan.addTarget(*inputDPData, inputDPData->numberOfPoints());
        const dommy::maya::AttributeCreateControls controls = {
            attributeDataList, attributeType, aAttributeClass, attributeName,
            valueSize, aPrecision, existActionType, inputData};
        DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightAttributeCreateRows(
            data, thisMObject(), controls, false, creationPlan));

        MFnPluginData pluginData;
        DPData* outputDPData = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
            pluginData, DPData::id, outputDPData));
        if (!outputDPData) return MS::kFailure;
        outputDPData->setDPData(*inputDPData);
        
        
        
        // Read from attributeDataList
        MArrayDataHandle arrayHandle = data.inputArrayValue(attributeDataList, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        const unsigned int count = arrayHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
		
		
        
        for(uint i = 0; i < count; i++) {
            stat = arrayHandle.jumpToArrayElement(i);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
			MDataHandle attrHnd = arrayHandle.inputValue(&stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
            const DA_CREATE_TYPE attrType = attrHnd.child(attributeType).asShort();
            std::string attrName = attrHnd.child(attributeName).asString().asChar();
			int valueSizeVal = attrHnd.child(valueSize).asInt();
            float3 &defaultVal = attrHnd.child(defaultValue).asFloat3();
			float3 &applyVal = attrHnd.child(aSetValue).asFloat3();
			
			const short precision = attrHnd.child(aPrecision).asShort();
            short exActType = attrHnd.child(existActionType).asShort();
			
			
			
			
			
			
			
			
			
			
				if (attrName.size() == 0) {
					MGlobal::displayWarning("Attribute name is empty.");
					continue;
				}
			
            const dommy::attribute::AttributeCreationPolicy creation(exActType != 1);
            if (!creation.shouldCreate(*outputDPData, attrName)) continue;
			
#pragma mark connect input data
			//データがコネクトされている場合、優先的に利用する
			MFnData::Type inDataType;
			MPlug inputDataPlug(thisMObject(), inputData);
			const bool inputConnected = inputDataPlug.isConnected(&stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			if (inputConnected) {
					MDataHandle dataArrayHnd = attrHnd.child(inputData);
					inDataType = dataArrayHnd.type();

					size_t ptCount = outputDPData->numberOfPoints();
				
                if (inDataType == MFnData::kNumeric) {
						long double val = 0.0L;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::readNumericScalar(dataArrayHnd, val));
                    
					
                    if (attrType == DA_CREATE_INT) {
						
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<int32_t>(*outputDPData, attrName, val, defaultVal[0], ptCount));
                        continue;
                    } else if (attrType == DA_CREATE_FLOAT) {
						
						if (precision==0) {//float
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<float>(*outputDPData, attrName, val, defaultVal[0], ptCount));
						} else {//double
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<double>(*outputDPData, attrName, val, defaultVal[0], ptCount));
						}
						
                        continue;
					} else if (attrType == DA_CREATE_BOOL) {
						
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<bool>(*outputDPData, attrName, val, defaultVal[0], ptCount));
                        continue;
                    }
                    
                } else if (inDataType == MFnData::kString) {
                    
					
                    std::string val = dataArrayHnd.asString().asChar();
					std::string defStr = "";
                    DOMMY_RETURN_IF_FAILURE(
                        creation.create<DPStringAttr>(*outputDPData, attrName, val, defStr, ptCount));
                    continue;
                    
                } else if (inDataType == MFnData::kMatrix) {
                    
					
                    MMatrix val = dataArrayHnd.asMatrix();
					MMatrix defMat = MMatrix();
                    DOMMY_RETURN_IF_FAILURE(
                        creation.create<DPMat4Attr>(*outputDPData, attrName, val, defMat, ptCount));
                    continue;
                    
                } else if (inDataType == MFnData::kVectorArray) {
                    
					
					MFnVectorArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MVectorArray pts = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
//                    CHECK_MSTATUS(getVectorArrayFromData(thisMObject(), inputData, &pts));
					
                    if (pts.length() && pts.length()==ptCount) {
						
						if(precision == 0){// float
							std::vector<Imath::V3f> ptArray(ptCount,Imath::V3f(0.0,0.0,0.0) );
							
							if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), ptCount, ptArray))
                                return MS::kInvalidParameter;
                            
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3f>(*outputDPData, attrName, ptArray,
																		Imath::V3f(defaultVal[0],
																				   defaultVal[1],
																				   defaultVal[2])));
							
						} else {//double
							std::vector<Imath::V3d> ptArray(ptCount,Imath::V3d(0.0,0.0,0.0) );
							
							if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), ptCount, ptArray))
                                return MS::kInvalidParameter;
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3d>(*outputDPData, attrName, ptArray,
																		Imath::V3d(defaultVal[0],
																				   defaultVal[1],
																				   defaultVal[2])));
						}
						
                        continue;
                    }
                    
                } else if (inDataType == MFnData::kPointArray) {
                    
					
					MFnPointArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MPointArray pts = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
//                    CHECK_MSTATUS(getPointArrayFromData(thisMObject(), inputData, &pts));
					
                    if (pts.length() && pts.length()==ptCount) {
						
						if(precision == 0){// float
							std::vector<Imath::V3f> ptArray(ptCount,Imath::V3f(0.0,0.0,0.0) );
							
							if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), ptCount, ptArray))
                                return MS::kInvalidParameter;
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3f>(*outputDPData, attrName, ptArray,
																		Imath::V3f(defaultVal[0],
																				   defaultVal[1],
																				   defaultVal[2])));
							
						} else {//double
							std::vector<Imath::V3d> ptArray(ptCount,Imath::V3d(0.0,0.0,0.0) );
							
							if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), ptCount, ptArray))
                                return MS::kInvalidParameter;
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3d>(*outputDPData, attrName, ptArray, Imath::V3d(defaultVal[0],
																							defaultVal[1],
																							defaultVal[2])));
						}
						
						
						
						
                        continue;
                    }
					
                    
                } else if (inDataType == MFnData::kFloatArray) {
                    
                    
                    MFnFloatArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MFloatArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    if (dataArray.length() && dataArray.length()==ptCount) {
                        
	                        std::vector<float> newArray;
	                        DScalarMayaArrayToVectorList(dataArray, newArray);
	                        
	                        DOMMY_RETURN_IF_FAILURE(
	                            creation.numeric<float>(*outputDPData, attrName, newArray, defaultVal[0]));
                        continue;
                    }
                    
                } else if (inDataType == MFnData::kDoubleArray) {
                    
                    MFnDoubleArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MDoubleArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
					
                    if (dataArray.length() && dataArray.length()==ptCount) {
                        
	                        std::vector<double> newArray;
	                        DScalarMayaArrayToVectorList(dataArray, newArray);
	                        
	                        DOMMY_RETURN_IF_FAILURE(
	                            creation.numeric<double>(*outputDPData, attrName, newArray, defaultVal[0]));
                        continue;
                    }
                    
                } else if (inDataType == MFnData::kIntArray) {
                    
                    MFnIntArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MIntArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    if (dataArray.length() && dataArray.length()==ptCount) {
                        
	                        std::vector<int> newArray;
	                        DScalarMayaArrayToVectorList(dataArray, newArray);
	                        
	                        DOMMY_RETURN_IF_FAILURE(
	                            creation.numeric<int32_t>(*outputDPData, attrName, newArray, defaultVal[0]));
                        continue;
                    }
                    
                } else if (inDataType == MFnData::kStringArray) {
                    
                    
                    MFnStringArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MStringArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    if (dataArray.length() && dataArray.length()==ptCount) {
                        DStringList newArray(ptCount,"");
						
                        for (uint i=0; i<ptCount; i++) {
                            newArray[i] = dataArray[i].asChar();
                        }
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPStringAttr>(*outputDPData, attrName, newArray, ""));
                        continue;
                    }
                    
                } else if (inDataType == MFnData::kMatrixArray) {
                    
                    MFnMatrixArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MMatrixArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    if (dataArray.length() && dataArray.length()==ptCount) {
                        std::vector<MMatrix> newArray(ptCount, MMatrix());
						
                        for (uint i=0; i<ptCount; i++) {
                            newArray[i] = dataArray[i];
                        }
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPMat4Attr>(*outputDPData, attrName, newArray, MMatrix()));
                        continue;
                    }
                    
                }
			}
            
            
            
            
            
            
#pragma mark data create from attribute
			
			if (attrType==DA_CREATE_FLOAT) {
				
				
				if(precision==0) {//float
					if (valueSizeVal==1) {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<float>(*outputDPData, attrName, applyVal[0], defaultVal[0], outputDPData->numberOfPoints()));
						
					} else if(valueSizeVal==2) {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<Imath::V2f>(*outputDPData, attrName, Imath::V2f(applyVal[0],applyVal[1]),
																	Imath::V2f(defaultVal[0],defaultVal[1]),
																	outputDPData->numberOfPoints()));
						
					} else {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<Imath::V3f>(*outputDPData, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
																	Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
																	outputDPData->numberOfPoints()));
						
					}
				} else {//double
					if (valueSizeVal==1) {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<double>(*outputDPData, attrName, applyVal[0],
																	 defaultVal[0],
																	 outputDPData->numberOfPoints()));
						
					} else if(valueSizeVal==2) {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<Imath::V2d>(*outputDPData, attrName, Imath::V2d(applyVal[0],
																			   applyVal[1]),
																	Imath::V2d(defaultVal[0],
																			   defaultVal[1]),
																	outputDPData->numberOfPoints()));
						
					} else {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<Imath::V3d>(*outputDPData, attrName, Imath::V3d(applyVal[0],
																			   applyVal[1],
																			   applyVal[2]),
																	Imath::V3d(defaultVal[0],
																			   defaultVal[1],
																			   defaultVal[2]),
																	outputDPData->numberOfPoints()));
					}
				}
				
				
				
			} else if (attrType==DA_CREATE_INT) {
				
                if (valueSizeVal==1) {
                    DOMMY_RETURN_IF_FAILURE(
                        creation.numeric<int32_t>(*outputDPData, attrName, applyVal[0], defaultVal[0], outputDPData->numberOfPoints()));
                    
                } else if(valueSizeVal==2) {
                    DOMMY_RETURN_IF_FAILURE(
                        creation.numeric<Imath::V2i>(*outputDPData, attrName, Imath::V2f(applyVal[0],applyVal[1]),
																Imath::V2f(defaultVal[0],defaultVal[1]),
																outputDPData->numberOfPoints()));
                } else {
                    DOMMY_RETURN_IF_FAILURE(
                        creation.numeric<Imath::V3i>(*outputDPData, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
																Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
																outputDPData->numberOfPoints()));
                }
                
			} else if (attrType==DA_CREATE_VECTOR) {
				
				if(precision==0) {//float
					DOMMY_RETURN_IF_FAILURE(
					    creation.numeric<Imath::V3f>(*outputDPData, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
																Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
																outputDPData->numberOfPoints()));
				} else {//double
					
					DOMMY_RETURN_IF_FAILURE(
					    creation.numeric<Imath::V3d>(*outputDPData, attrName, Imath::V3d(applyVal[0],applyVal[1],applyVal[2]),
																Imath::V3d(defaultVal[0],defaultVal[1],defaultVal[2]),
																outputDPData->numberOfPoints()));
				}
				
				
				
                
			} else if (attrType==DA_CREATE_STRING) {
				DOMMY_RETURN_IF_FAILURE(
				    creation.create<DPStringAttr>(*outputDPData, attrName, "", "", outputDPData->numberOfPoints()));
                
			} else if (attrType==DA_CREATE_MATRIX) {
				
                MMatrix defMat;
                DOMMY_RETURN_IF_FAILURE(
                    creation.create<DPMat4Attr>(*outputDPData, attrName, defMat, defMat, outputDPData->numberOfPoints()));
                
			} else if (attrType==DA_CREATE_BOOL) {
				
                DOMMY_RETURN_IF_FAILURE(
                    creation.numeric<bool>(*outputDPData, attrName, applyVal[0],
														   defaultVal[0],
														   outputDPData->numberOfPoints()));
                
			}
			
        }
        
        
        
        
        
#pragma mark output DP
        
        MDataHandle output = data.outputValue(outDP, &stat);
        if (!stat) return stat;
        stat = dommy::maya::publishPluginDataObject(
            output, pluginData.object(), *outputDPData);
        if (!stat) return stat;
        
        return data.setClean(plug);
		
		
		
    }
	
	if( plug == outDC ) {
#pragma mark ---------DC Plug---------
		
		
		
		
        const DCurveData* inputDCData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));
        if (!inputDCData) return MS::kFailure;
		
        const std::size_t inputCurveCount = inputDCData->numberOfCurves();
        dommy::point::PointPayloadBudget retained;
        dommy::point::preflightPrimitivePointPayload(
            *inputDCData, inputCurveCount, sizeof(DPoint), "", 0U, retained);
        retained.add(inputCurveCount, sizeof(MMatrix));
        for (auto item = inputDCData->beginCurves(); item != inputDCData->endCurves(); ++item)
            retained.add((*item)->numberOfKnots(), sizeof(double));
        dommy::attribute::AttributeCreatePlan creationPlan(retained);
        creationPlan.addTarget(*inputDCData, inputCurveCount);
        for (auto item = inputDCData->beginCurves(); item != inputDCData->endCurves(); ++item)
            creationPlan.addTarget(**item, (*item)->numberOfPoints());
        const dommy::maya::AttributeCreateControls controls = {
            attributeDataList, attributeType, aAttributeClass, attributeName,
            valueSize, aPrecision, existActionType, inputData};
        if (inputCurveCount != 0U)
            DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightAttributeCreateRows(
                data, thisMObject(), controls, true, creationPlan));

		MFnPluginData pluginData;
		DCurveData* outDCrvs = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
            pluginData, DCurveData::id, outDCrvs));
        if (!outDCrvs) return MS::kFailure;
		DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));
		
        size_t curveCount = outDCrvs->numberOfCurves();
        
        if(curveCount==0) {
            MDataHandle output = data.outputValue(outDC, &stat);
            if (!stat) return stat;
            stat = dommy::maya::publishPluginDataObject(
                output, pluginData.object(), *outDCrvs);
            if (!stat) return stat;
            return data.setClean(plug);
        }
		
		// Read from attributeDataList
		MArrayDataHandle arrayHandle = data.inputArrayValue(attributeDataList, &stat);
		CHECK_MSTATUS_AND_RETURN_IT(stat);
		
		const unsigned int count = arrayHandle.elementCount(&stat);
		DOMMY_RETURN_IF_FAILURE(stat);
		
		
		for(size_t i = 0; i < count; i++) {
			stat = arrayHandle.jumpToArrayElement(i);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
			MDataHandle attrHnd = arrayHandle.inputValue(&stat);
			CHECK_MSTATUS_AND_RETURN_IT(stat);
			const DA_CREATE_TYPE attrType = attrHnd.child(attributeType).asShort();
			const DP_ATTRIBUTE_CLASS_TYPE attrClassType = attrHnd.child(aAttributeClass).asShort();
			std::string attrName = attrHnd.child(attributeName).asString().asChar();
			int valueSizeVal = attrHnd.child(valueSize).asInt();
			float3 &defaultVal = attrHnd.child(defaultValue).asFloat3();
			float3 &applyVal = attrHnd.child(aSetValue).asFloat3();
			
			const short precision = attrHnd.child(aPrecision).asShort();
			short exActType = attrHnd.child(existActionType).asShort();
			
			
			
			
			
			
			
			
			
			
			
			if (attrName.size() == 0) {
				MGlobal::displayWarning("Attribute name is empty.");
				continue;
			}
			
            const dommy::attribute::AttributeCreationPolicy creation(exActType != 1);
            if (attrClassType == DP_ATTR_CLASS_POINT) {
                if (!creation.hasPointTargets(outDCrvs->beginCurves(), outDCrvs->endCurves(), attrName)) continue;
            } else if (attrClassType == DP_ATTR_CLASS_PRIMITIVE) {
                if (!creation.shouldCreate(*outDCrvs, attrName)) continue;
            } else {
                return MS::kInvalidParameter;
            }
			
#pragma mark connect input data for DC
			//データがコネクトされている場合、優先的に利用する
			MFnData::Type inDataType;
			MPlug inputDataPlug(thisMObject(), inputData);
			const bool inputConnected = inputDataPlug.isConnected(&stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			if (inputConnected) {
				MDataHandle dataArrayHnd = attrHnd.child(inputData);
				inDataType = dataArrayHnd.type();
				
				
				
				
				
				
				
				
				if (inDataType == MFnData::kNumeric) {
#pragma mark input numeric
					
					
					
					long double val = 0.0L;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::readNumericScalar(dataArrayHnd, val));
					
					

					if (attrType == DA_CREATE_INT) {
						
						
						
						if(attrClassType==DP_ATTR_CLASS_POINT) {
							
							for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<int32_t>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
								
							}
							
						} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<int32_t>(*outDCrvs, attrName, val, defaultVal[0], curveCount));
							
						}
						
						
						continue;
					} else if (attrType == DA_CREATE_FLOAT) {
						
						if (precision==0) {//float
							
							if(attrClassType==DP_ATTR_CLASS_POINT) {
								
								for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
									
									DOMMY_RETURN_IF_FAILURE(
									    creation.numeric<float>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
									
								}
								
							} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<float>(*outDCrvs, attrName, val, defaultVal[0], curveCount));
								
							}
							
							
							
						} else {//double
							
							if(attrClassType==DP_ATTR_CLASS_POINT) {
								
								for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
									
									DOMMY_RETURN_IF_FAILURE(
									    creation.numeric<double>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
									
								}
								
							} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<double>(*outDCrvs, attrName, val, defaultVal[0], curveCount));
								
							}
							
							
						}
						
						continue;
					} else if (attrType == DA_CREATE_BOOL) {

						
						
						if(attrClassType==DP_ATTR_CLASS_POINT) {
							
							for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<bool>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
								
							}
							
						} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<bool>(*outDCrvs, attrName, val, defaultVal[0], curveCount));
							
						}
						
						
						continue;
					}
					
				} else if (inDataType == MFnData::kString) {
#pragma mark input string
					
					
					std::string val = dataArrayHnd.asString().asChar();
					std::string defStr = "";
					
					if(attrClassType==DP_ATTR_CLASS_POINT) {
						
						for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.create<DPStringAttr>(**cIt, attrName, val, defStr, (*cIt)->numberOfPoints()));
							
						}
						
					} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.create<DPStringAttr>(*outDCrvs, attrName, val, defStr, curveCount));
						
					}
					
					
					continue;

				} else if (inDataType == MFnData::kMatrix) {
#pragma mark input matrix
					
					
					MMatrix val = dataArrayHnd.asMatrix();
					MMatrix defMat = MMatrix();
					
					if(attrClassType==DP_ATTR_CLASS_POINT) {
						
						for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.create<DPMat4Attr>(**cIt, attrName, val, defMat, (*cIt)->numberOfPoints()));
							
						}
						
					} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.create<DPMat4Attr>(*outDCrvs, attrName, val, defMat, curveCount));
						
					}
					
					
					continue;
					
				} else if (inDataType == MFnData::kVectorArray) {
#pragma mark input vector array
					
					
					MFnVectorArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MVectorArray pts = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					
					if(precision == 0){// float
						std::vector<Imath::V3f> ptArray;
						
						if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
						
						if(attrClassType==DP_ATTR_CLASS_POINT) {
							
							if (!pts.length() || pts.length()!=outDCrvs->getTotalCvs()) {
								MGlobal::displayWarning("Input vector size is not equal point size!");
								continue;
							}
							
							size_t totalIndexOffset=0;
							for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
								std::vector<Imath::V3f> primPtArray((*cIt)->numberOfPoints());
								for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
									primPtArray[p] = ptArray[totalIndexOffset];
									totalIndexOffset++;
								}
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<Imath::V3f>(**cIt, attrName, primPtArray, Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
								
							}
							
						} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
							
							if (!pts.length() || pts.length()!=curveCount) {
								MGlobal::displayWarning("Input vector size is not equal primitive size!");
								continue;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3f>(*outDCrvs, attrName, ptArray,Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
							
						}
						
						
					}  else {//double
						std::vector<Imath::V3d> ptArray;
						
						if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
						
						if(attrClassType==DP_ATTR_CLASS_POINT) {
							
							if (!pts.length() || pts.length()!=outDCrvs->getTotalCvs()) {
								MGlobal::displayWarning("Input vector size is not equal point size!");
								continue;
							}
							
							size_t totalIndexOffset=0;
							for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
								std::vector<Imath::V3d> primPtArray((*cIt)->numberOfPoints());
								for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
									primPtArray[p] = ptArray[totalIndexOffset];
									totalIndexOffset++;
								}
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<Imath::V3d>(**cIt, attrName, primPtArray, Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
								
							}
							
						} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
							
							if (!pts.length() || pts.length()!=curveCount) {
								MGlobal::displayWarning("Input vector size is not equal primitive size!");
								continue;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3d>(*outDCrvs, attrName, ptArray,Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
							
						}
					}
					
					continue;
				} else if (inDataType == MFnData::kPointArray) {
#pragma mark input point array
					
					
					MFnPointArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MPointArray pts = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					
					
					
					if(precision == 0){// float
						std::vector<Imath::V3f> ptArray;
						
						if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
						
						if(attrClassType==DP_ATTR_CLASS_POINT) {
							
							if (!pts.length() || pts.length()!=outDCrvs->getTotalCvs()) {
								MGlobal::displayWarning("Input vector size is not equal point size!");
								continue;
							}
							
							size_t totalIndexOffset=0;
							for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
								std::vector<Imath::V3f> primPtArray((*cIt)->numberOfPoints());
								for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
									primPtArray[p] = ptArray[totalIndexOffset];
									totalIndexOffset++;
								}
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<Imath::V3f>(**cIt, attrName, primPtArray, Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
								
							}
							
						} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
							
							if (!pts.length() || pts.length()!=curveCount) {
								MGlobal::displayWarning("Input vector size is not equal primitive size!");
								continue;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3f>(*outDCrvs, attrName, ptArray,Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
							
						}
						
						
					}  else {//double
						std::vector<Imath::V3d> ptArray;
						
						if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
						
						if(attrClassType==DP_ATTR_CLASS_POINT) {
							
							if (!pts.length() || pts.length()!=outDCrvs->getTotalCvs()) {
								MGlobal::displayWarning("Input vector size is not equal point size!");
								continue;
							}
							
							size_t totalIndexOffset=0;
							for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
								std::vector<Imath::V3d> primPtArray((*cIt)->numberOfPoints());
								for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
									primPtArray[p] = ptArray[totalIndexOffset];
									totalIndexOffset++;
								}
								
								DOMMY_RETURN_IF_FAILURE(
								    creation.numeric<Imath::V3d>(**cIt, attrName, primPtArray, Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
								
							}
							
						} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
							
							if (!pts.length() || pts.length()!=curveCount) {
								MGlobal::displayWarning("Input vector size is not equal primitive size!");
								continue;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<Imath::V3d>(*outDCrvs, attrName, ptArray,Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
							
						}
					}
					
					continue;
					
				} else if (inDataType == MFnData::kFloatArray) {
#pragma mark input float array
					
					
					MFnFloatArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MFloatArray dataArray = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					std::vector<float> newArray;
					
					for (uint i=0; i<dataArray.length(); i++) {
						newArray.push_back(dataArray[i]);
					}
					
					if(attrClassType==DP_ATTR_CLASS_POINT) {
						
						if (!newArray.size() || newArray.size()!=outDCrvs->getTotalCvs()) {
							MGlobal::displayWarning("Input float array size is not equal point size!");
							continue;
						}
						
						size_t totalIndexOffset=0;
						for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
							std::vector<float> primPtArray((*cIt)->numberOfPoints());
							for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
								primPtArray[p] = newArray[totalIndexOffset];
								totalIndexOffset++;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<float>(**cIt, attrName, primPtArray, defaultVal[0]));
							
						}
						
					} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
						
						if (!newArray.size() || newArray.size()!=curveCount) {
							MGlobal::displayWarning("Input float array size is not equal primitive size!");
							continue;
						}
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<float>(*outDCrvs, attrName, newArray, defaultVal[0]));
						
					}
					
					continue;
				} else if (inDataType == MFnData::kDoubleArray) {
#pragma mark input double array
					
					MFnDoubleArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MDoubleArray dataArray = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					std::vector<double> newArray;
					for (uint i=0; i<dataArray.length(); i++) {
						newArray.push_back(dataArray[i]);
					}

					
					if(attrClassType==DP_ATTR_CLASS_POINT) {
						
						if (!newArray.size() || newArray.size()!=outDCrvs->getTotalCvs()) {
							MGlobal::displayWarning("Input double array size is not equal point size!");
							continue;
						}
						
						size_t totalIndexOffset=0;
						for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
							std::vector<double> primPtArray((*cIt)->numberOfPoints());
							for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
								primPtArray[p] = newArray[totalIndexOffset];
								totalIndexOffset++;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<double>(**cIt, attrName, primPtArray, defaultVal[0]));
							
						}
						
					} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
						
						if (!newArray.size() || newArray.size()!=curveCount) {
							MGlobal::displayWarning("Input double array size is not equal primitive size!");
							continue;
						}
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<double>(*outDCrvs, attrName, newArray, defaultVal[0]));
						
					}
					
					continue;
				} else if (inDataType == MFnData::kIntArray) {
#pragma mark input int array
					
					MFnIntArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MIntArray dataArray = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					std::vector<int> newArray;
					for (uint i=0; i<dataArray.length(); i++) {
						newArray.push_back(dataArray[i]);
					}
					
					
					if(attrClassType==DP_ATTR_CLASS_POINT) {
						
						if (!newArray.size() || newArray.size()!=outDCrvs->getTotalCvs()) {
							MGlobal::displayWarning("Input int array size is not equal point size!");
							continue;
						}
						
						size_t totalIndexOffset=0;
						for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
							std::vector<int> primPtArray((*cIt)->numberOfPoints());
							for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
								primPtArray[p] = newArray[totalIndexOffset];
								totalIndexOffset++;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.numeric<int32_t>(**cIt, attrName, primPtArray, defaultVal[0]));
							
						}
						
					} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
						
						if (!newArray.size() || newArray.size()!=curveCount) {
							MGlobal::displayWarning("Input int array size is not equal primitive size!");
							continue;
						}
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.numeric<int32_t>(*outDCrvs, attrName, newArray, defaultVal[0]));
						
					}
					continue;
				} else if (inDataType == MFnData::kStringArray) {
#pragma mark input string array
					
					
					MFnStringArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MStringArray dataArray = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					std::vector<std::string> newArray;
					for (uint i=0; i<dataArray.length(); i++) {
						newArray.push_back(dataArray[i].asChar());
					}
					
					if(attrClassType==DP_ATTR_CLASS_POINT) {
						
						if (!newArray.size() || newArray.size()!=outDCrvs->getTotalCvs()) {
							MGlobal::displayWarning("Input int array size is not equal point size!");
							continue;
						}
						
						size_t totalIndexOffset=0;
						for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
							std::vector<std::string> primPtArray((*cIt)->numberOfPoints());
							for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
								primPtArray[p] = newArray[totalIndexOffset];
								totalIndexOffset++;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.create<DPStringAttr>(**cIt, attrName, primPtArray, ""));
							
						}
						
					} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
						
						if (!newArray.size() || newArray.size()!=curveCount) {
							MGlobal::displayWarning("Input int array size is not equal primitive size!");
							continue;
						}
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.create<DPStringAttr>(*outDCrvs, attrName, newArray, ""));
						
					}
					
					continue;
				} else if (inDataType == MFnData::kMatrixArray) {
#pragma mark input matrix array
					
					MFnMatrixArrayData pData(dataArrayHnd.data(), &stat);
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					MMatrixArray dataArray = pData.array( &stat );
					CHECK_MSTATUS_AND_RETURN_IT(stat);
					
					std::vector<MMatrix> newArray;
					for (uint i=0; i<dataArray.length(); i++) {
						newArray.push_back(dataArray[i]);
					}
					
					if(attrClassType==DP_ATTR_CLASS_POINT) {
						
						if (!newArray.size() || newArray.size()!=outDCrvs->getTotalCvs()) {
							MGlobal::displayWarning("Input int array size is not equal point size!");
							continue;
						}
						
						size_t totalIndexOffset=0;
						for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
							std::vector<MMatrix> primPtArray((*cIt)->numberOfPoints());
							for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
								primPtArray[p] = newArray[totalIndexOffset];
								totalIndexOffset++;
							}
							
							DOMMY_RETURN_IF_FAILURE(
							    creation.create<DPMat4Attr>(**cIt, attrName, primPtArray, MMatrix()));
							
						}
						
					} else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
						
						if (!newArray.size() || newArray.size()!=curveCount) {
							MGlobal::displayWarning("Input int array size is not equal primitive size!");
							continue;
						}
						
						DOMMY_RETURN_IF_FAILURE(
						    creation.create<DPMat4Attr>(*outDCrvs, attrName, newArray, MMatrix()));
						
					}
					
					continue;
				}
			}
			
			
			
			
			
			
#pragma mark data create from attribute for DC
            
            
			if (attrType==DA_CREATE_FLOAT) {
				
				if(precision==0) {//float
					if (valueSizeVal==1) {
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<float>(**cIt, attrName, applyVal[0], defaultVal[0], (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<float>(*outDCrvs, attrName, applyVal[0], defaultVal[0], curveCount));
                            
                        }
                        
					} else if(valueSizeVal==2) {
						
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V2f>(**cIt, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                      Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V2f>(*outDCrvs, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                    Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                    curveCount));
                            
                        }
                        
						
						
					} else {
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3f>(**cIt, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                      Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3f>(*outDCrvs, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                    Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                    curveCount));
                            
                        }
						
						
						
					}
				} else {//double
					if (valueSizeVal==1) {
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<double>(**cIt, attrName, applyVal[0], defaultVal[0], (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<double>(*outDCrvs, attrName, applyVal[0],
                                                                     defaultVal[0],
                                                                     curveCount));
                            
                        }
						
						
					} else if(valueSizeVal==2) {
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V2d>(**cIt, attrName, Imath::V2d(applyVal[0], applyVal[1]),
                                                                      Imath::V2d(defaultVal[0], defaultVal[1]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V2d>(*outDCrvs, attrName, Imath::V2d(applyVal[0],
                                                                               applyVal[1]),
                                                                    Imath::V2d(defaultVal[0],
                                                                               defaultVal[1]),
                                                                    curveCount));
                            
                        }
						
						
					} else {
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3d>(**cIt, attrName, Imath::V3d(applyVal[0], applyVal[1], applyVal[2]),
                                                                      Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3d>(*outDCrvs, attrName, Imath::V3d(applyVal[0], applyVal[1], applyVal[2]),
                                                                    Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2]),
                                                                    curveCount));
                            
                        }
						
					}
				}
				
				
				
			} else if (attrType==DA_CREATE_INT) {
				
				if (valueSizeVal==1) {
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<int32_t>(**cIt, attrName, applyVal[0],
                                                                  defaultVal[0],
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<int32_t>(*outDCrvs, attrName, applyVal[0],
                                                              defaultVal[0],
                                                              curveCount));
                        
                    }
                    
                    
					
					
				} else if(valueSizeVal==2) {
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V2i>(**cIt, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V2i>(*outDCrvs, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                curveCount));
                        
                    }
                    
					
				} else {
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3i>(**cIt, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                  Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V3i>(*outDCrvs, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                curveCount));
                        
                    }
                    
					
				}
				
			} else if (attrType==DA_CREATE_VECTOR) {
				
				if(precision==0) {//float
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3f>(**cIt, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                  Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V3f>(*outDCrvs, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                curveCount));
                        
                    }
                    
					
				} else {//double
					
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3d>(**cIt, attrName, Imath::V3d(applyVal[0],applyVal[1],applyVal[2]),
                                                                  Imath::V3d(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V3d>(*outDCrvs, attrName, Imath::V3d(applyVal[0],applyVal[1],applyVal[2]),
                                                                Imath::V3d(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                curveCount));
                        
                    }
                    
					
				}
				
				
				
				
			} else if (attrType==DA_CREATE_STRING) {
                
                if(attrClassType==DP_ATTR_CLASS_POINT) {
                    
                    for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPStringAttr>(**cIt, attrName, "", "", (*cIt)->numberOfPoints()));
                        
                    }
                    
                } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                    
                    DOMMY_RETURN_IF_FAILURE(
                        creation.create<DPStringAttr>(*outDCrvs, attrName, "", "", curveCount));
                    
                }
                
				
				
			} else if (attrType==DA_CREATE_MATRIX) {
				MMatrix defMat;
                if(attrClassType==DP_ATTR_CLASS_POINT) {
                    
                    for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPMat4Attr>(**cIt, attrName, defMat, defMat, (*cIt)->numberOfPoints()));
                        
                    }
                    
                } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                    
                    
                    DOMMY_RETURN_IF_FAILURE(
                        creation.create<DPMat4Attr>(*outDCrvs, attrName, defMat, defMat, curveCount));
                    
                }
				
				
			} else if (attrType==DA_CREATE_BOOL) {
				
                if(attrClassType==DP_ATTR_CLASS_POINT) {
                    
                    for(auto cIt=outDCrvs->beginCurves(),ecIt=outDCrvs->endCurves();cIt!=ecIt;++cIt) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<bool>(**cIt, attrName, applyVal[0], defaultVal[0], (*cIt)->numberOfPoints()));
                        
                    }
                    
                } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                    
                    DOMMY_RETURN_IF_FAILURE(
                        creation.numeric<bool>(*outDCrvs, attrName, applyVal[0], defaultVal[0], curveCount));
                }
                
				
				
			}
			
		}
		
		
		
		
		
#pragma mark output DC
		
		MDataHandle output = data.outputValue(outDC, &stat);
		if (!stat) return stat;
		stat = dommy::maya::publishPluginDataObject(
			output, pluginData.object(), *outDCrvs);
		if (!stat) return stat;
		
		return data.setClean(plug);
		
	}
	
    
    
    
    if( plug == outDM ) {
#pragma mark ---------DM Plug---------
        
        
        
        
        const DMeshData* inputDMData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, inputDMData));
        if (!inputDMData) return MS::kFailure;
        
        const std::size_t inputMeshCount = inputDMData->numberOfMesh();
        dommy::point::PointPayloadBudget retained;
        dommy::point::preflightPrimitivePointPayload(
            *inputDMData, inputMeshCount, sizeof(DPoint), "", 0U, retained);
        retained.add(inputMeshCount, sizeof(MMatrix));
        for (auto item = inputDMData->beginMeshes(); item != inputDMData->endMeshes(); ++item) {
            // Face/corner topology, UV coordinates and possible UV assignments.
            retained.add(static_cast<std::size_t>((*item)->numFace()), 2U * sizeof(int));
            retained.add((*item)->numberOfConnections(), 2U * sizeof(int));
            retained.add((*item)->numberOfUVs(), 2U * sizeof(float));
            dommy::point::preflightAttributePayload((*item)->beginFaceAttr(), (*item)->endFaceAttr(),
                static_cast<std::size_t>((*item)->numFace()), retained);
            dommy::point::preflightAttributePayload((*item)->beginPVtxAttr(), (*item)->endPVtxAttr(),
                (*item)->numberOfConnections(), retained);
            dommy::point::preflightGroupPayload((*item)->beginFaceGrp(), (*item)->endFaceGrp(), retained);
            dommy::point::preflightGroupPayload((*item)->beginEdgeGrp(), (*item)->endEdgeGrp(), retained);
        }
        dommy::attribute::AttributeCreatePlan creationPlan(retained);
        creationPlan.addTarget(*inputDMData, inputMeshCount);
        for (auto item = inputDMData->beginMeshes(); item != inputDMData->endMeshes(); ++item)
            creationPlan.addTarget(**item, (*item)->numberOfPoints());
        const dommy::maya::AttributeCreateControls controls = {
            attributeDataList, attributeType, aAttributeClass, attributeName,
            valueSize, aPrecision, existActionType, inputData};
        if (inputMeshCount != 0U)
            DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightAttributeCreateRows(
                data, thisMObject(), controls, true, creationPlan));

        MFnPluginData pluginData;
        DMeshData* outDMeshes = nullptr;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::createPluginData(
            pluginData, DMeshData::id, outDMeshes));
        if (!outDMeshes) return MS::kFailure;
        
        DOMMY_RETURN_IF_FAILURE(outDMeshes->copyFrom(*inputDMData));
        
        size_t meshCount = outDMeshes->numberOfMesh();
        
        if(meshCount==0) {
            MDataHandle output = data.outputValue(outDM, &stat);
            if (!stat) return stat;
            stat = dommy::maya::publishPluginDataObject(
                output, pluginData.object(), *outDMeshes);
            if (!stat) return stat;
            return data.setClean(plug);
        }
        
        
		size_t totalVtx;
		outDMeshes->getTotalVertex(totalVtx);
		
        // Read from attributeDataList
        MArrayDataHandle arrayHandle = data.inputArrayValue(attributeDataList, &stat);
        CHECK_MSTATUS_AND_RETURN_IT(stat);
        
        const unsigned int count = arrayHandle.elementCount(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        
        
        for(size_t i = 0; i < count; i++) {
            stat = arrayHandle.jumpToArrayElement(i);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            MDataHandle attrHnd = arrayHandle.inputValue(&stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            const DA_CREATE_TYPE attrType = attrHnd.child(attributeType).asShort();
            const DP_ATTRIBUTE_CLASS_TYPE attrClassType = attrHnd.child(aAttributeClass).asShort();
            std::string attrName = attrHnd.child(attributeName).asString().asChar();
            int valueSizeVal = attrHnd.child(valueSize).asInt();
            float3 &defaultVal = attrHnd.child(defaultValue).asFloat3();
            float3 &applyVal = attrHnd.child(aSetValue).asFloat3();
            
            const short precision = attrHnd.child(aPrecision).asShort();
            short exActType = attrHnd.child(existActionType).asShort();
            
            
            
            
            
            
            
            
            
            
            
            if (attrName.size() == 0) {
                MGlobal::displayWarning("Attribute name is empty.");
                continue;
            }
            
            const dommy::attribute::AttributeCreationPolicy creation(exActType != 1);
            if (attrClassType == DP_ATTR_CLASS_POINT) {
                if (!creation.hasPointTargets(outDMeshes->beginMeshes(), outDMeshes->endMeshes(), attrName)) continue;
            } else if (attrClassType == DP_ATTR_CLASS_PRIMITIVE) {
                if (!creation.shouldCreate(*outDMeshes, attrName)) continue;
            } else {
                return MS::kInvalidParameter;
            }
            
#pragma mark connect input data for DM
            //データがコネクトされている場合、優先的に利用する
            MFnData::Type inDataType;
            MPlug inputDataPlug(thisMObject(), inputData);
            const bool inputConnected = inputDataPlug.isConnected(&stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            if (inputConnected) {
                MDataHandle dataArrayHnd = attrHnd.child(inputData);
                inDataType = dataArrayHnd.type();
                
                
                
                
                
                
                
				
				
                if (inDataType == MFnData::kNumeric) {
#pragma mark input numeric
                    
                    
                    
                    long double val = 0.0L;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::readNumericScalar(dataArrayHnd, val));
                    
                    
                    if (attrType == DA_CREATE_INT) {
                        
                        
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<int32_t>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<int32_t>(*outDMeshes, attrName, val, defaultVal[0], meshCount));
                            
                        }
                        
                        
                        continue;
                    } else if (attrType == DA_CREATE_FLOAT) {
                        
                        if (precision==0) {//float
                            
                            if(attrClassType==DP_ATTR_CLASS_POINT) {
                                
                                for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                    
                                    DOMMY_RETURN_IF_FAILURE(
                                        creation.numeric<float>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
                                    
                                }
                                
                            } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<float>(*outDMeshes, attrName, val, defaultVal[0], meshCount));
                                
                            }
                            
                            
                            
                        } else {//double
                            
                            if(attrClassType==DP_ATTR_CLASS_POINT) {
                                
                                for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                    
                                    DOMMY_RETURN_IF_FAILURE(
                                        creation.numeric<double>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
                                    
                                }
                                
                            } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<double>(*outDMeshes, attrName, val, defaultVal[0], meshCount));
                                
                            }
                            
                            
                        }
                        
                        continue;
                    } else if (attrType == DA_CREATE_BOOL) {
                        
                        
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<bool>(**cIt, attrName, val, defaultVal[0], (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<bool>(*outDMeshes, attrName, val, defaultVal[0], meshCount));
                            
                        }
                        
                        
                        continue;
                    }
                    
                } else if (inDataType == MFnData::kString) {
#pragma mark input string
                    
                    
                    std::string val = dataArrayHnd.asString().asChar();
                    std::string defStr = "";
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.create<DPStringAttr>(**cIt, attrName, val, defStr, (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPStringAttr>(*outDMeshes, attrName, val, defStr, meshCount));
                        
                    }
                    
                    
                    continue;
                    
                } else if (inDataType == MFnData::kMatrix) {
#pragma mark input matrix
                    
                    
                    MMatrix val = dataArrayHnd.asMatrix();
                    MMatrix defMat = MMatrix();
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.create<DPMat4Attr>(**cIt, attrName, val, defMat, (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPMat4Attr>(*outDMeshes, attrName, val, defMat, meshCount));
                        
                    }
                    
                    
                    continue;
                    
                } else if (inDataType == MFnData::kVectorArray) {
#pragma mark input vector array
                    
                    
                    MFnVectorArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MVectorArray pts = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    
                    if(precision == 0){// float
                        std::vector<Imath::V3f> ptArray;
                        
                        if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            if (!pts.length() || pts.length()!=totalVtx) {
                                MGlobal::displayWarning("Input vector size is not equal point size!");
                                continue;
                            }
                            
                            size_t totalIndexOffset=0;
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                std::vector<Imath::V3f> primPtArray((*cIt)->numberOfPoints());
                                for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                    primPtArray[p] = ptArray[totalIndexOffset];
                                    totalIndexOffset++;
                                }
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3f>(**cIt, attrName, primPtArray, Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            if (!pts.length() || pts.length()!=meshCount) {
                                MGlobal::displayWarning("Input vector size is not equal primitive size!");
                                continue;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3f>(*outDMeshes, attrName, ptArray,Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
                            
                        }
                        
                        
                    }  else {//double
                        std::vector<Imath::V3d> ptArray;
                        
                        if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            if (!pts.length() || pts.length()!=totalVtx) {
                                MGlobal::displayWarning("Input vector size is not equal point size!");
                                continue;
                            }
                            
                            size_t totalIndexOffset=0;
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                std::vector<Imath::V3d> primPtArray((*cIt)->numberOfPoints());
                                for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                    primPtArray[p] = ptArray[totalIndexOffset];
                                    totalIndexOffset++;
                                }
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3d>(**cIt, attrName, primPtArray, Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            if (!pts.length() || pts.length()!=meshCount) {
                                MGlobal::displayWarning("Input vector size is not equal primitive size!");
                                continue;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3d>(*outDMeshes, attrName, ptArray,Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
                            
                        }
                    }
                    
                    continue;
                } else if (inDataType == MFnData::kPointArray) {
#pragma mark input point array
                    
                    
                    MFnPointArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MPointArray pts = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    if(precision == 0){// float
                        std::vector<Imath::V3f> ptArray;
                        
                        if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            if (!pts.length() || pts.length()!=totalVtx) {
                                MGlobal::displayWarning("Input vector size is not equal point size!");
                                continue;
                            }
                            
                            size_t totalIndexOffset=0;
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                std::vector<Imath::V3f> primPtArray((*cIt)->numberOfPoints());
                                for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                    primPtArray[p] = ptArray[totalIndexOffset];
                                    totalIndexOffset++;
                                }
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3f>(**cIt, attrName, primPtArray, Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            if (!pts.length() || pts.length()!=meshCount) {
                                MGlobal::displayWarning("Input vector size is not equal primitive size!");
                                continue;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3f>(*outDMeshes, attrName, ptArray,Imath::V3f(defaultVal[0], defaultVal[1], defaultVal[2])));
                            
                        }
                        
                        
                    }  else {//double
                        std::vector<Imath::V3d> ptArray;
                        
                        if (!dommy::numeric::checkedVectorArrayCopy(pts, pts.length(), pts.length(), ptArray))
                            return MS::kInvalidParameter;
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            if (!pts.length() || pts.length()!=totalVtx) {
                                MGlobal::displayWarning("Input vector size is not equal point size!");
                                continue;
                            }
                            
                            size_t totalIndexOffset=0;
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                std::vector<Imath::V3d> primPtArray((*cIt)->numberOfPoints());
                                for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                    primPtArray[p] = ptArray[totalIndexOffset];
                                    totalIndexOffset++;
                                }
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3d>(**cIt, attrName, primPtArray, Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            if (!pts.length() || pts.length()!=meshCount) {
                                MGlobal::displayWarning("Input vector size is not equal primitive size!");
                                continue;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3d>(*outDMeshes, attrName, ptArray,Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2])));
                            
                        }
                    }
                    
                    
                    
                    continue;
                    
                } else if (inDataType == MFnData::kFloatArray) {
#pragma mark input float array
                    
                    
                    MFnFloatArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MFloatArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    std::vector<float> newArray;
                    
                    for (uint i=0; i<dataArray.length(); i++) {
                        newArray.push_back(dataArray[i]);
                    }
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        if (!newArray.size() || newArray.size()!=totalVtx) {
                            MGlobal::displayWarning("Input float array size is not equal point size!");
                            continue;
                        }
                        
                        size_t totalIndexOffset=0;
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            std::vector<float> primPtArray((*cIt)->numberOfPoints());
                            for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                primPtArray[p] = newArray[totalIndexOffset];
                                totalIndexOffset++;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<float>(**cIt, attrName, primPtArray, defaultVal[0]));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        if (!newArray.size() || newArray.size()!=meshCount) {
                            MGlobal::displayWarning("Input float array size is not equal primitive size!");
                            continue;
                        }
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<float>(*outDMeshes, attrName, newArray, defaultVal[0]));
                        
                    }
                    
                    continue;
                } else if (inDataType == MFnData::kDoubleArray) {
#pragma mark input double array
                    
                    MFnDoubleArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MDoubleArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    std::vector<double> newArray;
                    for (uint i=0; i<dataArray.length(); i++) {
                        newArray.push_back(dataArray[i]);
                    }
                    
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        if (!newArray.size() || newArray.size()!=totalVtx) {
                            MGlobal::displayWarning("Input double array size is not equal point size!");
                            continue;
                        }
                        
                        size_t totalIndexOffset=0;
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            std::vector<double> primPtArray((*cIt)->numberOfPoints());
                            for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                primPtArray[p] = newArray[totalIndexOffset];
                                totalIndexOffset++;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<double>(**cIt, attrName, primPtArray, defaultVal[0]));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        if (!newArray.size() || newArray.size()!=meshCount) {
                            MGlobal::displayWarning("Input double array size is not equal primitive size!");
                            continue;
                        }
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<double>(*outDMeshes, attrName, newArray, defaultVal[0]));
                        
                    }
                    
                    continue;
                } else if (inDataType == MFnData::kIntArray) {
#pragma mark input int array
                    
                    MFnIntArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MIntArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    std::vector<int> newArray;
                    for (uint i=0; i<dataArray.length(); i++) {
                        newArray.push_back(dataArray[i]);
                    }
                    
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        if (!newArray.size() || newArray.size()!=totalVtx) {
                            MGlobal::displayWarning("Input int array size is not equal point size!");
                            continue;
                        }
                        
                        size_t totalIndexOffset=0;
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            std::vector<int> primPtArray((*cIt)->numberOfPoints());
                            for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                primPtArray[p] = newArray[totalIndexOffset];
                                totalIndexOffset++;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<int32_t>(**cIt, attrName, primPtArray, defaultVal[0]));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        if (!newArray.size() || newArray.size()!=meshCount) {
                            MGlobal::displayWarning("Input int array size is not equal primitive size!");
                            continue;
                        }
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<int32_t>(*outDMeshes, attrName, newArray, defaultVal[0]));
                        
                    }
                    
                    continue;
                } else if (inDataType == MFnData::kStringArray) {
#pragma mark input string array
                    
                    
                    MFnStringArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MStringArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    std::vector<std::string> newArray;
                    for (uint i=0; i<dataArray.length(); i++) {
                        newArray.push_back(dataArray[i].asChar());
                    }
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        if (!newArray.size() || newArray.size()!=totalVtx) {
                            MGlobal::displayWarning("Input int array size is not equal point size!");
                            continue;
                        }
                        
                        size_t totalIndexOffset=0;
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            std::vector<std::string> primPtArray((*cIt)->numberOfPoints());
                            for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                primPtArray[p] = newArray[totalIndexOffset];
                                totalIndexOffset++;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.create<DPStringAttr>(**cIt, attrName, primPtArray, ""));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        if (!newArray.size() || newArray.size()!=meshCount) {
                            MGlobal::displayWarning("Input int array size is not equal primitive size!");
                            continue;
                        }
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPStringAttr>(*outDMeshes, attrName, newArray, ""));
                        
                    }
                    
                    continue;
                } else if (inDataType == MFnData::kMatrixArray) {
#pragma mark input matrix array
                    
                    MFnMatrixArrayData pData(dataArrayHnd.data(), &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    MMatrixArray dataArray = pData.array( &stat );
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    
                    std::vector<MMatrix> newArray;
                    for (uint i=0; i<dataArray.length(); i++) {
                        newArray.push_back(dataArray[i]);
                    }
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        if (!newArray.size() || newArray.size()!=totalVtx) {
                            MGlobal::displayWarning("Input int array size is not equal point size!");
                            continue;
                        }
                        
                        size_t totalIndexOffset=0;
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            std::vector<MMatrix> primPtArray((*cIt)->numberOfPoints());
                            for (size_t p=0; p<(*cIt)->numberOfPoints(); p++) {
                                primPtArray[p] = newArray[totalIndexOffset];
                                totalIndexOffset++;
                            }
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.create<DPMat4Attr>(**cIt, attrName, primPtArray, MMatrix()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        if (!newArray.size() || newArray.size()!=meshCount) {
                            MGlobal::displayWarning("Input int array size is not equal primitive size!");
                            continue;
                        }
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPMat4Attr>(*outDMeshes, attrName, newArray, MMatrix()));
                        
                    }
                    
                    continue;
                }
            }
            
            
            
            
            
            
#pragma mark data create from attribute for DC
            
            
            if (attrType==DA_CREATE_FLOAT) {
                
                if(precision==0) {//float
                    if (valueSizeVal==1) {
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<float>(**cIt, attrName, applyVal[0], defaultVal[0], (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<float>(*outDMeshes, attrName, applyVal[0], defaultVal[0], meshCount));
                            
                        }
                        
                    } else if(valueSizeVal==2) {
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V2f>(**cIt, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                      Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V2f>(*outDMeshes, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                    Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                    meshCount));
                            
                        }
                        
                        
                        
                    } else {
                        
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3f>(**cIt, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                      Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3f>(*outDMeshes, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                    Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                    meshCount));
                            
                        }
                        
                        
                        
                    }
                } else {//double
                    if (valueSizeVal==1) {
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<double>(**cIt, attrName, applyVal[0], defaultVal[0], (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<double>(*outDMeshes, attrName, applyVal[0],
                                                                     defaultVal[0],
                                                                     meshCount));
                            
                        }
                        
                        
                    } else if(valueSizeVal==2) {
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V2d>(**cIt, attrName, Imath::V2d(applyVal[0], applyVal[1]),
                                                                      Imath::V2d(defaultVal[0], defaultVal[1]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V2d>(*outDMeshes, attrName, Imath::V2d(applyVal[0],
                                                                               applyVal[1]),
                                                                    Imath::V2d(defaultVal[0],
                                                                               defaultVal[1]),
                                                                    meshCount));
                            
                        }
                        
                        
                    } else {
                        if(attrClassType==DP_ATTR_CLASS_POINT) {
                            
                            for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                                
                                DOMMY_RETURN_IF_FAILURE(
                                    creation.numeric<Imath::V3d>(**cIt, attrName, Imath::V3d(applyVal[0], applyVal[1], applyVal[2]),
                                                                      Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2]),
                                                                      (*cIt)->numberOfPoints()));
                                
                            }
                            
                        } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3d>(*outDMeshes, attrName, Imath::V3d(applyVal[0], applyVal[1], applyVal[2]),
                                                                    Imath::V3d(defaultVal[0], defaultVal[1], defaultVal[2]),
                                                                    meshCount));
                            
                        }
                        
                    }
                }
                
                
                
            } else if (attrType==DA_CREATE_INT) {
                
                if (valueSizeVal==1) {
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<int32_t>(**cIt, attrName, applyVal[0],
                                                                defaultVal[0],
                                                                (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<int32_t>(*outDMeshes, attrName, applyVal[0], defaultVal[0], meshCount));
                        
                    }
                    
                    
                    
                    
                } else if(valueSizeVal==2) {
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V2i>(**cIt, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                  Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V2i>(*outDMeshes, attrName, Imath::V2f(applyVal[0],applyVal[1]),
                                                                Imath::V2f(defaultVal[0],defaultVal[1]),
                                                                meshCount));
                        
                    }
                    
                    
                } else {
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3i>(**cIt, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                  Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V3i>(*outDMeshes, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                meshCount));
                        
                    }
                    
                    
                }
                
            } else if (attrType==DA_CREATE_VECTOR) {
                
                if(precision==0) {//float
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3f>(**cIt, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                  Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V3f>(*outDMeshes, attrName, Imath::V3f(applyVal[0],applyVal[1],applyVal[2]),
                                                                Imath::V3f(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                meshCount));
                        
                    }
                    
                    
                } else {//double
                    
                    if(attrClassType==DP_ATTR_CLASS_POINT) {
                        
                        for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                            
                            DOMMY_RETURN_IF_FAILURE(
                                creation.numeric<Imath::V3d>(**cIt, attrName, Imath::V3d(applyVal[0],applyVal[1],applyVal[2]),
                                                                  Imath::V3d(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                  (*cIt)->numberOfPoints()));
                            
                        }
                        
                    } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<Imath::V3d>(*outDMeshes, attrName, Imath::V3d(applyVal[0],applyVal[1],applyVal[2]),
                                                                Imath::V3d(defaultVal[0],defaultVal[1],defaultVal[2]),
                                                                meshCount));
                        
                    }
                    
                    
                }
                
                
                
                
            } else if (attrType==DA_CREATE_STRING) {
                
                if(attrClassType==DP_ATTR_CLASS_POINT) {
                    
                    for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPStringAttr>(**cIt, attrName, "", "", (*cIt)->numberOfPoints()));
                        
                    }
                    
                } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                    
                    DOMMY_RETURN_IF_FAILURE(
                        creation.create<DPStringAttr>(*outDMeshes, attrName, "", "", meshCount));
                    
                }
                
                
                
            } else if (attrType==DA_CREATE_MATRIX) {
                MMatrix defMat;
                if(attrClassType==DP_ATTR_CLASS_POINT) {
                    
                    for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.create<DPMat4Attr>(**cIt, attrName, defMat, defMat, (*cIt)->numberOfPoints()));
                        
                    }
                    
                } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                    
                    
                    DOMMY_RETURN_IF_FAILURE(
                        creation.create<DPMat4Attr>(*outDMeshes, attrName, defMat, defMat, meshCount));
                    
                }
                
                
            } else if (attrType==DA_CREATE_BOOL) {
                
                if(attrClassType==DP_ATTR_CLASS_POINT) {
                    
                    for(auto cIt=outDMeshes->beginMeshes(),ecIt=outDMeshes->endMeshes();cIt<ecIt;++cIt) {
                        
                        DOMMY_RETURN_IF_FAILURE(
                            creation.numeric<bool>(**cIt, attrName, applyVal[0], defaultVal[0], (*cIt)->numberOfPoints()));
                        
                    }
                    
                } else if (attrClassType==DP_ATTR_CLASS_PRIMITIVE) {
                    
                    DOMMY_RETURN_IF_FAILURE(
                        creation.numeric<bool>(*outDMeshes, attrName, applyVal[0], defaultVal[0], meshCount));
                }
                
                
                
            }
            
        }
        
        
        
        
        
#pragma mark output DC
        
        MDataHandle output = data.outputValue(outDM, &stat);
        if (!stat) return stat;
        stat = dommy::maya::publishPluginDataObject(
            output, pluginData.object(), *outDMeshes);
        if (!stat) return stat;
        
        return data.setClean(plug);
        
    }
	
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPAttributeCreate")



MStatus DPAttributeCreate::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
	
	
    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));
	
	inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
	
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));
	
	
    MFnEnumAttribute enumFn;
    attributeType = enumFn.create( "attributeType", "aty", DA_CREATE_FLOAT,&stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Float", DA_CREATE_FLOAT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Int", DA_CREATE_INT ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Vector", DA_CREATE_VECTOR ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "String", DA_CREATE_STRING ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Matrix", DA_CREATE_MATRIX ));
    DOMMY_RETURN_IF_FAILURE(enumFn.addField( "Bool", DA_CREATE_BOOL ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(enumFn.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(attributeType));
	
	MFnEnumAttribute attrClassEnumFn;
	aAttributeClass = attrClassEnumFn.create( "AttributeClass", "attrcls", 0,&stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.addField( "Point", DP_ATTR_CLASS_POINT ));
	DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.addField( "Primitive", DP_ATTR_CLASS_PRIMITIVE ));
	DOMMY_RETURN_IF_FAILURE(attrClassEnumFn.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aAttributeClass));
	
	
    MFnStringData fnStringData;
    MObject visAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    attributeName = tAttr.create("attributeName", "atn", MFnData::kString, visAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(attributeName));
	
	valueSize = nAttr.create( "valueSize", "vsi", MFnNumericData::kInt , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setMax(3));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(valueSize));
	
	
	defaultValue = nAttr.create( "defaultValue", "dval", MFnNumericData::k3Float , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(defaultValue));
	
	aSetValue = nAttr.create( "SetValue", "sval", MFnNumericData::k3Float , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 0.0, 0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aSetValue));
	
	
	MFnEnumAttribute preEnumFn;
	aPrecision = preEnumFn.create( "Precision", "prec", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	DOMMY_RETURN_IF_FAILURE(preEnumFn.addField( "float", 0 ));
	DOMMY_RETURN_IF_FAILURE(preEnumFn.addField( "double", 1 ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aPrecision));
	
	
    MFnEnumAttribute enumExistFn;
    existActionType = enumExistFn.create( "existActionType", "ety", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Replace", 0 ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Not Add", 1 ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(existActionType));
    
    
    
    
    inputData = gAttr.create( "inputData", "ind",  &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kIntArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kVectorArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kPointArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kFloatArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kDoubleArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kStringArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMatrixArray));
    DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kNumeric));
//	DOMMY_RETURN_IF_FAILURE(gAttr.addNumericDataAccept(MFnNumericData::kBoolean));
//	DOMMY_RETURN_IF_FAILURE(gAttr.addNumericDataAccept(MFnNumericData::kInt));
//	DOMMY_RETURN_IF_FAILURE(gAttr.addNumericDataAccept(MFnNumericData::kFloat));
//	DOMMY_RETURN_IF_FAILURE(gAttr.addNumericDataAccept(MFnNumericData::kDouble));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kString));
	DOMMY_RETURN_IF_FAILURE(gAttr.addDataAccept(MFnData::kMatrix));
    DOMMY_RETURN_IF_FAILURE(gAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(gAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(gAttr.setReadable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inputData));
    
    
    
    
    MFnCompoundAttribute cmpAttr;
    attributeDataList = cmpAttr.create("attributeDataList", "atl", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(attributeName));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(attributeType));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aAttributeClass));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(valueSize));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(defaultValue));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aSetValue));
	DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aPrecision));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(existActionType));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(inputData));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray( true ));
//    cmpAttr.setReadable( false );
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setUsesArrayDataBuilder(true));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setWritable( true ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(attributeDataList));
    
    
    
    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
	
	outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	CHECK_MSTATUS_AND_RETURN_IT(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    CHECK_MSTATUS_AND_RETURN_IT(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));
	
	
	
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeType, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeClass, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeName, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( valueSize, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( defaultValue, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSetValue, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aPrecision, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( existActionType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inputData, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeDataList, outDP ));
	
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeType, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeClass, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeName, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( valueSize, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( defaultValue, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aSetValue, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aPrecision, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( existActionType, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inputData, outDC ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeDataList, outDC ));
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAttributeClass, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeName, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( valueSize, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( defaultValue, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSetValue, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPrecision, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( existActionType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( inputData, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( attributeDataList, outDM ));
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPAttributeCreate", "initialize", std::current_exception());
}
