//
//  DPGroup.cpp
//  dommyTool
//
//  Created by yuzo on 5/25/16.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPGroup.h"
#include "common/utility/DGroupEvaluation.h"
#include "common/utility/DGroupPublication.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDependencyNodeValues.h"
#include "common/utility/DLegacyDrawGuard.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DPointDataHeaders.h"

#include "DNodeOperation.h"

#include "dommyMath.h"
#include "common/utility/DEvaluationContext.h"

#pragma mark DPGroupManip


MManipData DPGroupManip::plugToManipConversion(unsigned int)
{
		MStatus status;
		MFnStateManip stateManip(fStateManip, &status);
		if (!status) return MManipData();

		MFnManip3D bboxManipFn(fBBoxManip, &status);
		if (!status) return MManipData();
		MFnManip3D bboxScaleManipFn(fScaleManip, &status);
		if (!status) return MManipData();
		MFnManip3D normManipFn(fNormalVectorManip, &status);
		if (!status) return MManipData();
		MFnManip3D spreadManipFn(fSpreadManip, &status);
		if (!status) return MManipData();

		status = bboxManipFn.setVisible(false);
		if (!status) return MManipData();
		status = bboxScaleManipFn.setVisible(false);
		if (!status) return MManipData();
		status = normManipFn.setVisible(false);
		if (!status) return MManipData();
		status = spreadManipFn.setVisible(false);
		if (!status) return MManipData();

		const unsigned int mode = stateManip.state(&status);
		if (!status) return MManipData();
		if (mode == 0U) {
			status = bboxManipFn.setVisible(true);
			if (!status) return MManipData();
			status = bboxScaleManipFn.setVisible(true);
			if (!status) return MManipData();
		} else if (mode == 1U) {
			status = normManipFn.setVisible(true);
			if (!status) return MManipData();
			status = spreadManipFn.setVisible(true);
			if (!status) return MManipData();
		}

		MFnNumericData numData;
		const MObject numDataObj = numData.create(MFnNumericData::k3Double, &status);
		if (!status || numDataObj.isNull()) return MManipData();
		const MVector origin = manipulatorOrigin();
		status = numData.setData(origin.x, origin.y, origin.z);
		return status ? MManipData(numDataObj) : MManipData();
}


MVector DPGroupManip::manipulatorOrigin() const
{
		MVector center;
		return dommy::maya::readDependencyVector(
				   nodeObj, "BoundingCenter", center)
				   ? center
				   : MVector::zero;
}

MStatus DPGroupManip::getBBoxSize(
		MVector& sizeVec,
		MVector& centerVec,
		short& typeVal) const
{
		MStatus status =
			dommy::maya::readDependencyVector(nodeObj, "BoundingCenter", centerVec);
		if (!status) return status;
		status = dommy::maya::readDependencyVector(nodeObj, "BoundingSize", sizeVec);
		if (!status) return status;
		return dommy::maya::readDependencyShort(nodeObj, "BoundingType", typeVal);
}

MTypeId DPGroupManip::id( ID_DP_GROUP_MANIP );


void* DPGroupManip::creator()
{
    return ::dommy::maya::createObject("DPGroupManip", [&]() {
        return new DPGroupManip();
    });
}

MStatus DPGroupManip::initialize()
{
	return MPxManipContainer::initialize();
}

MStatus DPGroupManip::createChildren()
{
		fBBoxManip = addFreePointTriadManip("bboxManip", "bbox");
		fScaleManip = addScaleManip("scaleManip", "scale");
		fNormalVectorManip = addFreePointTriadManip("normalManip", "normal");
		fSpreadManip = addDistanceManip("spreadManip", "spread");
		fStateManip = addStateManip("stateManip", "state");

		if (!fBBoxManip.isValid() || !fScaleManip.isValid() ||
			!fNormalVectorManip.isValid() || !fSpreadManip.isValid() ||
			!fStateManip.isValid()) {
			return MS::kFailure;
		}

		MStatus status;
		MFnDistanceManip spreadManipFn(fSpreadManip, &status);
		if (!status) return status;
		status = spreadManipFn.setStartPoint(MPoint::origin);
		if (!status) return status;
		status = spreadManipFn.setDirection(MVector::yAxis);
		if (!status) return status;
		status = spreadManipFn.setScalingFactor(0.1);
		if (!status) return status;

		MFnStateManip stateManip(fStateManip, &status);
		if (!status) return status;
		status = stateManip.setMaxStates(2U);
		if (!status) return status;
		return stateManip.setInitialState(0U);
}




