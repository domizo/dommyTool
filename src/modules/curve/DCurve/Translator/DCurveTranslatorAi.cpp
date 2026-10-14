//
//  DCurveTranslatorAi.cpp
//  dommyTool
//
//  Created by yuzo on 3/23/17.
//
//

#include "DCurveTranslatorAi.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DMayaNameList.h"



#include <ai_msg.h>
#include <ai_nodes.h>

#include "DMayaHeaders.h"
#include "DCurveDataHeaders.h"

#include "DNodeOperation.h"
#include "DType.h"
#include "DCurveShape.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldMotionKey.h"
#include "DCurveArnoldLayout.h"
#include "common/utility/DIndexTransform.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "integrations/arnold/DArnoldEntryPointGuard.h"
#include "integrations/arnold/DArnoldMayaPlug.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "common/utility/DPointTransform.h"

#include <cmath>

namespace {

bool makeRenderableCurvePlan(const DCurveData& curves, short basisMode,
                             bool addEdgeCVs, std::vector<std::size_t>& indices,
                             std::vector<dommy::arnold::CurveLayout>& layouts,
                             std::size_t& controlVertices, std::size_t& varyingValues) {
    const auto* source = curves.getCurveListConst();
    if (!source || !dommy::attribute::validArrayConversionBudget(
            source->size(), sizeof(std::size_t) + sizeof(dommy::arnold::CurveLayout))) return false;
    std::vector<std::size_t> selected;
    std::vector<dommy::arnold::CurveLayout> planned;
    selected.reserve(source->size());
    planned.reserve(source->size());
    std::size_t cvs = 0U, varying = 0U;
    for (std::size_t index = 0U; index < source->size(); ++index) {
        if (!(*source)[index]) return false;
        const std::size_t count = (*source)[index]->numberOfPoints();
        const auto layout = dommy::arnold::CurveLayout::create(count, basisMode != 0, addEdgeCVs);
        if (!layout.valid()) {
            // Retain the archived unpadded-cubic short-curve skip policy.
            if (basisMode != 0 && !addEdgeCVs && count < 4U) continue;
            return false;
        }
        if (!dommy::arnold::checkedAdd(layout.controlVertexCount(), cvs) ||
            !dommy::arnold::checkedAdd(layout.varyingCount(), varying)) return false;
        selected.push_back(index);
        planned.push_back(layout);
    }
    indices.swap(selected);
    layouts.swap(planned);
    controlVertices = cvs;
    varyingValues = varying;
    return true;
}

bool hasRenderableCurveTopology(const DCurveData& curves) {
    for (const auto& curve : *curves.getCurveListConst()) {
        if (!curve || !curve->hasValidCurveTopology()) {
            return false;
        }
    }
    return curves.numberOfCurves() != 0;
}

bool hasRenderablePrimitiveAttribute(const DCurveData& curves,
                                     const std::string& attributeName) {
    const DPAttributeData::ConstPtr attribute = curves.getAttr(attributeName);
    return attribute && attribute->attrSize() == curves.numberOfCurves();
}

bool hasRenderablePointAttribute(const DCurveData& curves,
                                 const std::string& attributeName) {
    bool hasType = false;
    DP_ATTR_DATA_TYPE expectedType = DP_ATTR_BOOL;
    for (const auto& curve : *curves.getCurveListConst()) {
        if (!curve) {
            return false;
        }

        const DPAttributeData::ConstPtr attribute = curve->getAttr(attributeName);
        if (!attribute || attribute->attrSize() != curve->numberOfPoints()) {
            return false;
        }
        if (hasType && attribute->getAttrType() != expectedType) {
            return false;
        }
        expectedType = attribute->getAttrType();
        hasType = true;
    }
    return hasType;
}

} // namespace


void DCurveTranslatorAi::Export(AtNode* node) try
{
//    cout << "Export" << endl;
	AiMsgDebug("[dommyTool] Exporting %s", GetMayaNodeName().asChar());
	Update(node);
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DCurveTranslatorAi::Export", node)

void DCurveTranslatorAi::NodeChanged(MObject& node, MPlug& plug) try
{
	MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status) {
        AiMsgError("[DCurveTranslatorAi] Unable to inspect the changed Maya node.");
        return;
    }
	
	    const MString pName = plug.partialName(
        false, false, false, false, false, true, &status);
    if (!status) {
        AiMsgError("[DCurveTranslatorAi] Unable to read the changed Maya plug name.");
        return;
    }
    if(pName == "inDC" ||
       pName == "IsRender" ||
       pName == "aiOpaque" ||
       pName == "aiSelfShadows" ||
       pName == "aiMatte" ||
       pName == "ConstantWidth" ||
       pName == "IsMultiEndKnot" ||
       pName == "CurveWidthType" ||
       pName == "WidthAttributeName" ||
       pName == "RenderMode" ||
       pName == "BasisMode" ||
       pName == "MinPixelWidth" ||
       pName == "OrientType" ||
       pName == "OrientAttributeName" ||
       pName == "ExportCurveAttribute" ||
       pName == "ExportPointAttribute" ||
	   pName == "CurveShader" ) {
        
	        SetUpdateMode(CNodeTranslator::AI_RECREATE_TRANSLATOR);
        
        RequestUpdate();
    }
	}

DOMMY_ARNOLD_TRANSLATOR_CATCH("DCurveTranslatorAi::NodeChanged", nullptr)


