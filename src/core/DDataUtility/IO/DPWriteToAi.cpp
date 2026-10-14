//
//  DPWriteToAi.cpp
//  dommyTool
//
//  Created by yuzo on 11/20/16.
//
//

#include "DPWriteToAi.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaNameList.h"
#include "PluginFeatures.h"
#include "common/file/DExportPath.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"

#include "DPointDataHeaders.h"
#include "modules/point/DPoint/Common/DPAttributeAccess.h"



#include "DNodeOperation.h"
#include "DFileOperation.h"
#include "DArnoldSession.h"
#include "integrations/arnold/DArnoldArrayBudget.h"
#include "integrations/arnold/DArnoldResources.h"
#include "integrations/arnold/DArnoldUserParameters.h"
#include "integrations/arnold/DArnoldAssWrite.h"
#include "integrations/arnold/DArnoldNumeric.h"
#include "DEvaluationContext.h"

#include <ai.h>

#include <cmath>
#include <limits>



const MTypeId DPWriteToAi::typeId( ID_DP_TO_AI );


MObject DPWriteToAi::inDP;

MObject DPWriteToAi::aFilePath;
MObject DPWriteToAi::aExportMode;
MObject DPWriteToAi::aExportLog;

MObject DPWriteToAi::aRenderType;
MObject DPWriteToAi::aConstantRadius;
MObject DPWriteToAi::aRadiusAttributeName;

MObject DPWriteToAi::aOutputAttributes;

MObject DPWriteToAi::outDP;

void *DPWriteToAi::creator()
{
    return ::dommy::maya::createObject("DPWriteToAi", [&]() {
        return new DPWriteToAi();
    });
}

