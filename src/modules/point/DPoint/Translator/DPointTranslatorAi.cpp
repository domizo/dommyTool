//
//  DPointTranslatorAi.cpp
//  dommyTool
//
//  Created by yuzo on 3/26/17.
//
//

#include "DPointTranslatorAi.h"
#include "common/utility/DMayaPlugDataHandle.h"
#include "common/utility/DMayaNameList.h"




#include <ai_msg.h>
#include <ai_nodes.h>

#include "DMayaHeaders.h"
#include "DPointDataHeaders.h"
#include "modules/point/DPoint/Common/DPAttributeAccess.h"

#include "DNodeOperation.h"
#include "DPointDisplay.h"
#include "DLogConfig.h"
#include "DMayaDebug.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldMotionKey.h"
#include "integrations/arnold/DArnoldEntryPointGuard.h"
#include "integrations/arnold/DArnoldMayaPlug.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldNumeric.h"

#include <cmath>

AtNode* DPointTranslatorAi::CreateArnoldNodes() try
{
    return AddArnoldNode("points");
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DPointTranslatorAi::CreateArnoldNodes", nullptr, std::current_exception());
    return nullptr;
}

void* DPointTranslatorAi::creator()
{
    return ::dommy::arnold::createObject("DPointTranslatorAi", [&]() {
        return new DPointTranslatorAi();
    });
}