MStatus DPGroupManip::connectToDependNode(const MObject &node)
{
		MStatus status;
		MFnDependencyNode nodeFn(node, &status);
		if (!status) return status;
		nodeObj = node;

		MFnFreePointTriadManip bboxManipFn(fBBoxManip, &status);
		if (!status) return status;
		MPlug bboxCenterPlug = nodeFn.findPlug("BoundingCenter", true, &status);
		if (!status) return status;
		status = bboxManipFn.connectToPointPlug(bboxCenterPlug);
		if (!status) return status;

		MFnScaleManip scaleManipFn(fScaleManip, &status);
		if (!status) return status;
		MPlug scalePlug = nodeFn.findPlug("BoundingSize", true, &status);
		if (!status) return status;
		status = scaleManipFn.connectToScalePlug(scalePlug);
		if (!status) return status;
		status = scaleManipFn.connectToScaleCenterPlug(bboxCenterPlug);
		if (!status) return status;

		MFnFreePointTriadManip normalManipFn(fNormalVectorManip, &status);
		if (!status) return status;
		MPlug normalPlug = nodeFn.findPlug("NormalDirection", true, &status);
		if (!status) return status;
		status = normalManipFn.connectToPointPlug(normalPlug);
		if (!status) return status;

		MFnDistanceManip spreadManipFn(fSpreadManip, &status);
		if (!status) return status;
		MPlug spreadPlug = nodeFn.findPlug("SpreadAngle", true, &status);
		if (!status) return status;
		status = spreadManipFn.connectToDistancePlug(spreadPlug);
		if (!status) return status;
		const unsigned int startPointIndex =
			spreadManipFn.startPointIndex(&status);
		if (!status) return status;
		addPlugToManipConversion(startPointIndex);

		MFnStateManip stateManip(fStateManip, &status);
		if (!status) return status;
		status = stateManip.setTranslation(MVector(2.0, 0.0, 0.0), MSpace::kObject);
		if (!status) return status;
		const unsigned int positionIndex = stateManip.positionIndex(&status);
		if (!status) return status;
		addPlugToManipConversion(positionIndex);

		status = finishAddingManips();
		if (!status) return status;
		return MPxManipContainer::connectToDependNode(node);
}

void DPGroupManip::draw(M3dView & view, const MDagPath &path,
								M3dView::DisplayStyle style,
								M3dView::DisplayStatus status) try
{
		MPxManipContainer::draw(view, path, style, status);
		dommy::maya::ScopedLegacyGL drawing(view);
	
		MPoint textPos = manipulatorOrigin();
		
		MFnStateManip stateManip(fStateManip);
		const unsigned int mode = stateManip.state();
		const MString distanceText(
			mode == 0U ? "Bounding Box" :
			mode == 1U ? "Normal Angle" : "Unknown");
		view.drawText(distanceText, textPos, M3dView::kLeft);
	
}
DOMMY_DRAW_CATCH("DPGroupManip")

// Viewport 2.0 manipulator draw overrides
void DPGroupManip::preDrawUI(const M3dView&)
{
		fTextPosition = manipulatorOrigin();
		if (!getBBoxSize(fBBoxSize, fBBoxCenter, fBBoxType)) {
			fBBoxSize = MVector::zero;
			fBBoxCenter = MVector::zero;
			fBBoxType = DGRP_BOUND_BOX;
		}
}