MStatus DPWriteToAi::exportDPointToArnold(
	MDataBlock& data,
	const DPData* inputDPData,
	const std::string& filename) {
	

	
	//get point
	const DPointList pts = inputDPData->getPositionConst();
	dommy::arnold::ArrayLayout pointLayout;
	if (!dommy::arnold::checkedArrayLayout(pts.size(), 1U, pointLayout)) {
		MGlobal::displayWarning(
			"DPWriteToAi: point count exceeds the Arnold allocation budget.");
		return MS::kFailure;
	}
	
	MStatus status;
	MDataHandle exportLogHandle = data.inputValue(aExportLog, &status);
	if (status != MS::kSuccess) return status;
	MDataHandle renderTypeHandle = data.inputValue(aRenderType, &status);
	if (status != MS::kSuccess) return status;
	MDataHandle radiusHandle = data.inputValue(aConstantRadius, &status);
	if (status != MS::kSuccess) return status;
	MDataHandle radiusAttributeHandle =
		data.inputValue(aRadiusAttributeName, &status);
	if (status != MS::kSuccess) return status;
	MDataHandle outputAttributesHandle =
		data.inputValue(aOutputAttributes, &status);
	if (status != MS::kSuccess) return status;

	const bool exportLog = exportLogHandle.asBool();
	const short renderTypeVal = renderTypeHandle.asShort();
	const double constRadVal = radiusHandle.asDouble();
	const std::string radAttrName =
		radiusAttributeHandle.asString().asChar();
	const MString otherAttrName = outputAttributesHandle.asString();
	if (renderTypeVal < 0 || renderTypeVal > 2 ||
	    !std::isfinite(constRadVal) || constRadVal < 0.0) {
		MGlobal::displayError(
			"DPWriteToAi: render type or radius is invalid.");
		return MS::kInvalidParameter;
	}
	
	MDoubleArray radiusArray(pts.size(), constRadVal);
	if (!radAttrName.empty() && inputDPData->isExistAttr(radAttrName)) {
        if (!getCheckedMRealArrayFromAttribute(
                *inputDPData, radAttrName, pts.size(), radiusArray)) {
            MGlobal::displayError("DPWriteToAi: unable to read the numeric radius attribute.");
            return MS::kFailure;
        }
		if (radiusArray.length() != pts.size()) {
			MGlobal::displayWarning(
				"DPWriteToAi: radius attribute count does not match the point count.");
			return MS::kFailure;
		}
	}
	for (size_t index = 0; index < pts.size(); ++index) {
		const DPoint& point = inputDPData->positionAt(index);
		if (!std::isfinite(point[0]) ||
		    !std::isfinite(point[1]) ||
		    !std::isfinite(point[2]) ||
		    !std::isfinite(radiusArray[index]) ||
		    radiusArray[index] < 0.0) {
			MGlobal::displayError(
				"DPWriteToAi: point positions and radii must be finite.");
			return MS::kInvalidParameter;
		}
	}

	MStringArray attrNameArray;
	
	dommy::maya::NameListBudget nameListBudget;
	DOMMY_RETURN_IF_FAILURE(dommy::maya::splitCheckedNameList(
	    otherAttrName, pts.size(), nameListBudget, attrNameArray));
	
	
	
	
	
	dommy::arnold::Session arnoldSession;
	
	if (exportLog) {
		AiMsgSetLogFileName("dpointOut.log");
		AiMsgSetConsoleFlags(AI_LOG_ALL);
	}
	
	//make point
	dommy::arnold::ArrayOwner pointArray(AiArrayAllocate(
		pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_VECTOR));
	dommy::arnold::ArrayOwner radiusArrayOwner(AiArrayAllocate(
		pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_FLOAT));
	if (!pointArray || !radiusArrayOwner) {
		MGlobal::displayError(
			"DPWriteToAi: failed to allocate the required Arnold arrays.");
		return MS::kFailure;
	}
	
	for (size_t i = 0; i < pts.size(); ++i)
	{
		AtVector aVec;
		const DPoint& point = inputDPData->positionAt(i);
		aVec.x = dommy::arnold::checkedFloat(point[0]);
		aVec.y = dommy::arnold::checkedFloat(point[1]);
		aVec.z = dommy::arnold::checkedFloat(point[2]);
		
		dommy::arnold::requireArrayWrite(AiArraySetVec(pointArray.get(), i, aVec));
		dommy::arnold::requireArrayWrite(AiArraySetFlt(radiusArrayOwner.get(), i, dommy::arnold::checkedFloat(radiusArray[i])));
	}
	
	
	dommy::arnold::NodeOwner nodeOwner(AiNode("points"));
	if (!nodeOwner) {
		MGlobal::displayError("DPWriteToAi: failed to create the Arnold points node.");
		return MS::kFailure;
	}
	AtNode* const node = nodeOwner.get();
	AiNodeSetArray(node, "points", pointArray.release());
	AiNodeSetArray(node, "radius", radiusArrayOwner.release());
	
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
	
	
	//add attribute
	if (attrNameArray.length()) {
		for (uint i=0; i<attrNameArray.length(); i++) {
			std::string attrName = attrNameArray[i].asChar();
			
			//サイズアトリビュートと同じ場合は飛ばす
			if (radAttrName == attrName) continue;
			
			
			if (inputDPData->isExistAttr(attrName)) {
				
				DPAttributeData::ConstPtr attr = inputDPData->getAttr(attrName);
				if (!attr || attr->attrSize() != pts.size()) {
					MGlobal::displayWarning(
						"DPWriteToAi: skipped an attribute whose count does not match the points.");
					continue;
				}
				std::string typeStr = attr->typeName();
				
				if (typeStr == "int") {
//					std::vector<int> dataArray = inputDPData->attrListValue<int>(attrName);
                    MIntArray dataArray(pts.size());
                    getMIntArrayFromAttribute(*inputDPData, attrName, dataArray);
                    
					dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
						pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_INT));
					if (!outArray) {
						MGlobal::displayError(
							"DPWriteToAi: failed to allocate an Arnold integer attribute.");
						return MS::kFailure;
					}
					
					for (size_t i = 0; i < pts.size(); ++i) {
						dommy::arnold::requireArrayWrite(AiArraySetInt(outArray.get(), i, dataArray[i]));
					}
                    dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform INT");
                    AiNodeSetArray(node, attrName.c_str(), outArray.release());
					
				} else if (typeStr == "float" || typeStr == "double") {
//					std::vector<float> dataArray = inputDPData->attrListValue<float>(attrName);
                    MDoubleArray dataArray(pts.size());
                    getMDoubleArrayFromAttribute(*inputDPData, attrName, dataArray);
                    
					dommy::arnold::ArrayOwner outArray(AiArrayAllocate(
						pointLayout.elementCount, pointLayout.keyCount, AI_TYPE_FLOAT));
					if (!outArray) {
						MGlobal::displayError(
							"DPWriteToAi: failed to allocate an Arnold float attribute.");
						return MS::kFailure;
					}
					for (size_t i = 0; i < pts.size(); ++i) {
						dommy::arnold::requireArrayWrite(AiArraySetFlt(outArray.get(), i, dommy::arnold::checkedFloat(dataArray[i])));
					}
					dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform FLOAT");
					AiNodeSetArray(node, attrName.c_str(), outArray.release());
					
				} else if (typeStr == "vec2i" || typeStr == "vec2f" || typeStr == "vec2d") {
//					std::vector<Imath::V2f> dataArray = inputDPData->attrListValue<Imath::V2f>(attrName);
                    MVectorArray dataArray(pts.size());
                    getMVector2DArrayFromAttribute(*inputDPData, attrName, dataArray);
                    
					dommy::arnold::ArrayOwner point2Array(AiArrayAllocate(
						pointLayout.elementCount, pointLayout.keyCount,
						AI_TYPE_VECTOR2));
					if (!point2Array) {
						MGlobal::displayError(
							"DPWriteToAi: failed to allocate an Arnold vector2 attribute.");
						return MS::kFailure;
					}
					for (size_t i = 0; i < pts.size(); ++i) {
						AtVector2 pt2;
						pt2.x = dommy::arnold::checkedFloat(dataArray[i][0]);
						pt2.y = dommy::arnold::checkedFloat(dataArray[i][1]);
						dommy::arnold::requireArrayWrite(AiArraySetVec2(point2Array.get(), i, pt2));
					}
                    dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform POINT2");
					AiNodeSetArray(node, attrName.c_str(), point2Array.release());
					
				} else if (typeStr == "vec3i" || typeStr == "vec3f" || typeStr == "vec3d") {
					
//					std::vector<Imath::V3d> dataArray = inputDPData->attrListValue<Imath::V3d>(attrName);
                    MVectorArray dataArray(pts.size());
                    getMVectorArrayFromAttribute(*inputDPData, attrName, dataArray);
                    
					dommy::arnold::ArrayOwner point3Array(AiArrayAllocate(
						pointLayout.elementCount, pointLayout.keyCount,
						AI_TYPE_VECTOR));
					if (!point3Array) {
						MGlobal::displayError(
							"DPWriteToAi: failed to allocate an Arnold vector attribute.");
						return MS::kFailure;
					}
					for (size_t i = 0; i < pts.size(); ++i) {
						AtVector vec;
						vec.x = dommy::arnold::checkedFloat(dataArray[i][0]);
						vec.y = dommy::arnold::checkedFloat(dataArray[i][1]);
						vec.z = dommy::arnold::checkedFloat(dataArray[i][2]);
						dommy::arnold::requireArrayWrite(AiArraySetVec(point3Array.get(), i, vec));
					}
					
					dommy::arnold::requireUserParameter(node, attrName.c_str(), "uniform VECTOR");
					AiNodeSetArray(node, attrName.c_str(), point3Array.release());
					
				} else {
					MGlobal::displayWarning("DPWriteToAi::Not support Attribute type for point.");
				}
			}
			
			
		}
	}
	
	
	
	
	
	
	//write ass file
	return dommy::arnold::writeAssFile(filename, false, "DPWriteToAi");
}