void DPointTranslatorAi::Export(AtNode* node) try
{
	AiMsgDebug("[dommyTool] Exporting %s", GetMayaNodeName().asChar());
	Update(node);
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPointTranslatorAi::Export", node)

void DPointTranslatorAi::NodeChanged(MObject& node, MPlug& plug) try
{
	MStatus status;
    MFnDependencyNode nodeFn(node, &status);
    if (!status) {
        AiMsgError("[DPointTranslatorAi] Unable to inspect the changed Maya node.");
        return;
    }
	
	const MString pName = plug.partialName(
        false, false, false, false, false, true, &status);
    if (!status) {
        AiMsgError("[DPointTranslatorAi] Unable to read the changed Maya plug name.");
        return;
    }
	if(pName == "inDP" ||
	   pName == "IsRender" ||
       pName == "aiOpaque" ||
       pName == "aiSelfShadows" ||
       pName == "aiReceiveShadows" ||
       pName == "aiMatte" ||
	   pName == "ConstantWidth" ||
	   pName == "PointWidthType" ||
	   pName == "WidthAttributeName" ||
	   pName == "RenderMode" ||
	   pName == "ExportPointAttribute" ||
	   pName == "ExportAnimPointAttribute") {
		
		
		SetUpdateMode(CNodeTranslator::AI_RECREATE_TRANSLATOR);
		
		RequestUpdate();
	}
	
	
}

DOMMY_ARNOLD_TRANSLATOR_CATCH("DPointTranslatorAi::NodeChanged", nullptr)


void DPointTranslatorAi::Update(AtNode* node) try
{
	const bool VERBOSE = DOMMY_ENABLE_DEBUG_LOGS != 0;
    if (VERBOSE) {
        AiMsgDebug("[DPointTranslatorAi] Update");
    }
	//    unsigned int motionstep = GetMotionStep();
	unsigned int numMotionStep = GetNumMotionSteps();
	dommy::arnold::PlugReader plugs(
		[this](const char* name) -> MPlug { return FindMayaPlug(name); },
		"DPointTranslatorAi");
	
//    cout << "ExportMatrix" << endl;
//	AiNodeSetByte( node, "visibility", ComputeVisibility() );
	bool isRender = false;
	if (!plugs.read("IsRender", isRender)) {
		return;
	}
    if(VERBOSE) DOMMY_MAYA_DEBUG("DPointTranslatorAi", "isRender:" << isRender);
	static const AtString visParam("visibility");
	
	
    
    MPlug dpPlug;
    bool inputConnected = false;
    if (isRender) {
        dpPlug = FindMayaPlug("inDP");
        if (!plugs.connection(dpPlug, "inDP", inputConnected)) return;
    }
    ExportMatrix(node);
    AiNodeSetByte(node, visParam, isRender);
	if (isRender && inputConnected) {
//        cout << "isRender" << endl;
		dommy::arnold::UpdateVisibilityGuard updateVisibility(node);
		dommy::maya::ScopedPlugDataHandle dPointHndOwner;
		MDataHandle& dPointHnd = dPointHndOwner.handle();
		if (dPointHndOwner.read(dpPlug) != MS::kSuccess) {
			AiMsgError("[dommyTool] Unable to read Arnold DPoint input.");
			return;
		}
		const DPData* const dPoints(getInputDPfromHandle(dPointHnd));
		if (!dPoints) {
		    AiMsgError("[dommyTool] Arnold DPoint input has invalid type or topology.");
		    return;
		}
        
		if (dPoints && dPoints->numberOfPoints()) {
			
			size_t totalCount = dPoints->numberOfPoints();
			dommy::arnold::ArrayLayout pointLayout;
			dommy::arnold::ArrayLayout radiusLayout;
			if (!dommy::arnold::checkedArrayLayout(
					totalCount, numMotionStep, pointLayout) ||
				!dommy::arnold::checkedArrayLayout(
					totalCount, 1U, radiusLayout)) {
				AiMsgError("Arnold point arrays exceed the allocation budget.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			
			
			short renderTypeVal = 0;
			short curveWidthType = 0;
			float constRadVal = 0.0f;
			std::string radAttrName;
			MString otherAttrName;
			if (!plugs.read("RenderMode", renderTypeVal) ||
				!plugs.read("PointWidthType", curveWidthType) ||
				!plugs.read("ConstantWidth", constRadVal) ||
				!plugs.readString("WidthAttributeName", radAttrName) ||
				!plugs.read("ExportPointAttribute", otherAttrName) ||
				renderTypeVal < 0 || renderTypeVal > 2 ||
				curveWidthType < 0 || curveWidthType > 1 ||
				!std::isfinite(constRadVal) || constRadVal < 0.0f) {
				AiMsgError("[dommyTool] DPoint render parameters are invalid.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			
			//radius
			DScalarDataList radiusArray(totalCount, constRadVal);
			if (curveWidthType==1 && dPoints->isExistAttr(radAttrName)) {
                if (!getCheckedDScalarArrayFromAttribute(
                        *dPoints, radAttrName, totalCount, radiusArray)) {
                    AiMsgError("[DPointTranslatorAi] Unable to read the numeric radius attribute.");
                    return;
                }
			}
			if (radiusArray.size() != totalCount) {
				AiMsgError("[dommyTool] DPoint radius attribute has an invalid size.");
				AiNodeSetByte(node, visParam, 0);
				return;
			}
			for (size_t index = 0; index < radiusArray.size(); ++index) {
				if (!std::isfinite(radiusArray[index]) || radiusArray[index] < 0.0) {
					AiMsgError(
						"[dommyTool] DPoint radius attribute contains invalid values.");
					AiNodeSetByte(node, visParam, 0);
					return;
				}
			}
			
			
			///other attribute
			MStringArray attrNameArray;
			dommy::maya::NameListBudget nameListBudget;
			if (dommy::maya::splitCheckedNameList(
			        otherAttrName, totalCount, nameListBudget, attrNameArray) != MS::kSuccess) {
				AiMsgError("[DPointTranslatorAi] Unable to parse the export attribute list.");
				return;
			}
			if(VERBOSE)DOMMY_MAYA_DEBUG("DPointTranslatorAi", "attribute count:" << attrNameArray.length());
			
			
			//make point
			dommy::arnold::ArrayOwner pointarray(AiArrayAllocate(
				pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_VECTOR));
			if (!pointarray) {
				AiMsgError("Unable to allocate the Arnold point motion array.");
				return;
			}
			dommy::arnold::ArrayOwner radiusarray(AiArrayAllocate(
				radiusLayout.elementCount, radiusLayout.keyCount, AI_TYPE_FLOAT));
			if (!radiusarray) {
				AiMsgError("Unable to allocate the Arnold point radius array.");
				return;
			}
			
			size_t index = 0;
			for (auto pIt=dPoints->beginPt();pIt!=dPoints->endPt();++pIt)
			{
				AtVector aVec;
				aVec.x = (*pIt)[0];
				aVec.y = (*pIt)[1];
				aVec.z = (*pIt)[2];
				
				dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), index, aVec));
				dommy::arnold::requireArrayWrite(AiArraySetFlt(radiusarray.get(), index, radiusArray[index]));
				
				index++;
			}
			
			
			AiNodeSetArray(node, "points", pointarray.release());
			AiNodeSetArray(node, "radius", radiusarray.release());
			
			
#pragma mark set render mode
			switch (renderTypeVal) {
				case 0:
					AiNodeSetStr(node, "mode", "sphere");
					break;
				case 1:
					AiNodeSetStr(node, "mode", "disk");
					break;
				case 2:
					AiNodeSetStr(node, "mode", "quad");
					break;
				default:
					break;
			}
			
#pragma mark set attribute
			
			
			if (attrNameArray.length()) {
				for (uint i=0; i<attrNameArray.length(); i++) {
					std::string attrName = attrNameArray[i].asChar();
					
					//サイズアトリビュートと同じ場合は飛ばす
					if (radAttrName == attrName) continue;
					
					
					if (dPoints->isExistAttr(attrName)) {
						const DPAttributeData::ConstPtr attribute =
							dPoints->getAttr(attrName);
						if (!attribute || attribute->attrSize() != totalCount) {
							AiMsgWarning(
								"Skipping Arnold point attribute %s with an invalid size.",
								attrName.c_str());
							continue;
						}
						DP_ATTR_DATA_TYPE type = attribute->getAttrType();
						
						if (type == DP_ATTR_INT) {
							std::vector<int32_t> tmp = dPoints->attrListValue<int32_t>(attrName);
							
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
							
							std::vector<float> tmp = dPoints->attrListValue<float>(attrName);
							
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
							
							std::vector<double> tmp = dPoints->attrListValue<double>(attrName);
							
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
							std::vector<Imath::V2i> tmp = dPoints->attrListValue<Imath::V2i>(attrName);
							
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
							std::vector<Imath::V2f> tmp = dPoints->attrListValue<Imath::V2f>(attrName);
							
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
							std::vector<Imath::V2d> tmp = dPoints->attrListValue<Imath::V2d>(attrName);
							
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
							std::vector<Imath::V3i> tmp = dPoints->attrListValue<Imath::V3i>(attrName);
							
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
							std::vector<Imath::V3f> tmp = dPoints->attrListValue<Imath::V3f>(attrName);
							
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
							std::vector<Imath::V3d> tmp = dPoints->attrListValue<Imath::V3d>(attrName);
							
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
						AiMsgWarning("Not exist attribute %s!", attrName.c_str());
					}// check exist attribute
					
				}// for attribute name
			}// attribute rutine
			
			
			
#pragma mark shader and render flag
			
            
//            cout << "shader and render flag" << endl;
            
            
			MPlug PointShaderPlug = FindMayaPlug("PointShader");
			MPlug shPlug;
			if (!plugs.source(PointShaderPlug, "PointShader", shPlug)) return;
			if(VERBOSE) DOMMY_MAYA_DEBUG("DPointTranslatorAi", "PointShaderPlug:" << !shPlug.isNull());
			
			if (!shPlug.isNull()) {
				AtNode* aiSgNode = ExportConnectedNode(shPlug);
				if (!aiSgNode) {
					AiMsgError("[DPointTranslatorAi] Unable to export the connected shader.");
					return;
				}
				AiNodeSetPtr( node, "shader", aiSgNode);
			} else {
				AiNodeResetParameter(node, "shader");
			}
            
//            cout << "ProcessRenderFlags" << endl;
			
			ProcessRenderFlags(node);
			ExportLightLinking(node);
			
			
			}//export main
		
		
		updateVisibility.complete();
	}//plug check
	
//    cout << "End Update" << endl;
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPointTranslatorAi::Update", node)

bool DPointTranslatorAi::RequiresMotionData() try
{
	return IsMotionBlurEnabled( MTOA_MBLUR_DEFORM ) && IsLocalMotionBlurEnabled();
}
catch (...) {
    ::dommy::arnold::reportTranslatorException(
        "DPointTranslatorAi::RequiresMotionData", nullptr, std::current_exception());
    return false;
}

void DPointTranslatorAi::ExportMotion(AtNode* node) try {
	
	unsigned int motionstep = GetMotionStep();
	unsigned int numMotionStep = GetNumMotionSteps();
	//    motionstep++;
	
	if( !IsMotionBlurEnabled())
	{
		return;
	}
	
	
    dommy::arnold::PlugReader plugs(
        [this](const char* name) -> MPlug { return FindMayaPlug(name); },
        "DPointTranslatorAi::ExportMotion");
    MPlug dpPlug = FindMayaPlug("inDP");
    bool inputConnected = false;
    if (!plugs.connection(dpPlug, "inDP", inputConnected)) return;
    ExportMatrix(node);
	if (inputConnected) {
		
		dommy::arnold::UpdateVisibilityGuard updateVisibility(node);
		dommy::maya::ScopedPlugDataHandle dPointHndOwner;
		MDataHandle& dPointHnd = dPointHndOwner.handle();
		if (dPointHndOwner.read(dpPlug) != MS::kSuccess) {
			AiMsgError("[dommyTool] Unable to read Arnold DPoint motion input.");
			return;
		}
		const DPData* const dPoints(getInputDPfromHandle(dPointHnd));
		if (!dPoints) {
		    AiMsgError("[dommyTool] Arnold DPoint input has invalid type or topology.");
		    return;
		}
        
		if (dPoints && dPoints->numberOfPoints()) {
			
			size_t totalCount = dPoints->numberOfPoints();
			dommy::arnold::ArrayLayout pointLayout;
			if (!dommy::arnold::checkedArrayLayout(
					totalCount, 1U, pointLayout)) {
				AiMsgError(
					"Arnold point motion array exceeds the allocation budget.");
				return;
			}
			
				//make point
			dommy::arnold::ArrayOwner pointarray(AiArrayAllocate(
				pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_VECTOR));
			if (!pointarray) {
				AiMsgError("Unable to allocate the Arnold point motion array.");
				return;
			}
			
			
			size_t index = 0;
			for (auto pIt=dPoints->beginPt();pIt!=dPoints->endPt();++pIt)
			{
				AtVector aVec;
				aVec.x = (*pIt)[0];
				aVec.y = (*pIt)[1];
				aVec.z = (*pIt)[2];
				
				dommy::arnold::requireArrayWrite(AiArraySetVec(pointarray.get(), index, aVec));
				
				index++;
			}
			
			
			AtArray* const destinationPoints = AiNodeGetArray(node, "points");
			if (!dommy::arnold::setCheckedVectorMotionKey(
			        destinationPoints, pointarray.get(), motionstep, numMotionStep)) {
				AiMsgError("[DPointTranslatorAi] Incompatible motion arrays or failed key write.");
				return;
			}
			
#pragma mark set attribute
			
			
		}//export main
		
		
		updateVisibility.complete();
	}//plug check
	
}
DOMMY_ARNOLD_TRANSLATOR_CATCH("DPointTranslatorAi::ExportMotion", node)
