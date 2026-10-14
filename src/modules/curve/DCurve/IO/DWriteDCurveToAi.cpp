//
//  DWriteDCurveToAi.cpp
//  dommyTool
//
//  Created by yuzo on 2017/01/21.
//
//

#include "DWriteDCurveToAi.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaNameList.h"
#include "PluginFeatures.h"
#include "common/file/DExportPath.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"


#include "DMayaHeaders.h"


#include "DFileOperation.h"
#include "DMayaDebug.h"
#include "DCurveDataHeaders.h"
#include "DCurveArnoldLayout.h"
#include "DArnoldSession.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldAssWrite.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "DEvaluationContext.h"


#include <cmath>
#include <limits>
#include <vector>
#include <sstream>
#include <string>


#include <ai.h>


const MTypeId DWriteDCurveToAi::typeId( ID_DC_WRITE_DCURVE_TO_AI );


MObject DWriteDCurveToAi::inDC;
MObject DWriteDCurveToAi::aFilePath;
MObject DWriteDCurveToAi::aExportMode;
MObject DWriteDCurveToAi::aExportLog;

MObject DWriteDCurveToAi::aIsBinary;

MObject DWriteDCurveToAi::aOutNodeName;

MObject DWriteDCurveToAi::aRenderMode;
MObject DWriteDCurveToAi::basisMode;
MObject DWriteDCurveToAi::isAddEdgeCV;

MObject DWriteDCurveToAi::aCurveWidthType;
MObject DWriteDCurveToAi::aConstantWidth;
MObject DWriteDCurveToAi::aWidthAttributeName;
MObject DWriteDCurveToAi::aMinPixelWidth;

MObject DWriteDCurveToAi::aOrientType;
MObject DWriteDCurveToAi::aOrientAttributeName;

MObject DWriteDCurveToAi::aExportCurveAttribute;
MObject DWriteDCurveToAi::aExportPointAttribute;

MObject DWriteDCurveToAi::outDC;

void *DWriteDCurveToAi::creator()
{
    return ::dommy::maya::createObject("DWriteDCurveToAi", [&]() {
        return new DWriteDCurveToAi();
    });
}