void DPGroupManip::drawUI(MHWRender::MUIDrawManager& drawManager,
								  const MHWRender::MFrameContext&) const
{
	drawManager.beginDrawable();
	
	drawManager.setColor( MColor( 0.0f, 1.0f, 0.1f ) );
	
	MFnStateManip stateManip(fStateManip);
	
	int mode = stateManip.state();
	if (mode==0) {
		drawManager.text( fTextPosition, "Bounding Box", MHWRender::MUIDrawManager::kLeft );
		
		double baseSizeX = fBBoxSize.x*.5;
		double baseSizeY = fBBoxSize.y*.5;
		double baseSizeZ = fBBoxSize.z*.5;
		
		if (fBBoxType==0) {//box
			
			MPointArray cvs;
			
			//top
			cvs.append(MPoint(-baseSizeX,baseSizeY,-baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(baseSizeX,baseSizeY,-baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(baseSizeX,baseSizeY,-baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(baseSizeX,baseSizeY,baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(baseSizeX,baseSizeY,baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(-baseSizeX,baseSizeY,baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(-baseSizeX,baseSizeY,baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(-baseSizeX,baseSizeY,-baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(-baseSizeX,-baseSizeY,-baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(baseSizeX,-baseSizeY,-baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(baseSizeX,-baseSizeY,-baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(baseSizeX,-baseSizeY,baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(baseSizeX,-baseSizeY,baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(-baseSizeX,-baseSizeY,baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(-baseSizeX,-baseSizeY,baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(-baseSizeX,-baseSizeY,-baseSizeZ)+fBBoxCenter);
			
			//side bar
			cvs.append(MPoint(-baseSizeX,baseSizeY,-baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(-baseSizeX,-baseSizeY,-baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(baseSizeX,baseSizeY,-baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(baseSizeX,-baseSizeY,-baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(baseSizeX,baseSizeY,baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(baseSizeX,-baseSizeY,baseSizeZ)+fBBoxCenter);
			
			cvs.append(MPoint(-baseSizeX,baseSizeY,baseSizeZ)+fBBoxCenter);
			cvs.append(MPoint(-baseSizeX,-baseSizeY,baseSizeZ)+fBBoxCenter);
			
			drawManager.lineList(cvs, false);
			
		} else {//sphere
			double baseSizeX = fBBoxSize.x;
			double baseSizeY = fBBoxSize.y;
			double baseSizeZ = fBBoxSize.z;
			
			const uint divisionVal = 20U;
			const double stepDegree = 360.0 / divisionVal;
			
			MPointArray cvs;
			
			//XZ
			for (uint i=0; i<divisionVal; i++) {
				cvs.append(MPoint(baseSizeX * cos(dommy::math::degreesToRadians(-(stepDegree*i))), 0.0, baseSizeZ * sin(dommy::math::degreesToRadians(-(stepDegree*i))))+fBBoxCenter);
			}
			cvs.append(MPoint(baseSizeX * cos(0), 0.0, baseSizeZ * sin(0))+fBBoxCenter);

			
			
			//YZ
			for (uint i=0; i<divisionVal; i++) {
				cvs.append(MPoint(0.0, baseSizeY * cos(dommy::math::degreesToRadians(-(stepDegree*i))), baseSizeZ * sin(dommy::math::degreesToRadians(-(stepDegree*i))))+fBBoxCenter);
			}
			cvs.append(MPoint(0.0, baseSizeY * cos(0), baseSizeZ * sin(0))+fBBoxCenter);
			
			//XY
			for (uint i=0; i<divisionVal; i++) {
				cvs.append(MPoint(MPoint(baseSizeX * cos(dommy::math::degreesToRadians(-(stepDegree*i))), baseSizeY * sin(dommy::math::degreesToRadians(-(stepDegree*i))), 0.0))+fBBoxCenter);
			}
			cvs.append(MPoint(MPoint(baseSizeX * cos(0), baseSizeY * sin(0), 0.0))+fBBoxCenter);
			
			drawManager.lineStrip(cvs, false);
		}
		
		
		
		
		
	} else if (mode==1) {
		drawManager.text( fTextPosition, "Normal Angle", MHWRender::MUIDrawManager::kLeft );
	}
	
	
	drawManager.endDrawable();
}



#pragma mark DPGroup

MTypeId DPGroup::typeId( ID_DP_GROUP_CREATE );


MObject DPGroup::inDP;

MObject DPGroup::aGroupName;
MObject DPGroup::aExistActionType;

//number
MObject DPGroup::aIsNumberAction;
MObject DPGroup::aNumberType;
MObject DPGroup::aPattern;
MObject DPGroup::aStartEnd;
MObject DPGroup::aSelectOf;
MObject DPGroup::aExpression;

//bounding
MObject DPGroup::aIsBoundingAction;
MObject DPGroup::aBoundingType;
MObject DPGroup::aBoundingSize;
MObject DPGroup::aBoundingCenter;
MObject DPGroup::aInObject;
MObject DPGroup::aInvert;

//normal
MObject DPGroup::aIsNormalAction;
MObject DPGroup::aNormalDirection;
MObject DPGroup::aSpreadAngle;
MObject DPGroup::aNormalAttributeName;

MObject DPGroup::outDP;






void *DPGroup::creator() {
    return ::dommy::maya::createObject("DPGroup", [&]() {
        return new DPGroup();
    });
}

MStatus DPGroup::evalParms(MDataBlock& data, DGroupParams& parms) {

    MStatus stat;

    const MDataHandle aGroupNameHandle = data.inputValue(aGroupName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_grp_name = aGroupNameHandle.asString().asChar();

    const MDataHandle aExistActionTypeHandle = data.inputValue(aExistActionType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_exist_act = aExistActionTypeHandle.asShort();

    const MDataHandle aIsNumberActionHandle = data.inputValue(aIsNumberAction, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isNumeric = aIsNumberActionHandle.asBool();

    const MDataHandle aNumberTypeHandle = data.inputValue(aNumberType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_numeric_type = aNumberTypeHandle.asShort();

    const MDataHandle aPatternHandle = data.inputValue(aPattern, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_pattern_str = aPatternHandle.asString();

    const MDataHandle aStartEndHandle = data.inputValue(aStartEnd, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int2& startEndVal = aStartEndHandle.asInt2();

    parms.m_start = startEndVal[0];
    parms.m_end = startEndVal[1];

    const MDataHandle aSelectOfHandle = data.inputValue(aSelectOf, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int2& selVal = aSelectOfHandle.asInt2();

    parms.m_select = selVal[1];
    parms.m_step = selVal[0];

    const MDataHandle aExpressionHandle = data.inputValue(aExpression, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_expression_str = aExpressionHandle.asString().asChar();

    const MDataHandle aIsBoundingActionHandle = data.inputValue(aIsBoundingAction, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isBound = aIsBoundingActionHandle.asBool();

    const MDataHandle aBoundingTypeHandle = data.inputValue(aBoundingType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.boundingType = aBoundingTypeHandle.asShort();

    const MDataHandle aBoundingCenterHandle = data.inputValue(aBoundingCenter, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const double3& center = aBoundingCenterHandle.asDouble3();
    const MDataHandle aBoundingSizeHandle = data.inputValue(aBoundingSize, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const double3& bSize = aBoundingSizeHandle.asDouble3();
    parms.m_bbox_center = MVector(center);
    parms.m_bbox_size = MVector(bSize);

    if (parms.isBound && parms.boundingType == DGRP_BOUND_OBJECT) {
        MPlug objPlug(thisMObject(), aInObject);
        const bool objectConnected = objPlug.isConnected(&stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        if (!objectConnected) return MS::kInvalidParameter;
        parms.m_objHnd = data.inputValue(aInObject, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
    }

    const MDataHandle aIsNormalActionHandle = data.inputValue(aIsNormalAction, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isNormal = aIsNormalActionHandle.asBool();

    const MDataHandle aNormalDirectionHandle = data.inputValue(aNormalDirection, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const double3& normal = aNormalDirectionHandle.asDouble3();
    parms.m_normal = MVector(normal);

    const MDataHandle aSpreadAngleHandle = data.inputValue(aSpreadAngle, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_spread = aSpreadAngleHandle.asDouble();

    const MDataHandle aNormalAttributeNameHandle = data.inputValue(aNormalAttributeName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_normal_attr = aNormalAttributeNameHandle.asString().asChar();

    const MDataHandle aInvertHandle = data.inputValue(aInvert, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_invert = aInvertHandle.asBool();

    return validateGroupParams(parms, true);
}

MStatus DPGroup::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDP)
        return MS::kUnknownParameter;
    DGroupParams params;
    DOMMY_RETURN_IF_FAILURE(evalParms(data, params));
    const DPData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, input));
    if (!input) return MS::kFailure;
    MTime currentTime;
    if (params.isNumeric && params.m_numeric_type == DGRP_NUM_EXPRESSION)
        DOMMY_RETURN_IF_FAILURE(dommy::maya::getEvaluationTime(data, currentTime));
    MStatus status = dommy::maya::setPluginDataOutput<DPData>(
        data, outDP, DPData::id, [&](DPData& output) -> MStatus {
            output.setDPData(*input);
            const std::size_t count = output.numberOfPoints();
            DPIdList selection(count, false);
            if (params.isNumeric) {
                if (params.m_numeric_type == DGRP_NUM_EXPRESSION) {
                    DPointSeExpr expression(params.m_expression_str);
                    if (!expression.isValid())
                        return MS::kInvalidParameter;
                    DOMMY_RETURN_IF_FAILURE(
                        dommy::maya::setExpressionTime(expression, currentTime));
                    expression.setDPData(&output);
                    const auto& points = *output.getPositionPtrConst();
                    if (!dommy::group::evaluateSelection(
                            count, selection, [&](std::size_t i, bool& selected) {
                                expression.setId(i);
                                expression.setPos(points[i]);
                                SeVec3d value;
                                if (!dommy::expression::evaluateChecked(expression, value))
                                    return false;
                                selected = value[0] > 0.0;
                                return true;
                            }))
                        return MS::kFailure;
                } else {
                    DOMMY_RETURN_IF_FAILURE(generateNumericGroup(params, count, selection));
                }
            }
            if (params.isBound)
                DOMMY_RETURN_IF_FAILURE(
                    generateBoundGroup(params, *output.getPositionPtrConst(), selection));
            if (params.isNormal)
                DOMMY_RETURN_IF_FAILURE(
                    generateAttributeNormalGroup(params, *input, count, selection));
            return dommy::group::publishCombinedGroup(output, params.m_grp_name, count, selection,
                                                      params.m_exist_act, params.m_invert)
                       ? MS::kSuccess
                       : MS::kInvalidParameter;
        });
    if (!status)
        return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DPGroup")

MStatus DPGroup::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    MFnStringData fnStringData;
    const MObject defaultString1 = fnStringData.create("group", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGroupName = tAttr.create("GroupName", "grpn", MFnData::kString, defaultString1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupName));

    MFnEnumAttribute enumExistFn;
    aExistActionType = enumExistFn.create( "ExistActionType", "eat", DGRP_EXT_REPLACE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Replace", DGRP_EXT_REPLACE ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Union", DGRP_EXT_UNION ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Substcut", DGRP_EXT_SUBSTRUCT ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Intersect", DGRP_EXT_INTERSECT ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExistActionType));

    aIsNumberAction = nAttr.create( "IsNumberAction", "isnumact", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsNumberAction));

    aNumberType = enumExistFn.create( "NumberType", "numty", DGRP_NUM_PATTERN, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Pattern", DGRP_NUM_PATTERN ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Range", DGRP_NUM_RANGE ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Expression", DGRP_NUM_EXPRESSION ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNumberType));

    MObject patAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aPattern = tAttr.create("Pattern", "patt", MFnData::kString, patAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aPattern));

    aStartEnd = nAttr.create( "StartEnd", "sten", MFnNumericData::k2Int, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0,1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartEnd));

    aSelectOf = nAttr.create( "SelectOf", "selo", MFnNumericData::k2Int, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1,2));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelectOf));

    MObject exAttrName = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aExpression = tAttr.create("Expression", "expn", MFnData::kString, exAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExpression));

    aIsBoundingAction = nAttr.create( "IsBoundingAction", "isbndact", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsBoundingAction));

    aBoundingType = enumExistFn.create( "BoundingType", "bndty", DGRP_BOUND_BOX, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Box", DGRP_BOUND_BOX ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Sphere", DGRP_BOUND_SPHERE ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Input Object", DGRP_BOUND_OBJECT));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBoundingType));

    aBoundingSize = nAttr.create( "BoundingSize", "bnds", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0,1.0,1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBoundingSize));

    aBoundingCenter = nAttr.create( "BoundingCenter", "bndc", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBoundingCenter));

    aInObject= tAttr.create( "InObject", "ino",MFnData::kMesh, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInObject));

    aInvert = nAttr.create( "Invert", "inv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));

    aIsNormalAction = nAttr.create( "IsNormalAction", "isnormact", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsNormalAction));

    aNormalDirection = nAttr.create( "NormalDirection", "normd", MFnNumericData::k3Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,1.0,0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalDirection));

    aSpreadAngle = nAttr.create( "SpreadAngle", "spagl", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(90.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMax(180.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSpreadAngle));

    MObject nAttrName = fnStringData.create("N", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aNormalAttributeName = tAttr.create("NormalAttributeName", "nattrn", MFnData::kString, nAttrName, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalAttributeName));

    //create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDP, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGroupName, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExistActionType, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsNumberAction, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNumberType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPattern, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aStartEnd, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSelectOf, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExpression, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsBoundingAction, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingType, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingSize, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingCenter, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObject, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInvert, outDP ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsNormalAction, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalDirection, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpreadAngle, outDP ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalAttributeName, outDP ));

    DOMMY_RETURN_IF_FAILURE(DPGroupManip::addToManipConnectTable(typeId));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DPGroup", "initialize", std::current_exception());
}