MStatus DPWriteToAi::compute(const MPlug& plug, MDataBlock& data) try
{
	MStatus stat;
	if (plug == outDP) {
		

		
		//input data
		const DPData* inputDPData = nullptr;
        DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));
		
			const MDataHandle exportModeHandle = data.inputValue(aExportMode, &stat);
			DOMMY_RETURN_IF_FAILURE(stat);
			const short exportModeVal = exportModeHandle.asShort();
			if (exportModeVal < 0 || exportModeVal > 2) {
				MGlobal::displayError("DPWriteToAi: ExportMode is invalid.");
				return MS::kInvalidParameter;
			}

			if (exportModeVal != 2) {
				// Resolve and validate the path only when an export was
				// explicitly requested. No-export evaluation remains a pure
				// data pass-through.
				const MDataHandle filePathHandle = data.inputValue(aFilePath, &stat);
				DOMMY_RETURN_IF_FAILURE(stat);
				const MString srcFileName = filePathHandle.asString();
				if (srcFileName.length() == 0) {
					MGlobal::displayWarning(
						"DPWriteToAi: FilePath must not be empty.");
					return MS::kFailure;
				}

				MTime evaluationTime;
				stat = dommy::maya::getEvaluationTime(data, evaluationTime);
				DOMMY_RETURN_IF_FAILURE(stat);
				MString resolvedPath;
				if (!getSequenceFileName(
						srcFileName, evaluationTime.value(), resolvedPath)) {
					MGlobal::displayError(
						"DPWriteToAi: FilePath contains an invalid frame token.");
					return MS::kFailure;
				}
				std::string filename = resolvedPath.asChar();
				if (!dommy::file::isSafeExportPath(filename, ".ass")) {
					MGlobal::displayError(
						"DPWriteToAi: output path must be a bounded .ass "
						"path without control characters or '..' components.");
					return MS::kInvalidParameter;
				}
				

				MFileObject fileObj;
				DOMMY_RETURN_IF_FAILURE(fileObj.setRawFullName(filename.c_str()));
                filename = fileObj.resolvedFullName().asChar();
                if (!dommy::file::isSafeExportPath(filename, ".ass"))
                    return MS::kInvalidParameter;

				if (exportModeVal == 0 ||
					(exportModeVal == 1 && !fileObj.exists())) {
					DOMMY_RETURN_IF_FAILURE(
						exportDPointToArnold(data, inputDPData, filename));
				}
			}
		
		
		
		//pass outDP
		MStatus status;
		MFnPluginData pluginData;
		pluginData.create(DPData::id, &status);
		
		if (status != MS::kSuccess) {
			MGlobal::displayError("DPWriteToAi::Failed to create a new DPData object.");
			return status;
		}
		
		DPData* outputDPData = nullptr;
		stat = dommy::maya::getPluginData(pluginData, outputDPData);
		if (!stat) return stat;
		if (stat != MS::kSuccess || !outputDPData) {
			MGlobal::displayError("DPWriteToAi::Failed to access the output DPData object.");
			return MS::kFailure;
		}
		outputDPData->setDPData(*inputDPData);
		