MStatus DWriteDCurveToAi::compute(const MPlug& plug, MDataBlock& data) try
{
    
    if (plug == outDC) {
        MStatus stat;

        
        //input data
        const DCurveData* inputDCData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, inputDCData));
        
        //create output
        MFnPluginData pluginData;
        pluginData.create(DCurveData::id, &stat);
        
        if (stat != MS::kSuccess) {
            EchoWarning(name().asChar(), "Failed to create a new DCurveData object");
            return stat;
        }
        
        
        DCurveData* outDCrvs = nullptr;
        stat = dommy::maya::getPluginData(pluginData, outDCrvs);
        if (!stat) return stat;
        if (stat != MS::kSuccess || !outDCrvs) {
            EchoWarning(name().asChar(), "Failed to access the output DCurveData object");
            return MS::kFailure;
        }
        DOMMY_RETURN_IF_FAILURE(outDCrvs->copyFrom(*inputDCData));
        
        if(outDCrvs->numberOfCurves()==0) {
            MDataHandle output = data.outputValue(outDC, &stat);
            if (!stat) return stat;
            stat = dommy::maya::publishTypedPluginDataObject<DCurveData>(
                output, pluginData.object(), DCurveData::id);
            if (!stat) return stat;
            return data.setClean(plug);
        }
        
        if (outDCrvs) {
            
	            const MDataHandle exportModeHandle = data.inputValue(aExportMode, &stat);
	            DOMMY_RETURN_IF_FAILURE(stat);
	            const short exportModeVal = exportModeHandle.asShort();
				if (exportModeVal < 0 || exportModeVal > 2) {
					EchoWarning(name().asChar(), "ExportMode is invalid");
					return MS::kInvalidParameter;
				}

				bool exportedFile = false;
				if (exportModeVal != 2) {
					const MDataHandle filePathHandle = data.inputValue(aFilePath, &stat);
					DOMMY_RETURN_IF_FAILURE(stat);
					const MString srcFileName = filePathHandle.asString();
					if (srcFileName.length() == 0) {
						EchoWarning(name().asChar(), "FilePath must not be empty");
						return MS::kFailure;
					}

					MTime evaluationTime;
					stat = dommy::maya::getEvaluationTime(data, evaluationTime);
					DOMMY_RETURN_IF_FAILURE(stat);
					MString resolvedPath;
					if (!getSequenceFileName(
							srcFileName, evaluationTime.value(), resolvedPath)) {
						EchoWarning(
							name().asChar(),
							"FilePath contains an invalid frame token");
						return MS::kFailure;
					}
					std::string filename = resolvedPath.asChar();
					if (!dommy::file::isSafeExportPath(filename, ".ass")) {
						EchoWarning(
							name().asChar(),
							"Output path must be a bounded .ass path without "
							"control characters or '..' components");
						return MS::kInvalidParameter;
					}
					

					MFileObject fileObj;
					DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));
                    filename = fileObj.resolvedFullName().asChar();
                    if (!dommy::file::isSafeExportPath(filename, ".ass"))
                        return MS::kInvalidParameter;

					if (exportModeVal == 0 ||
						(exportModeVal == 1 && !fileObj.exists())) {
						auto readInputHandle =
							[&data](const MObject& attribute,
							        MDataHandle& handle) -> MStatus {
								MStatus status;
								handle = data.inputValue(attribute, &status);
								return status;
							};
						MDataHandle basisHandle;
						MDataHandle addEdgeHandle;
						MDataHandle constantWidthHandle;
						MDataHandle curveWidthTypeHandle;
						MDataHandle exportLogHandle;
						MDataHandle binaryHandle;
						MDataHandle nodeNameHandle;
						MDataHandle renderModeHandle;
						MDataHandle minPixelWidthHandle;
						MDataHandle widthAttributeHandle;
						MDataHandle orientTypeHandle;
						MDataHandle orientAttributeHandle;
						MDataHandle exportCurveAttributesHandle;
						MDataHandle exportPointAttributesHandle;
						if (readInputHandle(basisMode, basisHandle) !=
						        MS::kSuccess ||
						    readInputHandle(isAddEdgeCV, addEdgeHandle) !=
						        MS::kSuccess ||
						    readInputHandle(
								aConstantWidth, constantWidthHandle) !=
						        MS::kSuccess ||
						    readInputHandle(
								aCurveWidthType, curveWidthTypeHandle) !=
						        MS::kSuccess ||
						    readInputHandle(aExportLog, exportLogHandle) !=
						        MS::kSuccess ||
						    readInputHandle(aIsBinary, binaryHandle) !=
						        MS::kSuccess ||
						    readInputHandle(aOutNodeName, nodeNameHandle) !=
						        MS::kSuccess ||
						    readInputHandle(aRenderMode, renderModeHandle) !=
						        MS::kSuccess ||
						    readInputHandle(
								aMinPixelWidth, minPixelWidthHandle) !=
						        MS::kSuccess ||
						    readInputHandle(
								aWidthAttributeName, widthAttributeHandle) !=
						        MS::kSuccess ||
						    readInputHandle(aOrientType, orientTypeHandle) !=
						        MS::kSuccess ||
						    readInputHandle(
								aOrientAttributeName, orientAttributeHandle) !=
						        MS::kSuccess ||
						    readInputHandle(
								aExportCurveAttribute,
								exportCurveAttributesHandle) != MS::kSuccess ||
						    readInputHandle(
								aExportPointAttribute,
								exportPointAttributesHandle) != MS::kSuccess) {
							EchoWarning(
								name().asChar(),
								"Unable to read all Arnold curve export "
								"parameters");
							return MS::kFailure;
						}

						const int basisModeVal = basisHandle.asShort();
						const bool isAddEdgeCVVal = addEdgeHandle.asBool();
						const double constantWidth =
							constantWidthHandle.asDouble();
						const short curveWidthType =
							curveWidthTypeHandle.asShort();
						const bool exportLog = exportLogHandle.asBool();
						const bool isBinary = binaryHandle.asBool();
						const std::string nodeName =
							nodeNameHandle.asString().asChar();
						const short modeType = renderModeHandle.asShort();
						const float minPixelWidth =
							minPixelWidthHandle.asFloat();
						const std::string widthAttributeName =
							widthAttributeHandle.asString().asChar();
						const short orientType = orientTypeHandle.asShort();
						const std::string orientAttrName =
							orientAttributeHandle.asString().asChar();
						const MString exportCurveAttributeNames =
							exportCurveAttributesHandle.asString();
						const MString exportPointAttributeNames =
							exportPointAttributesHandle.asString();
						if (basisModeVal < 0 || basisModeVal > 2 ||
						    curveWidthType < 0 || curveWidthType > 2 ||
						    modeType < 0 || modeType > 2 ||
						    orientType < 0 || orientType > 2 ||
						    !std::isfinite(constantWidth) ||
						    constantWidth < 0.0 ||
						    !std::isfinite(minPixelWidth) ||
						    minPixelWidth < 0.0f) {
							EchoWarning(
								name().asChar(),
								"Arnold curve export parameters are invalid");
							return MS::kInvalidParameter;
						}
				
				
                
				const bool isCubicMode = basisModeVal != 0;
				std::vector<dommy::arnold::CurveLayout> curveLayouts;
				curveLayouts.reserve(outDCrvs->numberOfCurves());
				size_t totalCVCount = 0;
				size_t totalParamCount = 0;
				for (auto cIt = outDCrvs->beginCurves(); cIt != outDCrvs->endCurves(); ++cIt) {
					const dommy::arnold::CurveLayout layout = dommy::arnold::CurveLayout::create(
						(*cIt)->numberOfPoints(), isCubicMode, isAddEdgeCVVal);
					if (!layout.valid()) {
						EchoWarning(name().asChar(),
						            "Cannot export an empty curve or a cubic curve with fewer than four CVs.");
						return MS::kFailure;
					}
					if (layout.controlVertexCount() >
					    static_cast<size_t>(std::numeric_limits<int>::max())) {
						EchoWarning(name().asChar(),
						            "A curve has more control vertices than Arnold can represent.");
						return MS::kFailure;
					}
					if (!dommy::arnold::checkedAdd(layout.controlVertexCount(), totalCVCount) ||
					    !dommy::arnold::checkedAdd(layout.varyingCount(), totalParamCount)) {
						EchoWarning(name().asChar(), "Curve export array size overflow.");
						return MS::kFailure;
					}
					curveLayouts.push_back(layout);
				}

				const size_t totalCurveCount = curveLayouts.size();
				dommy::arnold::ArrayLayout cvLayout;
				dommy::arnold::ArrayLayout curveLayout;
				dommy::arnold::ArrayLayout parameterLayout;
				if (!dommy::arnold::checkedArrayLayout(
						totalCVCount, 1U, cvLayout) ||
					!dommy::arnold::checkedArrayLayout(
						totalCurveCount, 1U, curveLayout) ||
					!dommy::arnold::checkedArrayLayout(
						totalParamCount, 1U, parameterLayout)) {
					EchoWarning(
						name().asChar(),
						"Curve export exceeds the Arnold allocation budget.");
					return MS::kFailure;
				}
				
				

				dommy::maya::NameListBudget nameListBudget;
				MStringArray curveAttrNameArray;
				MStringArray pointAttrNameArray;
				DOMMY_RETURN_IF_FAILURE(dommy::maya::splitCheckedNameList(
				    exportCurveAttributeNames, totalCurveCount, nameListBudget, curveAttrNameArray));
				DOMMY_RETURN_IF_FAILURE(dommy::maya::splitCheckedNameList(
				    exportPointAttributeNames, totalCVCount, nameListBudget, pointAttrNameArray));

#pragma mark export start
                dommy::arnold::Session arnoldSession;
                
                if (exportLog) {
                    AiMsgSetLogFileName("dcurveOut.log");
                    AiMsgSetConsoleFlags(AI_LOG_ALL);
                }

				
#pragma mark create curve node
				dommy::arnold::NodeOwner curveNodeOwner(AiNode("curves"));
				if (!curveNodeOwner) {
					EchoWarning(name().asChar(), "Failed to create the Arnold curves node");
					return MS::kFailure;
				}
				AtNode* const nodeCurves = curveNodeOwner.get();
				
				if(!nodeName.empty())
					AiNodeSetStr( nodeCurves, "name", nodeName.c_str() );
				switch (modeType) {
					case 0:
						AiNodeSetStr( nodeCurves, "mode", "ribbon");
						break;
					case 1:
						AiNodeSetStr( nodeCurves, "mode", "thick");
						break;
					case 2:
						AiNodeSetStr( nodeCurves, "mode", "oriented");
					default:
						break;
				}
				AiNodeSetFlt(
					nodeCurves, "min_pixel_width", dommy::arnold::checkedFloat(minPixelWidth));
				
				
#pragma mark set basis
				
				if( basisModeVal==0 ) {
					AiNodeSetStr( nodeCurves, "basis", "linear" );
				} else if( basisModeVal==1 ) {
					AiNodeSetStr( nodeCurves, "basis", "b-spline" );
				} else if( basisModeVal==2 ) {
					  AiNodeSetStr( nodeCurves, "basis", "catmull-rom" );
					} else {
					  AiNodeSetStr( nodeCurves, "basis", "linear" );
                }
				
				
#pragma mark make point
                dommy::arnold::ArrayOwner pointArray(AiArrayAllocate(
                    cvLayout.elementCount, cvLayout.keyCount, AI_TYPE_VECTOR));
                dommy::arnold::ArrayOwner curvePointCountArray(AiArrayAllocate(
                    curveLayout.elementCount, curveLayout.keyCount,
                    AI_TYPE_INT));
				if (!pointArray || !curvePointCountArray) {
					EchoWarning(name().asChar(),
					            "Failed to allocate the required Arnold curve arrays");
					return MS::kFailure;
				}
                
                size_t pointOffset=0;
                size_t curveOffset=0;
                
                size_t layoutIndex = 0;
                for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves();
                     ++cIt, ++layoutIndex) {
                    
                    const DPointList *pts =
                        (*cIt)->getPositionPtrConst();
					const dommy::arnold::CurveLayout& layout = curveLayouts[layoutIndex];
					if (pts == nullptr) {
						EchoWarning(
							name().asChar(),
							"Curve export encountered missing point data");
						return MS::kFailure;
					}
					for (size_t outputIndex = 0; outputIndex < layout.controlVertexCount();
					     ++outputIndex) {
						const size_t sourceIndex = layout.controlVertexSourceIndex(outputIndex);
						if (sourceIndex >= pts->size()) {
							EchoWarning(
								name().asChar(),
								"Curve export layout references an invalid CV");
							return MS::kFailure;
						}
						const DPoint& position = (*pts)[sourceIndex];
						if (!std::isfinite(position[0]) ||
						    !std::isfinite(position[1]) ||
						    !std::isfinite(position[2])) {
							EchoWarning(
								name().asChar(),
								"Curve export points must be finite");
							return MS::kInvalidParameter;
						}
                        AtVector aVec;
						aVec.x = dommy::arnold::checkedFloat(position[0]);
						aVec.y = dommy::arnold::checkedFloat(position[1]);
						aVec.z = dommy::arnold::checkedFloat(position[2]);
                        dommy::arnold::requireArrayWrite(AiArraySetVec(pointArray.get(), pointOffset, aVec));
                        pointOffset++;
                    }
					dommy::arnold::requireArrayWrite(AiArraySetInt(curvePointCountArray.get(), curveOffset,
					              static_cast<int>(layout.controlVertexCount())));
                    curveOffset++;
                }
				
                AiNodeSetArray(nodeCurves, "points", pointArray.release());
                AiNodeSetArray(
					nodeCurves, "num_points", curvePointCountArray.release());
				
				