void DCurveTranslatorAi::Update(AtNode* node) try
{
//    cout << "Update" << endl;
//    unsigned int motionstep = GetMotionStep();
    unsigned int numMotionStep = GetNumMotionSteps();
	dommy::arnold::PlugReader plugs(
		[this](const char* name) -> MPlug { return FindMayaPlug(name); },
		"DCurveTranslatorAi");
    //    motionstep++;
//    cout << "motionstep:" << motionstep << endl;
//    cout << "numMotionStep:" << numMotionStep << endl;
	// Export the transform matrix
//	ExportMatrix(node, 0);
    bool isRender = false;
	if (!plugs.read("IsRender", isRender)) {
		return;
	}
    
    static const AtString visParam("visibility");
    

	
    MPlug dcPlug;
    bool inputConnected = false;
    if (isRender) {
        dcPlug = FindMayaPlug("inDC");
        if (!plugs.connection(dcPlug, "inDC", inputConnected)) return;
    }
    ExportMatrix(node);
    AiNodeSetByte(node, visParam, isRender);
	if (isRender && inputConnected) {
		
		dommy::arnold::UpdateVisibilityGuard updateVisibility(node);
		dommy::maya::ScopedPlugDataHandle dCurveHndOwner;
		MDataHandle& dCurveHnd = dCurveHndOwner.handle();
		if (dCurveHndOwner.read(dcPlug) != MS::kSuccess) {
			AiMsgError("[dommyTool] Unable to read Arnold DCurve input.");
			return;
		}
			const DCurveData* const dCurves(getInputDCfromHandle(dCurveHnd));
			if (!dCurves) {
			    AiMsgError("[dommyTool] Arnold DCurve input has invalid type or topology.");
			    return;
			}
        
        
			if (dCurves && dCurves->numberOfCurves()) {
				if (!hasRenderableCurveTopology(*dCurves)) {
					MGlobal::displayError("Arnold export rejected invalid or empty DCurve topology.");
					AiNodeSetByte(node, visParam, 0);
					return;
				}
            
			
			
			short basisModeVal = 0;
			bool isAddEdgeCVVal = false;
			double constantWidth = 0.0;
			short curveWidthType = 0;
			short modeType = 0;
			float minPixelWidth = 0.0f;
			std::string widthAttributeName;
			short orientType = 0;
			std::string orientAttrName;
			MString exportCurveAttributeNames;
			MString exportPointAttributeNames;
			if (!plugs.read("BasisMode", basisModeVal) ||
				!plugs.read("IsMultiEndKnot", isAddEdgeCVVal) ||
				!plugs.read("ConstantWidth", constantWidth) ||
				!plugs.read("CurveWidthType", curveWidthType) ||
				!plugs.read("RenderMode", modeType) ||
				!plugs.read("MinPixelWidth", minPixelWidth) ||
				!plugs.readString("WidthAttributeName", widthAttributeName) ||
				!plugs.read("OrientType", orientType) ||
				!plugs.readString("OrientAttributeName", orientAttrName) ||
				!plugs.read(
					"ExportCurveAttribute", exportCurveAttributeNames) ||
				!plugs.read(
					"ExportPointAttribute", exportPointAttributeNames) ||
				basisModeVal < 0 || basisModeVal > 2 ||
				curveWidthType < 0 || curveWidthType > 1 ||
				modeType < 0 || modeType > 2 ||
				orientType < 0 || orientType > 2 ||
				!std::isfinite(constantWidth) || constantWidth < 0.0 ||
				!std::isfinite(minPixelWidth) || minPixelWidth < 0.0f) {
				AiMsgError("[dommyTool] DCurve render parameters are invalid.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			
			






			
			
            const bool isCubicMode = basisModeVal != 0 && isAddEdgeCVVal;
            const bool isParamCubicMode = basisModeVal != 0 && !isAddEdgeCVVal;
            const int basisAdjustCount = isCubicMode ? 4 : 0;
            const auto* sourceCurves = dCurves->getCurveListConst();
            std::vector<std::size_t> renderableCurveIndices;
            std::vector<dommy::arnold::CurveLayout> renderableCurveLayouts;
            std::size_t totalCVCount = 0U, totalParamCount = 0U;
            if (!makeRenderableCurvePlan(*dCurves, basisModeVal, isAddEdgeCVVal,
                    renderableCurveIndices, renderableCurveLayouts, totalCVCount, totalParamCount)) {
                AiMsgError("Unable to plan Arnold curve source/output correspondence.");
                return;
            }
            const std::size_t totalCurveCount = renderableCurveIndices.size();
            dommy::arnold::ArrayLayout pointLayout;
			dommy::arnold::ArrayLayout cvLayout;
			dommy::arnold::ArrayLayout curveLayout;
			dommy::arnold::ArrayLayout parameterLayout;
			if (totalCurveCount == 0U ||
				!dommy::arnold::checkedArrayLayout(
					totalCVCount, numMotionStep, pointLayout) ||
				!dommy::arnold::checkedArrayLayout(
					totalCVCount, 1U, cvLayout) ||
				!dommy::arnold::checkedArrayLayout(
					totalCurveCount, 1U, curveLayout) ||
				!dommy::arnold::checkedArrayLayout(
					totalParamCount, 1U, parameterLayout)) {
				AiMsgError("Arnold curve arrays exceed the allocation budget.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			dommy::maya::NameListBudget nameListBudget;
			MStringArray curveAttrNameArray;
			MStringArray pointAttrNameArray;
			if (dommy::maya::splitCheckedNameList(
			        exportCurveAttributeNames, totalCurveCount, nameListBudget, curveAttrNameArray) != MS::kSuccess) {
				AiMsgError("[DCurveTranslatorAi] Unable to parse the curve attribute list.");
				return;
			}
			if (dommy::maya::splitCheckedNameList(
			        exportPointAttributeNames, totalCVCount, nameListBudget, pointAttrNameArray) != MS::kSuccess) {
				AiMsgError("[DCurveTranslatorAi] Unable to parse the point attribute list.");
				return;
			}
//			if(VERBOSE) cout << "totalCVCount:" << totalCVCount << endl;
//			if(VERBOSE) cout << "totalParamCount:" << totalParamCount << endl;
			
			







			

            
            
            
            
            
#pragma mark set curve mode
            
            static const AtString modeTypeParam("mode");
            
			switch (modeType) {
				case 0:
                {
                    AtString mParam("ribbon");
					AiNodeSetStr( node, modeTypeParam, mParam);
                }
					break;
				case 1:
                {
                    AtString mParam("thick");
					AiNodeSetStr( node, modeTypeParam, mParam);
                }
					break;
				case 2:
                {
                    AtString mParam("oriented");
					AiNodeSetStr( node, modeTypeParam, mParam);
                }
				default:
					break;
			}
            
            if(modeType==1) {
                static const AtString minPixelWidthParam("min_pixel_width");
                AiNodeSetFlt(node, minPixelWidthParam, minPixelWidth);
            }
			
			
            
            
            
#pragma mark set basis
            static const AtString basisTypeParam("basis");
            
            switch (basisModeVal) {
                case 0:
                {
                    AtString cParam("linear");
                    AiNodeSetStr( node, basisTypeParam, cParam );
                }
                    break;
                case 1:
                {
                    AtString cParam("b-spline");
                    AiNodeSetStr( node, basisTypeParam, cParam );
                }
                    break;
                case 2:
                {
                    AtString cParam("catmull-rom");
                    AiNodeSetStr( node, basisTypeParam, cParam );
                }
                    break;
                default:
                {
                    AtString cParam("linear");
                    AiNodeSetStr( node, basisTypeParam, cParam );
                }
                    break;
            }
			
			
#pragma mark make point
			dommy::arnold::ArrayOwner pointarray(AiArrayAllocate(
				pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_VECTOR));
			dommy::arnold::ArrayOwner curvePointCountArray(AiArrayAllocate(
				curveLayout.elementCount, curveLayout.keyCount, AI_TYPE_UINT));
			if (!pointarray || !curvePointCountArray) {
				AiMsgError("Unable to allocate the Arnold curve topology arrays.");
				return;
			}
			
			size_t pointOffset=0;
			size_t curveOffset=0;
			
			for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
				auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
				
                if(isParamCubicMode && (*cIt)->numberOfPoints()<4)
                {
                    continue;
                }
                
                
                DPointListPtr pts;
                MMatrix mat;
                if (!(*cIt)->isIdentity()) {
                    pts.reset(new DPointList((*cIt)->getPointsInWorld()));
                } else {
                    pts.reset(new DPointList(
                        *(*cIt)->getPositionPtrConst()));
                }
				
				if(isCubicMode) {
					AtVector aVec;
					aVec.x = pts->front()[0];
					aVec.y = pts->front()[1];
					aVec.z = pts->front()[2];
					dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
					pointOffset++;
					
					dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
					pointOffset++;
				}
				
//				for (size_t p = 0;p<pts->size();++p) {
                for (auto &pIt: *pts) {
					AtVector aVec;
//					aVec.x = (*pts)[p][0];
//					aVec.y = (*pts)[p][1];
//					aVec.z = (*pts)[p][2];
                    
                    aVec.x = pIt[0];
                    aVec.y = pIt[1];
                    aVec.z = pIt[2];
                    
					dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
					pointOffset++;
				}
				
				if(isCubicMode) {
					AtVector aVec;
					aVec.x = pts->back()[0];
					aVec.y = pts->back()[1];
					aVec.z = pts->back()[2];
					dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
					pointOffset++;
					
					dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
					pointOffset++;
				}
				
				dommy::arnold::requireArrayWrite(AiArraySetUInt(
					curvePointCountArray.get(), curveOffset,
					(*cIt)->numberOfPoints() + basisAdjustCount));
				curveOffset++;
			}
			static const AtString ptsParam("points");
            static const AtString numPtParam("num_points");
			AiNodeSetArray(node, ptsParam, pointarray.release());
			AiNodeSetArray(
				node, numPtParam, curvePointCountArray.release());
			
			
			
			
            
            
//#pragma mark set transformMatrix
            
            
            
            
#pragma mark set curve width
			
			static const AtString radParam("radius");
            
            
			dommy::arnold::ArrayOwner radiusArray;
			
			if (curveWidthType==1) {//curve attribute
				
				radiusArray.reset(AiArrayAllocate(
					parameterLayout.elementCount, parameterLayout.keyCount,
					AI_TYPE_FLOAT));
				if (!radiusArray) {
					AiMsgError("Unable to allocate the Arnold curve radius array.");
					return;
				}
				
				if (dCurves->isExistAttr(widthAttributeName) &&
					hasRenderablePrimitiveAttribute(*dCurves, widthAttributeName)) {
					DP_ATTR_DATA_TYPE type = dCurves->getAttr(widthAttributeName)->getAttrType();
					
					bool isNotType = false;
					std::vector<float> wList;
					if(type==DP_ATTR_FLOAT) {
						wList = dCurves->attrListValue<float>(widthAttributeName);
					} else if (type==DP_ATTR_DOUBLE) {
						std::vector<double> tmp = dCurves->attrListValue<double>(widthAttributeName);
						wList = dommy::arnold::checkedFloatValues(tmp);
					} else {
						AiMsgWarning("Curve width attribute is not support type! supported type is float or double");
						isNotType = true;
					}
					
					if (!isNotType) {
						size_t pointOffset=0;
						for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
							auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
							
							if(isParamCubicMode && (*cIt)->numberOfPoints()<4)
							{
								AiMsgWarning("Curve cv count needs more than 4.");
								continue;
							}
							
							if (isCubicMode) {
								dommy::arnold::requireArrayWrite(AiArraySetFlt(
									radiusArray.get(), pointOffset++, wList[renderableCurveIndices[renderCurveIndex]]));
							}
							
							size_t pCount=(*cIt)->numberOfPoints();
							if (isParamCubicMode) {
								for (size_t c=1; c<(pCount-1); ++c) {
									dommy::arnold::requireArrayWrite(AiArraySetFlt(
										radiusArray.get(), pointOffset++, wList[renderableCurveIndices[renderCurveIndex]]));
								}
							} else {
								for (size_t c=0; c<pCount; ++c) {
									dommy::arnold::requireArrayWrite(AiArraySetFlt(
										radiusArray.get(), pointOffset++, wList[renderableCurveIndices[renderCurveIndex]]));
								}
							}
							
							
							if (isCubicMode) {
								dommy::arnold::requireArrayWrite(AiArraySetFlt(
									radiusArray.get(), pointOffset++, wList[renderableCurveIndices[renderCurveIndex]]));
							}
						}
						
						AiNodeSetArray(node, radParam, radiusArray.release());
					}
					
				} else if (dCurves->isExistPointAttr(widthAttributeName) &&
						   hasRenderablePointAttribute(*dCurves, widthAttributeName)) {
					//point attribute
					radiusArray.reset(AiArrayAllocate(
						parameterLayout.elementCount, parameterLayout.keyCount,
						AI_TYPE_FLOAT));
					if (!radiusArray) {
						AiMsgError("Unable to allocate the Arnold curve radius array.");
						return;
					}
					
					bool isNotType = false;
					size_t pointOffset=0;
					
					for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
						auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
						
						if(isParamCubicMode && (*cIt)->numberOfPoints()<4)
						{
							AiMsgWarning("Curve cv count needs more than 4.");
							continue;
						}
						
						std::vector<float> srcList;
						if((*cIt)->isExistAttr(widthAttributeName)) {
							
							DP_ATTR_DATA_TYPE type = (*cIt)->getAttr(widthAttributeName)->getAttrType();
							
							if(type==DP_ATTR_FLOAT) {
								srcList = (*cIt)->attrListValue<float>(widthAttributeName);
							} else if (type==DP_ATTR_DOUBLE) {
								std::vector<double> wList = (*cIt)->attrListValue<double>(widthAttributeName);
								srcList = dommy::arnold::checkedFloatValues(wList);
							} else {
								AiMsgWarning("Not support point attribute type!");
								isNotType = true;
							}
						} else {
							AiMsgWarning("Not Exist point attribute!");
							isNotType = true;
						}
						
						if (isNotType)//when not support type is finish routine.
							break;
						
						
						if (isCubicMode) {
							dommy::arnold::requireArrayWrite(AiArraySetFlt(
								radiusArray.get(), pointOffset++, srcList.front()));
						}
						
						size_t pCount=(*cIt)->numberOfPoints();
						if (isParamCubicMode) {
							for (size_t c=1; c<(pCount-1); ++c) {
								dommy::arnold::requireArrayWrite(AiArraySetFlt(
									radiusArray.get(), pointOffset++, srcList[c]));
							}
						} else {
							for (size_t c=0; c<pCount; ++c) {
								dommy::arnold::requireArrayWrite(AiArraySetFlt(
									radiusArray.get(), pointOffset++, srcList[c]));
							}
						}
						
						if (isCubicMode) {
							dommy::arnold::requireArrayWrite(AiArraySetFlt(
								radiusArray.get(), pointOffset++, srcList.back()));
						}
						
						
					}
					
					if (!isNotType) {
						AiNodeSetArray(node, radParam, radiusArray.release());
					}
					
				} else {
					AiMsgWarning("DCurve width attribute is not exist!");
				}
				
			} else {
				radiusArray.reset(AiArrayAllocate(1, 1, AI_TYPE_FLOAT));
				if (!radiusArray) {
					AiMsgError("Unable to allocate the Arnold curve radius array.");
					return;
				}
				dommy::arnold::requireArrayWrite(AiArraySetFlt(radiusArray.get(), 0, dommy::arnold::checkedFloat(constantWidth)));
				AiNodeSetArray(node, radParam, radiusArray.release());
			}
			
			
			
			
#pragma mark orient attribute
			
            
            
			if (modeType==2 && orientType==1) {//curve attribute
				
				
				if (dCurves->isExistAttr(orientAttrName) &&
					hasRenderablePrimitiveAttribute(*dCurves, orientAttrName)) {
					DP_ATTR_DATA_TYPE type = dCurves->getAttr(orientAttrName)->getAttrType();
					
                    dommy::arnold::ArrayOwner orientationArray(AiArrayAllocate(
                        cvLayout.elementCount, cvLayout.keyCount,
                        AI_TYPE_VECTOR));
					if (!orientationArray) {
						AiMsgError(
							"Unable to allocate the Arnold curve orientation array.");
						return;
					}
                    
					bool isNotType = false;
					DVec3DataList oriList;
					if(type==DP_ATTR_VEC3F) {
                        oriList = DVec3DataList(dCurves->attrListValue<Imath::V3f>(orientAttrName));
//                        oriList = dCurves->attrListValue<Imath::V3f>(orientAttrName);
					} else if (type==DP_ATTR_VEC3D) {
						std::vector<Imath::V3d> tmp = dCurves->attrListValue<Imath::V3d>(orientAttrName);
                        oriList = dommy::arnold::checkedFloatVectors<Imath::V3f>(tmp);
					} else {
						AiMsgWarning("Curve width attribute is not support type! supported type is vec3f or vec3d");
						isNotType = true;
					}
                    
                    
                    
					
					if (!isNotType) {
						size_t pointOffset=0;
						for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
							auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
							
							if(isParamCubicMode && (*cIt)->numberOfPoints()<4)
							{
								AiMsgWarning("Curve cv count needs more than 4.");
								continue;
							}
							
							AtVector aVec;
							aVec.x = oriList[renderableCurveIndices[renderCurveIndex]][0];
							aVec.y = oriList[renderableCurveIndices[renderCurveIndex]][1];
							aVec.z = oriList[renderableCurveIndices[renderCurveIndex]][2];
							
							if (isCubicMode) {
								dommy::arnold::requireArrayWrite(AiArraySetVec(
									orientationArray.get(), pointOffset++, aVec));
								dommy::arnold::requireArrayWrite(AiArraySetVec(
									orientationArray.get(), pointOffset++, aVec));
							}
							
							size_t pCount=(*cIt)->numberOfPoints();
							for (size_t c=0; c<pCount; ++c) {
								dommy::arnold::requireArrayWrite(AiArraySetVec(
									orientationArray.get(), pointOffset++, aVec));
							}
							
							if (isCubicMode) {
								dommy::arnold::requireArrayWrite(AiArraySetVec(
									orientationArray.get(), pointOffset++, aVec));
								dommy::arnold::requireArrayWrite(AiArraySetVec(
									orientationArray.get(), pointOffset++, aVec));
							}
						}
						
						AiNodeSetArray(
							node, AtString("orientations"),
							orientationArray.release());
					}
					
				} else {
					AiMsgWarning("Curve orientation attribute is not exist!");
				}
				
			} else if (modeType==2 && orientType==2 ) {//point attribute
				
                if (dCurves->isExistPointAttr(orientAttrName) &&
                    hasRenderablePointAttribute(*dCurves, orientAttrName)) {
                    
                    dommy::arnold::ArrayOwner orientationArray(AiArrayAllocate(
                        cvLayout.elementCount, cvLayout.keyCount,
                        AI_TYPE_VECTOR));
					if (!orientationArray) {
						AiMsgError(
							"Unable to allocate the Arnold curve orientation array.");
						return;
					}
                    
                    bool isNotType = false;
                    size_t pointOffset=0;
                    
                    for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
                        auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
                        
                        if(isParamCubicMode && (*cIt)->numberOfPoints()<4)
                        {
                            AiMsgWarning("Curve cv count needs more than 4.");
                            continue;
                        }
                        
                        // Keep Double attributes wide until the direction
                        // transform is complete; only Arnold storage is Float.
                        std::vector<Imath::V3d> orientationValues;
                        
                        if((*cIt)->isExistAttr(orientAttrName)) {
                            DP_ATTR_DATA_TYPE type = (*cIt)->getAttr(orientAttrName)->getAttrType();
                            
                            
                            if(type==DP_ATTR_VEC3F) {
                                const std::vector<Imath::V3f>& values =
                                    (*cIt)->attrListValue<Imath::V3f>(orientAttrName);
                                orientationValues = std::vector<Imath::V3d>(
                                    values.begin(), values.end());
                            } else if (type==DP_ATTR_VEC3D) {
                                orientationValues =
                                    (*cIt)->attrListValue<Imath::V3d>(orientAttrName);
                            } else {
                                AiMsgWarning("Not support orient point attribute type");
                                isNotType = true;
                            }
                        } else {
                            AiMsgWarning("Not Exist orient point attribute!");
                            isNotType = true;
                        }
                        
                        if (isNotType)//when not support type is finish routine.
                            break;
                        
                        
                        if (!(*cIt)->isIdentity() && !orientationValues.empty()) {
                            const MMatrix matrix = (*cIt)->getTransformMatrix();
                            if (!dommy::numeric::isFiniteAffineTransform(matrix)) {
                                throw std::invalid_argument(
                                    "Arnold curve orientation transform is not finite affine.");
                            }
                            for (auto& value : orientationValues) {
                                std::array<double, 3> transformed;
                                if (!dommy::numeric::checkedTransformDirection(
                                        value, matrix, transformed)) {
                                    throw std::invalid_argument(
                                        "Arnold curve orientation transform overflows.");
                                }
                                value = Imath::V3d(
                                    transformed[0], transformed[1], transformed[2]);
                            }
                        }
                        const DVec3DataList srcList =
                            dommy::arnold::checkedFloatVectors<Imath::V3f>(orientationValues);
                        
                        
                        if (isCubicMode) {
                            AtVector aVec;
                            aVec.x = srcList.front()[0];
                            aVec.y = srcList.front()[1];
                            aVec.z = srcList.front()[2];
                            dommy::arnold::requireArrayWrite(AiArraySetVec(
								orientationArray.get(), pointOffset++, aVec));
                            dommy::arnold::requireArrayWrite(AiArraySetVec(
								orientationArray.get(), pointOffset++, aVec));
                        }
                        
                        size_t pCount=(*cIt)->numberOfPoints();
                        for (size_t c=0; c<pCount; ++c) {
                            AtVector aVec;
                            aVec.x = srcList[c][0];
                            aVec.y = srcList[c][1];
                            aVec.z = srcList[c][2];
                            dommy::arnold::requireArrayWrite(AiArraySetVec(
								orientationArray.get(), pointOffset++, aVec));
                        }
                        
                        if (isCubicMode) {
                            AtVector aVec;
                            aVec.x = srcList.back()[0];
                            aVec.y = srcList.back()[1];
                            aVec.z = srcList.back()[2];
                            dommy::arnold::requireArrayWrite(AiArraySetVec(
								orientationArray.get(), pointOffset++, aVec));
                            dommy::arnold::requireArrayWrite(AiArraySetVec(
								orientationArray.get(), pointOffset++, aVec));
                        }
                        
                        
                    }
                    
                    if (!isNotType) {
                        AiNodeSetArray(
							node, AtString("orientations"),
							orientationArray.release());
                    }
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
				
				
				if (dCurves->isExistAttr(attrName) &&
					hasRenderablePrimitiveAttribute(*dCurves, attrName)) {
					DP_ATTR_DATA_TYPE type = dCurves->getAttr(attrName)->getAttrType();
					
					if (type == DP_ATTR_INT) {
						std::vector<int32_t> tmp = dCurves->attrListValue<int32_t>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_INT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold integer attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							dommy::arnold::requireArrayWrite(AiArraySetInt(outArray.get(), i, tmp[i]));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform INT");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_FLOAT) {
						
						std::vector<float> tmp = dCurves->attrListValue<float>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_FLOAT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold float attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							dommy::arnold::requireArrayWrite(AiArraySetFlt(outArray.get(), i, tmp[i]));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform FLOAT");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
						
					} else if (type == DP_ATTR_DOUBLE) {
						
						std::vector<double> tmp = dCurves->attrListValue<double>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_FLOAT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold float attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							dommy::arnold::requireArrayWrite(AiArraySetFlt(
								outArray.get(), i, dommy::arnold::checkedFloat(tmp[i])));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform FLOAT");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
						
					} else if (type == DP_ATTR_VEC2I) {
						std::vector<Imath::V2i> tmp = dCurves->attrListValue<Imath::V2i>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							AtVector2 pt2;
							pt2.x = dommy::arnold::checkedFloat(tmp[i][0]);
							pt2.y = dommy::arnold::checkedFloat(tmp[i][1]);
							dommy::arnold::requireArrayWrite(AiArraySetVec2(outArray.get(), i, pt2));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform POINT2");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_VEC2F) {
						std::vector<Imath::V2f> tmp = dCurves->attrListValue<Imath::V2f>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							AtVector2 pt2;
							pt2.x = dommy::arnold::checkedFloat(tmp[i][0]);
							pt2.y = dommy::arnold::checkedFloat(tmp[i][1]);
							dommy::arnold::requireArrayWrite(AiArraySetVec2(outArray.get(), i, pt2));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform POINT2");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_VEC2D) {
						std::vector<Imath::V2d> tmp = dCurves->attrListValue<Imath::V2d>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							AtVector2 pt2;
							pt2.x = dommy::arnold::checkedFloat(tmp[i][0]);
							pt2.y = dommy::arnold::checkedFloat(tmp[i][1]);
							dommy::arnold::requireArrayWrite(AiArraySetVec2(outArray.get(), i, pt2));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform POINT2");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_VEC3I) {
						std::vector<Imath::V3i> tmp = dCurves->attrListValue<Imath::V3i>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							AtVector vec;
							vec.x = dommy::arnold::checkedFloat(tmp[i][0]);
							vec.y = dommy::arnold::checkedFloat(tmp[i][1]);
							vec.z = dommy::arnold::checkedFloat(tmp[i][2]);
							dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), i, vec));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform VECTOR");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_VEC3F) {
						std::vector<Imath::V3f> tmp = dCurves->attrListValue<Imath::V3f>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							AtVector vec;
							vec.x = dommy::arnold::checkedFloat(tmp[i][0]);
							vec.y = dommy::arnold::checkedFloat(tmp[i][1]);
							vec.z = dommy::arnold::checkedFloat(tmp[i][2]);
							dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), i, vec));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform VECTOR");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_VEC3D) {
						std::vector<Imath::V3d> tmp = dCurves->attrListValue<Imath::V3d>(attrName);
						if (!dommy::container::remapInPlace(tmp, renderableCurveIndices))
							throw std::invalid_argument("Invalid Arnold curve attribute selection.");
						
						dommy::arnold::ArrayOwner outArray(
							AiArrayAllocate(tmp.size(), 1, AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
						
						for (size_t i = 0; i < tmp.size(); i++) {
							AtVector vec;
							vec.x = dommy::arnold::checkedFloat(tmp[i][0]);
							vec.y = dommy::arnold::checkedFloat(tmp[i][1]);
							vec.z = dommy::arnold::checkedFloat(tmp[i][2]);
							dommy::arnold::requireArrayWrite(AiArraySetVec(outArray.get(), i, vec));
						}
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform VECTOR");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else {
						AiMsgWarning("Failed export attribute %s!", attrName.c_str());
					}
					
					
					
				} else {
					AiMsgWarning("Export attribute (%s) is not exist!", attrName.c_str());
					
				}
			
			}// curve atribute
			
			
			
			
 #pragma mark export point attribute
			
			
			for (uint i=0; i<pointAttrNameArray.length(); i++) {
				std::string attrName = pointAttrNameArray[i].asChar();
				if (!hasRenderablePointAttribute(*dCurves, attrName)) {
					AiMsgWarning("Skipping malformed point attribute (%s)!", attrName.c_str());
					continue;
				}
				
				auto cIt = sourceCurves->begin() + renderableCurveIndices.front();
				if ((*cIt)->isExistAttr(attrName)) {
					
					size_t pointOffset=0;
					
					DP_ATTR_DATA_TYPE type =(*cIt)->getAttr(attrName)->getAttrType();
					
					if (type == DP_ATTR_INT) {
						
						dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
							parameterLayout.elementCount,
							parameterLayout.keyCount, AI_TYPE_INT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold integer attribute.");
							return;
						}
						
						for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
							auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
							if ((*cIt)->isExistAttr(attrName)) {
								std::vector<int32_t> tmp = (*cIt)->attrListValue<int32_t>(attrName);
								
								
								
								const auto& layout = renderableCurveLayouts[renderCurveIndex];
								
								for (std::size_t varyingIndex = 0U; varyingIndex < layout.varyingCount(); ++varyingIndex) {
									const std::size_t c = layout.varyingSourceIndex(varyingIndex);
									dommy::arnold::requireArrayWrite(AiArraySetInt(
										outArray.get(), pointOffset++, tmp[c]));
								}
								
								
							} else {
								AiMsgWarning("Not exist point attribute (%s)!",attrName.c_str());
								continue;
							}
						}
						
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying INT");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_FLOAT || type == DP_ATTR_DOUBLE) {
						
						dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
							parameterLayout.elementCount,
							parameterLayout.keyCount, AI_TYPE_FLOAT));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold float attribute.");
							return;
						}
						
						for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
							auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
							if ((*cIt)->isExistAttr(attrName)) {
								
								std::vector<float> tmp;
								if (type == DP_ATTR_FLOAT) {
									tmp = (*cIt)->attrListValue<float>(attrName);
								} else if (type == DP_ATTR_DOUBLE) {
									std::vector<double> vals = (*cIt)->attrListValue<double>(attrName);
									tmp = dommy::arnold::checkedFloatValues(vals);
								}
								
								
								
								const auto& layout = renderableCurveLayouts[renderCurveIndex];
								
								for (std::size_t varyingIndex = 0U; varyingIndex < layout.varyingCount(); ++varyingIndex) {
									const std::size_t c = layout.varyingSourceIndex(varyingIndex);
									dommy::arnold::requireArrayWrite(AiArraySetFlt(
										outArray.get(), pointOffset++, tmp[c]));
								}
								
								
								
							} else {
								AiMsgWarning("Not exist point attribute (%s)!",attrName.c_str());
								continue;
							}
						}
						
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying FLOAT");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_VEC2I || type == DP_ATTR_VEC2F || type == DP_ATTR_VEC2D) {
						
						dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
							parameterLayout.elementCount,
							parameterLayout.keyCount, AI_TYPE_VECTOR2));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector2 attribute.");
							return;
						}
						
						for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
							auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
							if ((*cIt)->isExistAttr(attrName)) {
								
								std::vector<Imath::V2f> tmp;
								if(type == DP_ATTR_VEC2I) {
									std::vector<Imath::V2i> vals = (*cIt)->attrListValue<Imath::V2i>(attrName);
									tmp = dommy::arnold::checkedFloatVectors<Imath::V2f>(vals);
								} else if (type == DP_ATTR_VEC2F) {
									tmp = (*cIt)->attrListValue<Imath::V2f>(attrName);
								} else if (type == DP_ATTR_VEC2D) {
									std::vector<Imath::V2d> vals = (*cIt)->attrListValue<Imath::V2d>(attrName);
									tmp = dommy::arnold::checkedFloatVectors<Imath::V2f>(vals);
								}
								
								
								
								
								const auto& layout = renderableCurveLayouts[renderCurveIndex];
								
								for (std::size_t varyingIndex = 0U; varyingIndex < layout.varyingCount(); ++varyingIndex) {
									const std::size_t c = layout.varyingSourceIndex(varyingIndex);
									AtVector2 pt2;
									pt2.x = tmp[c][0];
									pt2.y = tmp[c][1];
									dommy::arnold::requireArrayWrite(AiArraySetVec2(
										outArray.get(), pointOffset++, pt2));
								}
								
								
							} else {
								AiMsgWarning("Not exist point attribute (%s)!",attrName.c_str());
								continue;
							}
						}
						
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying POINT2");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else if (type == DP_ATTR_VEC3I || type == DP_ATTR_VEC3F || type == DP_ATTR_VEC3D) {
						
						dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
							parameterLayout.elementCount,
							parameterLayout.keyCount, AI_TYPE_VECTOR));
						if (!outArray) {
							AiMsgError("Unable to allocate an Arnold vector attribute.");
							return;
						}
						
						for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
							auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
							if ((*cIt)->isExistAttr(attrName)) {
								std::vector<Imath::V3f> tmp;
								if(type == DP_ATTR_VEC3I) {
									std::vector<Imath::V3i> vals = (*cIt)->attrListValue<Imath::V3i>(attrName);
									tmp = dommy::arnold::checkedFloatVectors<Imath::V3f>(vals);
								} else if (type == DP_ATTR_VEC3F) {
									tmp = (*cIt)->attrListValue<Imath::V3f>(attrName);
								} else if (type == DP_ATTR_VEC3D) {
									std::vector<Imath::V3d> vals = (*cIt)->attrListValue<Imath::V3d>(attrName);
									tmp = dommy::arnold::checkedFloatVectors<Imath::V3f>(vals);
								}
								
								
								
								
								const auto& layout = renderableCurveLayouts[renderCurveIndex];
								
								for (std::size_t varyingIndex = 0U; varyingIndex < layout.varyingCount(); ++varyingIndex) {
									const std::size_t c = layout.varyingSourceIndex(varyingIndex);
									AtVector vec;
									vec.x = tmp[c][0];
									vec.y = tmp[c][1];
									vec.z = tmp[c][2];
									dommy::arnold::requireArrayWrite(AiArraySetVec(
										outArray.get(), pointOffset++, vec));
								}
								
								
							} else {
								AiMsgWarning("Not exist point attribute (%s)!",attrName.c_str());
								continue;
							}
						}
						
						dommy::arnold::requireUserParameter(node, attrName.c_str(), "varying VECTOR");
						AiNodeSetArray(
							node, attrName.c_str(), outArray.release());
						
					} else {
						AiMsgWarning("Not Support point attribute (%s)!",attrName.c_str());
						continue;
					}
				} else {
					AiMsgWarning("Not Exist point attribute (%s)!",attrName.c_str());
					continue;
				}
				
			}
			
			
			
			
            MPlug CurveShaderPlug = FindMayaPlug("CurveShader");
			MPlug shPlug;
			if (!plugs.source(CurveShaderPlug, "CurveShader", shPlug)) return;
			
			if (!shPlug.isNull()) {
				AtNode* aiSgNode = ExportConnectedNode(shPlug);
				if (!aiSgNode) {
					AiMsgError("[DCurveTranslatorAi] Unable to export the connected shader.");
					return;
				}
				AiNodeSetPtr( node, AtString("shader"), aiSgNode);
			} else {
				AiNodeResetParameter(node, "shader");
			}
			
			