#pragma mark Create output
		MDataHandle output = data.outputValue(outDP, &stat);
		if (!stat) return stat;
		stat = dommy::maya::publishPluginDataObject(
			output, pluginData.object(), *outputDPData);
		if (!stat) return stat;
		
		return data.setClean(plug);
	}
	
	return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("DPWriteToAi")





MStatus DPWriteToAi::initialize()
try {
	MStatus stat;
	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;
	
	MFnStringData fnStringData;
	
	// ----------------------- INPUTS --------------------------
	inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
	DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

	
#pragma mark export
	
	MObject defaultFileStringData = fnStringData.create("dpoint.ass", &stat);
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
	
	MFnEnumAttribute renAttr;
	aRenderType = renAttr.create("RenderType", "rty", 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(renAttr.addField("sphere", 0));
	DOMMY_RETURN_IF_FAILURE(renAttr.addField("disk", 1));
	DOMMY_RETURN_IF_FAILURE(renAttr.addField("quad", 2));
	DOMMY_RETURN_IF_FAILURE(renAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRenderType));
	
	
	
	aConstantRadius = nAttr.create( "ConstantRadius", "cr", MFnNumericData::kDouble , 0, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
	DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
	DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
	DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aConstantRadius));
	
	
	MObject wAttrName = fnStringData.create("pscale", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aRadiusAttributeName = tAttr.create("RadiusAttributeName", "rattr", MFnData::kString, wAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aRadiusAttributeName));
	
	
	MObject oAttrName = fnStringData.create("", &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	aOutputAttributes = tAttr.create("OutputAttributes", "oattrs", MFnData::kString, oAttrName, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(aOutputAttributes));
	
	
	
	// ----------------------- OUTPUTS --------------------------
	outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
	DOMMY_RETURN_IF_FAILURE(stat);
	DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
	DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
	DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));
	
	
	
	
#pragma mark affect attribute
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aFilePath, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aExportMode, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aExportLog, outDP ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRenderType, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aConstantRadius, outDP ));
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aRadiusAttributeName, outDP ));
	
	DOMMY_RETURN_IF_FAILURE(attributeAffects( aOutputAttributes, outDP ));
	
	
	
	return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPWriteToAi", "initialize", std::current_exception());
}