#pragma mark set curve width
				if (curveWidthType==0) {
					dommy::arnold::ArrayOwner radiusArray(
						AiArrayAllocate(1, 1, AI_TYPE_FLOAT));
					if (!radiusArray) {
						EchoWarning(name().asChar(),
						            "Failed to allocate the Arnold radius array");
						return MS::kFailure;
					}
					dommy::arnold::requireArrayWrite(AiArraySetFlt(radiusArray.get(), 0, dommy::arnold::checkedFloat(constantWidth)));
					AiNodeSetArray(nodeCurves, "radius", radiusArray.release());
				} else if (curveWidthType==1) {//curve attribute
					
					dommy::arnold::ArrayOwner radiusArray(AiArrayAllocate(
						parameterLayout.elementCount,
						parameterLayout.keyCount, AI_TYPE_FLOAT));
					if (!radiusArray) {
						EchoWarning(name().asChar(),
						            "Failed to allocate the Arnold radius array");
						return MS::kFailure;
					}
					
					if (outDCrvs->isExistAttr(widthAttributeName)) {
						std::string typeStr = outDCrvs->getAttributeType(widthAttributeName);
						
						
						
						
						bool isNotType = false;
						std::vector<float> wList;
						if(typeStr=="float") {
							wList = outDCrvs->attrListValue<float>(widthAttributeName);
						} else if (typeStr=="double") {
							std::vector<double> tmp = outDCrvs->attrListValue<double>(widthAttributeName);
							wList = dommy::arnold::checkedFloatValues(tmp);
						} else {
							EchoWarning(name().asChar(), "Curve width attribute is not support type! supported type is float or double");
							isNotType = true;
						}
						
						if (!isNotType && wList.size() != totalCurveCount) {
							EchoWarning(name().asChar(),
							            "Curve width attribute count does not match the curve count.");
							isNotType = true;
						}

						if (!isNotType) {
							size_t pointOffset = 0;
							for (size_t curveIndex = 0; curveIndex < curveLayouts.size(); ++curveIndex) {
								const dommy::arnold::CurveLayout& layout = curveLayouts[curveIndex];
								for (size_t valueIndex = 0; valueIndex < layout.varyingCount(); ++valueIndex)
									dommy::arnold::requireArrayWrite(AiArraySetFlt(
										radiusArray.get(), pointOffset++, dommy::arnold::checkedFloat(wList[curveIndex])));
							}
							AiNodeSetArray(
								nodeCurves, "radius", radiusArray.release());
						}
						
					} else {
						EchoWarning(name().asChar(), "Curve width attribute is not exist!");
					}
					
				} else if (curveWidthType==2) {//point attribute
					
					dommy::arnold::ArrayOwner radiusArray(AiArrayAllocate(
						parameterLayout.elementCount,
						parameterLayout.keyCount, AI_TYPE_FLOAT));
					if (!radiusArray) {
						EchoWarning(name().asChar(),
						            "Failed to allocate the Arnold radius array");
						return MS::kFailure;
					}
					
					bool isNotType = false;
					size_t pointOffset=0;
					
					size_t widthLayoutIndex = 0;
					for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves();
					     ++cIt, ++widthLayoutIndex) {
						const dommy::arnold::CurveLayout& layout = curveLayouts[widthLayoutIndex];
						std::vector<float> srcList;
						if((*cIt)->isExistAttr(widthAttributeName)) {
							std::string typeStr = (*cIt)->getAttrTypeName(widthAttributeName);
							
							if(typeStr=="float") {
								srcList = (*cIt)->attrListValue<float>(widthAttributeName);
							} else if (typeStr=="double") {
								std::vector<double> wList = (*cIt)->attrListValue<double>(widthAttributeName);
								srcList = dommy::arnold::checkedFloatValues(wList);
							} else {
								EchoWarning(name().asChar(), "Not support point attribute type!");
								isNotType = true;
							}
						} else {
							EchoWarning(name().asChar(), "Not Exist point attribute!");
							isNotType = true;
						}
						
						if (isNotType)//when not support type is finish routine.
							break;

						if (srcList.size() != layout.sourcePointCount()) {
							EchoWarning(name().asChar(),
							            "Point width attribute count does not match the curve CV count.");
							isNotType = true;
							break;
						}

						for (size_t valueIndex = 0; valueIndex < layout.varyingCount(); ++valueIndex) {
							const size_t sourceIndex = layout.varyingSourceIndex(valueIndex);
							dommy::arnold::requireArrayWrite(AiArraySetFlt(
								radiusArray.get(), pointOffset++, dommy::arnold::checkedFloat(srcList[sourceIndex])));
						}
					}
					
					if (!isNotType) {
						AiNodeSetArray(
							nodeCurves, "radius", radiusArray.release());
					}
					
				}
				