#pragma mark shader and render flag
			
			ProcessRenderFlags(node);
			ExportLightLinking(node);
			
		}//export main
		
		
		updateVisibility.complete();
	}//plug check
	
	
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DCurveTranslatorAi::Update", node)

bool DCurveTranslatorAi::RequiresMotionData() try
{
    return IsMotionBlurEnabled( MTOA_MBLUR_DEFORM ) && IsLocalMotionBlurEnabled();
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DCurveTranslatorAi::RequiresMotionData", nullptr, std::current_exception());
    return false;
}

void DCurveTranslatorAi::ExportMotion(AtNode* node) try {
    

    
    
    unsigned int motionstep = GetMotionStep();
    unsigned int numMotionStep = GetNumMotionSteps();
	dommy::arnold::PlugReader plugs(
		[this](const char* name) -> MPlug { return FindMayaPlug(name); },
		"DCurveTranslatorAi::ExportMotion");
//    motionstep++;
    
    
    
    if( !IsMotionBlurEnabled())
    {
        return;
    }
    
    
    bool isRender = false;
	if (!plugs.read("IsRender", isRender)) {
		return;
	}
    
    static const AtString visParam("visibility");
    
    MPlug dcPlug;
    bool inputConnected = false;
    if (isRender) {
        dcPlug = FindMayaPlug("inDC");
        if (!plugs.connection(dcPlug, "inDC", inputConnected)) return;
    }
    ExportMatrix(node);
    AiNodeSetByte(node, visParam, isRender);
    if (isRender && inputConnected) {
        
        dommy::arnold::UpdateVisibilityGuard updateVisibility(node);
        dommy::maya::ScopedPlugDataHandle dCurveHndOwner;
        MDataHandle& dCurveHnd = dCurveHndOwner.handle();
        if (dCurveHndOwner.read(dcPlug) != MS::kSuccess) {
            AiMsgError("[dommyTool] Unable to read Arnold DCurve motion input.");
            return;
        }
        const DCurveData* const dCurves(getInputDCfromHandle(dCurveHnd));
        if (!dCurves) {
            AiMsgError("[dommyTool] Arnold DCurve input has invalid type or topology.");
            return;
        }
        
        if (dCurves && dCurves->numberOfCurves()) {
			if (!hasRenderableCurveTopology(*dCurves)) {
				MGlobal::displayError("Arnold motion export rejected invalid or empty DCurve topology.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
            
            short basisModeVal = 0;
            bool isAddEdgeCVVal = false;
			if (!plugs.read("BasisMode", basisModeVal) ||
				!plugs.read("IsMultiEndKnot", isAddEdgeCVVal) ||
				basisModeVal < 0 || basisModeVal > 2) {
				AiMsgError(
					"[dommyTool] DCurve motion parameters are invalid.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
            
            
            const bool isCubicMode = basisModeVal != 0 && isAddEdgeCVVal;
            const bool isParamCubicMode = basisModeVal != 0 && !isAddEdgeCVVal;
            const auto* sourceCurves = dCurves->getCurveListConst();
            std::vector<std::size_t> renderableCurveIndices;
            std::vector<dommy::arnold::CurveLayout> renderableCurveLayouts;
            std::size_t totalCVCount = 0U, totalParamCount = 0U;
            if (!makeRenderableCurvePlan(*dCurves, basisModeVal, isAddEdgeCVVal,
                    renderableCurveIndices, renderableCurveLayouts, totalCVCount, totalParamCount)) {
                AiMsgError("Unable to plan Arnold curve source/output correspondence.");
                return;
            }
            dommy::arnold::ArrayLayout pointLayout;
            if (!dommy::arnold::checkedArrayLayout(
                    totalCVCount, 1U, pointLayout)) {
                AiMsgError(
                    "Arnold curve motion array exceeds the allocation budget.");
                AiNodeSetByte(node, visParam, 0);
                return;
            }
            

            
#pragma mark make point
            dommy::arnold::ArrayOwner pointarray(AiArrayAllocate(
                pointLayout.elementCount, pointLayout.keyCount,
                AI_TYPE_VECTOR));
            if (!pointarray) {
                AiMsgError("Unable to allocate the Arnold curve motion array.");
                return;
            }
            size_t pointOffset=0;
            
            for (std::size_t renderCurveIndex = 0U; renderCurveIndex < renderableCurveIndices.size(); ++renderCurveIndex) {
                auto cIt = sourceCurves->begin() + renderableCurveIndices[renderCurveIndex];
                if (isParamCubicMode && (*cIt)->numberOfPoints() < 4U) {
                    continue;
                }
                
                DPointListPtr pts;
                MMatrix mat;
                if (!(*cIt)->isIdentity()) {
                    pts.reset(new DPointList((*cIt)->getPointsInWorld()));
                } else {
                    pts.reset(new DPointList(
                        *(*cIt)->getPositionPtrConst()));
                }
                
                if(isCubicMode) {
                    AtVector aVec;
                    aVec.x = pts->front()[0];
                    aVec.y = pts->front()[1];
                    aVec.z = pts->front()[2];
                    dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
                    pointOffset++;
                    
                    dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
                    pointOffset++;
                }
                
                for (size_t p = 0;p<pts->size();++p) {
                    //                    for (auto pIt=(*cIt)->beginPt(); pIt!=(*cIt)->endPt(); ++pIt) {
                    AtVector aVec;
                    aVec.x = (*pts)[p][0];
                    aVec.y = (*pts)[p][1];
                    aVec.z = (*pts)[p][2];
                    
                    dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
                    pointOffset++;
                }
                
                if(isCubicMode) {
                    AtVector aVec;
                    aVec.x = pts->back()[0];
                    aVec.y = pts->back()[1];
                    aVec.z = pts->back()[2];
                    dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
                    pointOffset++;
                    
                    dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), pointOffset, aVec));
                    pointOffset++;
                }
                
            }
			
            AtArray* const destinationPoints = AiNodeGetArray(node, AtString("points"));
            if (!dommy::arnold::setCheckedVectorMotionKey(
                    destinationPoints, pointarray.get(), motionstep, numMotionStep)) {
                AiMsgError("[DCurveTranslatorAi] Incompatible motion arrays or failed key write.");
                return;
            }
            
#pragma mark set curve width
            
            

            
        }//export main
        
        
        updateVisibility.complete();
    }//plug check
    
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DCurveTranslatorAi::ExportMotion", node)





AtNode* DCurveTranslatorAi::CreateArnoldNodes() try
{
	return AddArnoldNode("curves");
//	return AddArnoldNode("sphere");
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DCurveTranslatorAi::CreateArnoldNodes", nullptr, std::current_exception());
    return nullptr;
}

void* DCurveTranslatorAi::creator()
{
    return ::dommy::arnold::createObject("DCurveTranslatorAi", [&]() {
        return new DCurveTranslatorAi();
    });
}