#pragma mark orient attribute
				if (orientType==1 && modeType==2) {//curve attribute
					dommy::arnold::ArrayOwner orientationArray(AiArrayAllocate(
						cvLayout.elementCount, cvLayout.keyCount, AI_TYPE_VECTOR));
					if (!orientationArray) {
						EchoWarning(name().asChar(),
						            "Failed to allocate the Arnold orientation array");
						return MS::kFailure;
					}
					
					if (outDCrvs->isExistAttr(orientAttrName)) {
						std::string typeStr = outDCrvs->getAttributeType(orientAttrName);
						
						bool isNotType = false;
						std::vector<Imath::V3f> oriList;
						if(typeStr=="vec3f") {
							oriList = outDCrvs->attrListValue<Imath::V3f>(orientAttrName);
						} else if (typeStr=="vec3d") {
							std::vector<Imath::V3d> tmp = outDCrvs->attrListValue<Imath::V3d>(orientAttrName);
							oriList = dommy::arnold::checkedFloatVectors<Imath::V3f>(tmp);
						} else {
							EchoWarning(name().asChar(), "Curve width attribute is not support type! supported type is vec3f or vec3d");
							isNotType = true;
						}
						
						if (!isNotType && oriList.size() != totalCurveCount) {
							EchoWarning(name().asChar(),
							            "Curve orientation attribute count does not match the curve count.");
							isNotType = true;
						}

						if (!isNotType) {
							size_t pointOffset = 0;
							for (size_t curveIndex = 0; curveIndex < curveLayouts.size(); ++curveIndex) {
								AtVector aVec;
								aVec.x = dommy::arnold::checkedFloat(oriList[curveIndex][0]);
								aVec.y = dommy::arnold::checkedFloat(oriList[curveIndex][1]);
								aVec.z = dommy::arnold::checkedFloat(oriList[curveIndex][2]);
								for (size_t valueIndex = 0;
								     valueIndex < curveLayouts[curveIndex].controlVertexCount();
								     ++valueIndex)
									dommy::arnold::requireArrayWrite(AiArraySetVec(
										orientationArray.get(), pointOffset++, aVec));
							}
							AiNodeSetArray(
								nodeCurves, "orientations",
								orientationArray.release());
						}
						
					} else {
						EchoWarning(name().asChar(), "Curve orientation attribute is not exist!");
					}
					
				} else if (orientType==2 && modeType==2) {//point attribute
					
					dommy::arnold::ArrayOwner orientationArray(AiArrayAllocate(
						cvLayout.elementCount, cvLayout.keyCount, AI_TYPE_VECTOR));
					if (!orientationArray) {
						EchoWarning(name().asChar(),
						            "Failed to allocate the Arnold orientation array");
						return MS::kFailure;
					}
					
					bool isNotType = false;
					size_t pointOffset=0;
					
					size_t orientationLayoutIndex = 0;
					for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves();
					     ++cIt, ++orientationLayoutIndex) {
						const dommy::arnold::CurveLayout& layout =
							curveLayouts[orientationLayoutIndex];
						std::vector<Imath::V3f> srcList;
						if((*cIt)->isExistAttr(orientAttrName)) {
							std::string typeStr = (*cIt)->getAttrTypeName(orientAttrName);
							
							if(typeStr=="vec3f") {
								srcList = (*cIt)->attrListValue<Imath::V3f>(orientAttrName);
							} else if (typeStr=="vec3d") {
								std::vector<Imath::V3d> wList = (*cIt)->attrListValue<Imath::V3d>(orientAttrName);
								srcList = dommy::arnold::checkedFloatVectors<Imath::V3f>(wList);
							} else {
								EchoWarning(name().asChar(), "Not support orient point attribute type!");
								isNotType = true;
							}
						} else {
							EchoWarning(name().asChar(), "Not Exist orient point attribute!");
							isNotType = true;
						}
						
						if (isNotType)//when not support type is finish routine.
							break;

						if (srcList.size() != layout.sourcePointCount()) {
							EchoWarning(name().asChar(),
							            "Point orientation attribute count does not match the curve CV count.");
							isNotType = true;
							break;
						}

						for (size_t valueIndex = 0; valueIndex < layout.controlVertexCount();
						     ++valueIndex) {
							const size_t sourceIndex = layout.controlVertexSourceIndex(valueIndex);
							AtVector aVec;
							aVec.x = dommy::arnold::checkedFloat(srcList[sourceIndex][0]);
							aVec.y = dommy::arnold::checkedFloat(srcList[sourceIndex][1]);
							aVec.z = dommy::arnold::checkedFloat(srcList[sourceIndex][2]);
							dommy::arnold::requireArrayWrite(AiArraySetVec(
								orientationArray.get(), pointOffset++, aVec));
						}
					}
					
					if (!isNotType) {
						AiNodeSetArray(
							nodeCurves, "orientations",
							orientationArray.release());
					}
					
				}
				
#pragma mark export curve attribute
                
                ///////////////////////////////////////////////////////////////
                //constant values exist on a per-object basis
                //uniform values exist on a per-face basis
                //varying values exist on a per-vertex basis
                ///////////////////////////////////////////////////////////////
                
                
                
                
                for (uint i=0; i<curveAttrNameArray.length(); i++) {
                    std::string attrName = curveAttrNameArray[i].asChar();
                    
                    
                    if (outDCrvs->isExistAttr(attrName)) {
                        std::string typeStr = outDCrvs->getAttributeType(attrName);
                        const DPAttributeData::ConstPtr attribute = outDCrvs->getAttr(attrName);
                        if (!attribute || attribute->attrSize() != totalCurveCount) {
                            EchoWarning(attrName.c_str(),
                                        "Uniform attribute count does not match the curve count.");
                            continue;
                        }
                        
                        if (typeStr == "int") {
                            std::vector<int> tmp = outDCrvs->attrListValue<int>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_INT));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold integer attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                dommy::arnold::requireArrayWrite(AiArraySetInt(outArray.get(), i, tmp[i]));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform INT");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                        } else if (typeStr == "float") {
                            
                            std::vector<float> tmp = outDCrvs->attrListValue<float>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_FLOAT));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold float attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                dommy::arnold::requireArrayWrite(AiArraySetFlt(outArray.get(), i, dommy::arnold::checkedFloat(tmp[i])));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform FLOAT");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                            
                        } else if (typeStr == "double") {
                            
                            std::vector<double> tmp = outDCrvs->attrListValue<double>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_FLOAT));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold float attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                dommy::arnold::requireArrayWrite(AiArraySetFlt(
									outArray.get(), i, dommy::arnold::checkedFloat(tmp[i])));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform FLOAT");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                            
                        } else if (typeStr == "vec2i") {
                            std::vector<Imath::V2i> tmp = outDCrvs->attrListValue<Imath::V2i>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR2));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector2 attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                AtVector2 pt2;
                                pt2.x = dommy::arnold::checkedFloat(tmp[i][0]);
                                pt2.y = dommy::arnold::checkedFloat(tmp[i][1]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec2(outArray.get(), i, pt2));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform POINT2");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                        } else if (typeStr == "vec2f") {
                            std::vector<Imath::V2f> tmp = outDCrvs->attrListValue<Imath::V2f>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR2));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector2 attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                AtVector2 pt2;
                                pt2.x = dommy::arnold::checkedFloat(tmp[i][0]);
                                pt2.y = dommy::arnold::checkedFloat(tmp[i][1]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec2(outArray.get(), i, pt2));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform POINT2");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                        } else if (typeStr == "vec2d") {
                            std::vector<Imath::V2d> tmp = outDCrvs->attrListValue<Imath::V2d>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR2));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector2 attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                AtVector2 pt2;
                                pt2.x = dommy::arnold::checkedFloat(tmp[i][0]);
                                pt2.y = dommy::arnold::checkedFloat(tmp[i][1]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec2(outArray.get(), i, pt2));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform POINT2");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                        } else if (typeStr == "vec3i") {
                            std::vector<Imath::V3i> tmp = outDCrvs->attrListValue<Imath::V3i>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                AtVector vec;
                                vec.x = dommy::arnold::checkedFloat(tmp[i][0]);
                                vec.y = dommy::arnold::checkedFloat(tmp[i][1]);
                                vec.z = dommy::arnold::checkedFloat(tmp[i][2]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), i, vec));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform VECTOR");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                        } else if (typeStr == "vec3f") {
                            std::vector<Imath::V3f> tmp = outDCrvs->attrListValue<Imath::V3f>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                AtVector vec;
                                vec.x = dommy::arnold::checkedFloat(tmp[i][0]);
                                vec.y = dommy::arnold::checkedFloat(tmp[i][1]);
                                vec.z = dommy::arnold::checkedFloat(tmp[i][2]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), i, vec));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform VECTOR");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                        } else if (typeStr == "vec3d") {
                            std::vector<Imath::V3d> tmp = outDCrvs->attrListValue<Imath::V3d>(attrName);
                            
                            dommy::arnold::ArrayOwner outArray(
								AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector attribute");
								return MS::kFailure;
							}
                            
                            for (size_t i = 0; i < tmp.size(); i++) {
                                AtVector vec;
                                vec.x = dommy::arnold::checkedFloat(tmp[i][0]);
                                vec.y = dommy::arnold::checkedFloat(tmp[i][1]);
                                vec.z = dommy::arnold::checkedFloat(tmp[i][2]);
                                dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), i, vec));
                            }
                            dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "uniform VECTOR");
                            AiNodeSetArray(
								nodeCurves, attrName.c_str(), outArray.release());
                            
                        } else {
                            EchoWarning(attrName.c_str(), "Failed export attribute!");
                        }
                        
                        
                        
                    } else {
                        EchoWarning(attrName.c_str(), "Export attribute is not exist!");
                    }
                    
                    
                }
                
                
 #pragma mark export point attribute               
                
                
                
                
                for (uint i=0; i<pointAttrNameArray.length(); i++) {
                    std::string attrName = pointAttrNameArray[i].asChar();
                    
                    auto cIt=outDCrvs->beginCurves();
                    if ((*cIt)->isExistAttr(attrName)) {
                        
                        size_t pointOffset=0;
                        std::string typeStr = (*cIt)->getAttrTypeName(attrName);
                        size_t schemaLayoutIndex = 0;
                        bool validSchema = true;
                        for (auto schemaIt = outDCrvs->beginCurves();
                             schemaIt != outDCrvs->endCurves();
                             ++schemaIt, ++schemaLayoutIndex) {
                            const DPAttributeData::ConstPtr attribute =
                                (*schemaIt)->getAttr(attrName);
                            if (!attribute || (*schemaIt)->getAttrTypeName(attrName) != typeStr ||
                                attribute->attrSize() !=
                                    curveLayouts[schemaLayoutIndex].sourcePointCount()) {
                                EchoWarning(attrName.c_str(),
                                            "Point attribute type/count is inconsistent across curves.");
                                validSchema = false;
                                break;
                            }
                        }
                        if (!validSchema)
                            continue;
                        
                        if (typeStr == "int") {
                            
                            dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                                parameterLayout.elementCount,
                                parameterLayout.keyCount, AI_TYPE_INT));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold integer attribute");
								return MS::kFailure;
							}
                            
                            size_t attributeLayoutIndex = 0;
                            bool validAttribute = true;
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves();
                                 ++cIt, ++attributeLayoutIndex) {
                                if ((*cIt)->isExistAttr(attrName)) {
                                    std::vector<int> tmp = (*cIt)->attrListValue<int>(attrName);
									const dommy::arnold::CurveLayout& layout =
										curveLayouts[attributeLayoutIndex];
									if (tmp.size() != layout.sourcePointCount()) {
										EchoWarning(attrName, "Point attribute count does not match the curve CV count.");
										validAttribute = false;
										break;
									}
									for (size_t valueIndex = 0; valueIndex < layout.varyingCount();
									     ++valueIndex) {
										const size_t sourceIndex = layout.varyingSourceIndex(valueIndex);
										dommy::arnold::requireArrayWrite(AiArraySetInt(
											outArray.get(), pointOffset++, tmp[sourceIndex]));
                                    }
                                } else {
                                    EchoWarning(attrName, "Not exist point attribute!");
									validAttribute = false;
									break;
                                }
                            }

							if (validAttribute) {
								dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "varying INT");
								AiNodeSetArray(
									nodeCurves, attrName.c_str(), outArray.release());
							}
                            
                        } else if (typeStr == "float" || typeStr == "double") {
                            
                            dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                                parameterLayout.elementCount,
                                parameterLayout.keyCount, AI_TYPE_FLOAT));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold float attribute");
								return MS::kFailure;
							}
                            
                            size_t attributeLayoutIndex = 0;
                            bool validAttribute = true;
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves();
                                 ++cIt, ++attributeLayoutIndex) {
                                if ((*cIt)->isExistAttr(attrName)) {
                                    
                                    std::vector<float> tmp;
                                    if (typeStr == "float") {
                                        tmp = (*cIt)->attrListValue<float>(attrName);
                                    } else if (typeStr == "double") {
                                        std::vector<double> vals = (*cIt)->attrListValue<double>(attrName);
                                        tmp = dommy::arnold::checkedFloatValues(vals);
                                    }
									const dommy::arnold::CurveLayout& layout =
										curveLayouts[attributeLayoutIndex];
									if (tmp.size() != layout.sourcePointCount()) {
										EchoWarning(attrName, "Point attribute count does not match the curve CV count.");
										validAttribute = false;
										break;
									}
									for (size_t valueIndex = 0; valueIndex < layout.varyingCount();
									     ++valueIndex) {
										const size_t sourceIndex = layout.varyingSourceIndex(valueIndex);
										dommy::arnold::requireArrayWrite(AiArraySetFlt(
											outArray.get(), pointOffset++, dommy::arnold::checkedFloat(tmp[sourceIndex])));
                                    }
                                } else {
                                    EchoWarning(attrName, "Not exist point attribute!");
									validAttribute = false;
									break;
                                }
                            }

							if (validAttribute) {
								dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "varying FLOAT");
								AiNodeSetArray(
									nodeCurves, attrName.c_str(), outArray.release());
							}
                            
                        } else if (typeStr == "vec2i" || typeStr == "vec2f" || typeStr == "vec2d") {
                            
                            dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                                parameterLayout.elementCount,
                                parameterLayout.keyCount, AI_TYPE_VECTOR2));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector2 attribute");
								return MS::kFailure;
							}
                            
                            size_t attributeLayoutIndex = 0;
                            bool validAttribute = true;
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves();
                                 ++cIt, ++attributeLayoutIndex) {
                                if ((*cIt)->isExistAttr(attrName)) {
                                    
                                    std::vector<Imath::V2f> tmp;
                                    if(typeStr == "vec2i") {
                                        std::vector<Imath::V2i> vals = (*cIt)->attrListValue<Imath::V2i>(attrName);
                                        tmp = dommy::arnold::checkedFloatVectors<Imath::V2f>(vals);
                                    } else if (typeStr == "vec2f") {
                                        tmp = (*cIt)->attrListValue<Imath::V2f>(attrName);
                                    } else if (typeStr == "vec2d") {
                                        std::vector<Imath::V2d> vals = (*cIt)->attrListValue<Imath::V2d>(attrName);
                                        tmp = dommy::arnold::checkedFloatVectors<Imath::V2f>(vals);
                                    }
									const dommy::arnold::CurveLayout& layout =
										curveLayouts[attributeLayoutIndex];
									if (tmp.size() != layout.sourcePointCount()) {
										EchoWarning(attrName, "Point attribute count does not match the curve CV count.");
										validAttribute = false;
										break;
									}
									for (size_t valueIndex = 0; valueIndex < layout.varyingCount();
									     ++valueIndex) {
										const size_t sourceIndex = layout.varyingSourceIndex(valueIndex);
                                        AtVector2 pt2;
										pt2.x = dommy::arnold::checkedFloat(tmp[sourceIndex][0]);
										pt2.y = dommy::arnold::checkedFloat(tmp[sourceIndex][1]);
                                        dommy::arnold::requireArrayWrite(AiArraySetVec2(
											outArray.get(), pointOffset++, pt2));
                                    }
                                } else {
                                    EchoWarning(attrName, "Not exist point attribute!");
									validAttribute = false;
									break;
                                }
                            }

							if (validAttribute) {
								dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "varying POINT2");
								AiNodeSetArray(
									nodeCurves, attrName.c_str(), outArray.release());
							}
                            
                        } else if (typeStr == "vec3i" || typeStr == "vec3f" || typeStr == "vec3d") {
                            
                            dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
                                parameterLayout.elementCount,
                                parameterLayout.keyCount, AI_TYPE_VECTOR));
							if (!outArray) {
								EchoWarning(attrName.c_str(),
								            "Failed to allocate an Arnold vector attribute");
								return MS::kFailure;
							}
                            
                            size_t attributeLayoutIndex = 0;
                            bool validAttribute = true;
                            for (auto cIt=outDCrvs->beginCurves(); cIt!=outDCrvs->endCurves();
                                 ++cIt, ++attributeLayoutIndex) {
                                if ((*cIt)->isExistAttr(attrName)) {
                                    std::vector<Imath::V3f> tmp;
                                    if(typeStr == "vec3i") {
                                        std::vector<Imath::V3i> vals = (*cIt)->attrListValue<Imath::V3i>(attrName);
                                        tmp = dommy::arnold::checkedFloatVectors<Imath::V3f>(vals);
                                    } else if (typeStr == "vec3f") {
                                        tmp = (*cIt)->attrListValue<Imath::V3f>(attrName);
                                    } else if (typeStr == "vec3d") {
                                        std::vector<Imath::V3d> vals = (*cIt)->attrListValue<Imath::V3d>(attrName);
                                        tmp = dommy::arnold::checkedFloatVectors<Imath::V3f>(vals);
                                    }
									const dommy::arnold::CurveLayout& layout =
										curveLayouts[attributeLayoutIndex];
									if (tmp.size() != layout.sourcePointCount()) {
										EchoWarning(attrName, "Point attribute count does not match the curve CV count.");
										validAttribute = false;
										break;
									}
									for (size_t valueIndex = 0; valueIndex < layout.varyingCount();
									     ++valueIndex) {
										const size_t sourceIndex = layout.varyingSourceIndex(valueIndex);
                                        AtVector vec;
										vec.x = dommy::arnold::checkedFloat(tmp[sourceIndex][0]);
										vec.y = dommy::arnold::checkedFloat(tmp[sourceIndex][1]);
										vec.z = dommy::arnold::checkedFloat(tmp[sourceIndex][2]);
                                        dommy::arnold::requireArrayWrite(AiArraySetVec(
											outArray.get(), pointOffset++, vec));
                                    }
                                } else {
                                    EchoWarning(attrName, "Not exist point attribute!");
									validAttribute = false;
									break;
                                }
                            }

							if (validAttribute) {
								dommy::arnold::requireUserParameter(nodeCurves, attrName.c_str(), "varying VECTOR");
								AiNodeSetArray(
									nodeCurves, attrName.c_str(), outArray.release());
							}
                            
                        } else {
                            EchoWarning(attrName, "Not Support point attribute type!");
                            continue;
                        }
                    } else {
                        EchoWarning(attrName, "Not Exist point attribute!");
                        continue;
                    }

                }
				
                
                
                
#pragma mark export
		                //write ass file
#if DOMMY_WITH_DG_FILE_WRITERS
		                DOMMY_RETURN_IF_FAILURE(dommy::arnold::writeAssFile(
								filename, isBinary, "DWriteDCurveToAi"));
						exportedFile = true;
#else
						(void)filename;
						(void)isBinary;
						EchoWarning(
							name().asChar(),
							"ASS export is disabled by this build profile");
						return MS::kFailure;
#endif
						}
				}
				
				if (exportedFile) {
					EchoInfo(name().asChar(), "Finished writing the ASS file.");
				}
            
			// Output
			MDataHandle output = data.outputValue(outDC, &stat);
			if (!stat) return stat;
			stat = dommy::maya::publishTypedPluginDataObject<DCurveData>(
				output, pluginData.object(), DCurveData::id);
			if (!stat) return stat;
			
			return data.setClean(plug);
            
        }
        
    }
    
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DWriteDCurveToAi")




MStatus DWriteDCurveToAi::initialize()
try {
    MStatus stat;
    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    
    MFnStringData fnStringData;
    MObject defaultStringData = fnStringData.create("dcurve.rib", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    
    
    
    
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));
    
    
    MObject defaultFileStringData = fnStringData.create("exportCurve.ass", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aFilePath = tAttr.create("FilePath", "file", MFnData::kString, defaultFileStringData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFilePath));
    
    MFnEnumAttribute eAttr;
	    aExportMode = eAttr.create("ExportMode", "expm", 2, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Overwrite", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Overwrite when no file", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("No export", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportMode));
    
    aExportLog = nAttr.create( "ExportLog", "exl", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportLog));
    
    
    aIsBinary = nAttr.create( "IsBinary", "isbn", MFnNumericData::kBoolean , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsBinary));
	
	
	
	const MObject aOutNodeNameDefaultData = fnStringData.create("dcurveObj", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOutNodeName = tAttr.create("OutNodeName", "ondnm", MFnData::kString, aOutNodeNameDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOutNodeName));
	
	MFnEnumAttribute modeEnumFn;
	aRenderMode = modeEnumFn.create("RenderMode", "renmd", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("ribbon", 0));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("thick", 1));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.addField("oriented", 2));
	DOMMY_RETURN_IF_FAILURE(modeEnumFn.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRenderMode));
	
	
    basisMode = eAttr.create("basisMode", "bmode", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
	    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Linear", 0));
	    DOMMY_RETURN_IF_FAILURE(eAttr.addField("B-Spline", 1));
	    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Catmull-Rom", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(basisMode));
    
    isAddEdgeCV = nAttr.create("isAddEdgeCV", "iacv", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(isAddEdgeCV));
	    aCurveWidthType = eAttr.create("CurveWidthType", "crvwty", 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Constant", 0));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Curve Attribute", 1));
    DOMMY_RETURN_IF_FAILURE(eAttr.addField("Point Attribute", 2));
    DOMMY_RETURN_IF_FAILURE(eAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveWidthType));
    
    
    aConstantWidth = nAttr.create( "ConstantWidth", "cw", MFnNumericData::kDouble , 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConstantWidth));
    
    
    const MObject aWidthAttributeNameDefaultData = fnStringData.create("width", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aWidthAttributeName = tAttr.create("WidthAttributeName", "wattr", MFnData::kString, aWidthAttributeNameDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWidthAttributeName));
    
	aMinPixelWidth= nAttr.create( "MinPixelWidth", "mpxw", MFnNumericData::kFloat , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.001));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aMinPixelWidth));
	
	
	MFnEnumAttribute oriEnumFn;
	aOrientType = oriEnumFn.create("OrientType", "ority", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("None", 0));
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("Curve Attribute", 1));
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.addField("Point Attribute", 2));
	DOMMY_RETURN_IF_FAILURE(oriEnumFn.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOrientType));
	
	const MObject aOrientAttributeNameDefaultData = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOrientAttributeName = tAttr.create("OrientAttributeName", "orattr", MFnData::kString, aOrientAttributeNameDefaultData, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOrientAttributeName));
	
	
    // Setup the input attributes
    
    
    const MObject aExportCurveAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportCurveAttribute = tAttr.create("ExportCurveAttribute", "excrvattr", MFnData::kString, aExportCurveAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportCurveAttribute));
    
    const MObject aExportPointAttributeDefaultData = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExportPointAttribute = tAttr.create("ExportPointAttribute", "exptattr", MFnData::kString, aExportPointAttributeDefaultData, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExportPointAttribute));
    
    
    // Setup the output attributes
    
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));
    
    
    // Set the attribute dependencies
    DOMMY_RETURN_IF_FAILURE(attributeAffects(inDC, outDC));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aFilePath, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportMode, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportLog, outDC));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aIsBinary, outDC));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aOutNodeName, outDC));
    
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aRenderMode, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(basisMode, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(isAddEdgeCV, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aCurveWidthType, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aConstantWidth, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aWidthAttributeName, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aMinPixelWidth, outDC));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aOrientType, outDC));
	DOMMY_RETURN_IF_FAILURE(attributeAffects(aOrientAttributeName, outDC));
	
	
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportCurveAttribute, outDC));
    DOMMY_RETURN_IF_FAILURE(attributeAffects(aExportPointAttribute, outDC));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DWriteDCurveToAi", "initialize", std::current_exception());
}
