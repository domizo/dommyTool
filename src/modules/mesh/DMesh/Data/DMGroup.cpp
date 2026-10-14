//
//  DMGroup.cpp
//  dommyTool
//
//  Created by yuzo on 4/29/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DMGroup.h"
#include "common/utility/DGroupEvaluation.h"
#include "common/utility/DGroupPublication.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaMeshDataOutput.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDependencyNodeValues.h"
#include "common/utility/DLegacyDrawGuard.h"


#include "DMayaHeaders.h"

#include "DMeshData.h"
#include "DMeshUtil.h"
#include "dmesh_ids.h"

#include "DNodeOperation.h"

#include "dommyMath.h"

#pragma mark DMGroupManip



MManipData DMGroupManip::plugToManipConversion(unsigned int)
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


MVector DMGroupManip::manipulatorOrigin() const
{
		MVector center;
		return dommy::maya::readDependencyVector(
				   nodeObj, "BoundingCenter", center)
				   ? center
				   : MVector::zero;
}

MStatus DMGroupManip::getBBoxSize(
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

MTypeId DMGroupManip::id( ID_DM_GROUP_MANIP );


void* DMGroupManip::creator()
{
    return ::dommy::maya::createObject("DMGroupManip", [&]() {
        return new DMGroupManip();
    });
}

MStatus DMGroupManip::initialize()
{
	return MPxManipContainer::initialize();
}

MStatus DMGroupManip::createChildren()
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




MStatus DMGroupManip::connectToDependNode(const MObject &node)
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
		status = stateManip.setTranslation(
			MVector(2.0, 0.0, 0.0), MSpace::kObject);
		if (!status) return status;
		const unsigned int positionIndex = stateManip.positionIndex(&status);
		if (!status) return status;
		addPlugToManipConversion(positionIndex);

		status = finishAddingManips();
		if (!status) return status;
		return MPxManipContainer::connectToDependNode(node);
}

void DMGroupManip::draw(M3dView & view, const MDagPath &path,
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
DOMMY_DRAW_CATCH("DMGroupManip")

// Viewport 2.0 manipulator draw overrides
void DMGroupManip::preDrawUI(const M3dView&)
{
		fTextPosition = manipulatorOrigin();
		if (!getBBoxSize(fBBoxSize, fBBoxCenter, fBBoxType)) {
			fBBoxSize = MVector::zero;
			fBBoxCenter = MVector::zero;
			fBBoxType = DGRP_BOUND_BOX;
		}
}

void DMGroupManip::drawUI(MHWRender::MUIDrawManager& drawManager,
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




#pragma mark DMGroup

MTypeId DMGroup::typeId( ID_DM_GROUP_CREATE );

MObject DMGroup::inDM;

MObject DMGroup::aGroupName;
MObject DMGroup::aGroupType;
MObject DMGroup::aExistActionType;

//number
MObject DMGroup::aIsNumberAction;
MObject DMGroup::aNumberType;
MObject DMGroup::aPattern;
MObject DMGroup::aStartEnd;
MObject DMGroup::aSelectOf;
MObject DMGroup::aExpression;

//bounding
MObject DMGroup::aIsBoundingAction;
MObject DMGroup::aBoundingType;
MObject DMGroup::aBoundingSize;
MObject DMGroup::aBoundingCenter;
MObject DMGroup::aInObject;
MObject DMGroup::aInvert;



//normal
MObject DMGroup::aIsNormalAction;
MObject DMGroup::aNormalDirection;
MObject DMGroup::aSpreadAngle;
MObject DMGroup::aNormalAttributeName;

//face
MObject DMGroup::aIsFaceArea;
MObject DMGroup::aFaceAreaRange;
MObject DMGroup::aIsVertexCount;
MObject DMGroup::aVertexCountRange;
MObject DMGroup::aIsConvex;
MObject DMGroup::aIsPlanar;
MObject DMGroup::aIsBoundry;

//edge
MObject DMGroup::aIsEdgeAngle;
MObject DMGroup::aEdgeAngleRange;
MObject DMGroup::aIsEdgeLength;
MObject DMGroup::aEdgeLengthRange;
MObject DMGroup::aIsEdgeBoundry;

MObject DMGroup::outDM;




void *DMGroup::creator() {
    return ::dommy::maya::createObject("DMGroup", [&]() {
        return new DMGroup();
    });
}

MStatus DMGroup::evalParms(MDataBlock& data, DGroupParams& parms) {

    MStatus stat;

    const MDataHandle aGroupTypeHandle = data.inputValue(aGroupType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_grp_type = aGroupTypeHandle.asShort();

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

        parms.m_objHnd = data.inputValue(aInObject, &stat);
        if (!stat)
            return stat;
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

    const MDataHandle aIsFaceAreaHandle = data.inputValue(aIsFaceArea, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isFaceArea = aIsFaceAreaHandle.asBool();
    const MDataHandle aFaceAreaRangeHandle = data.inputValue(aFaceAreaRange, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const double2& faceAreaRange = aFaceAreaRangeHandle.asDouble2();

    if (faceAreaRange[0] > faceAreaRange[1]) {
        parms.faceAreaMin = faceAreaRange[1];
        parms.faceAreaMax = faceAreaRange[0];
    } else {
        parms.faceAreaMin = faceAreaRange[0];
        parms.faceAreaMax = faceAreaRange[1];
    }

    const MDataHandle aIsVertexCountHandle = data.inputValue(aIsVertexCount, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isFaceVtxCount = aIsVertexCountHandle.asBool();
    const MDataHandle aVertexCountRangeHandle = data.inputValue(aVertexCountRange, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const int2& vtxCountRange = aVertexCountRangeHandle.asInt2();

    if (vtxCountRange[0] > vtxCountRange[1]) {
        parms.faceVtxMin = vtxCountRange[1];
        parms.faceVtxMax = vtxCountRange[0];
    } else {
        parms.faceVtxMin = vtxCountRange[0];
        parms.faceVtxMax = vtxCountRange[1];
    }

    const MDataHandle aIsConvexHandle = data.inputValue(aIsConvex, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isConvex = aIsConvexHandle.asBool();
    const MDataHandle aIsPlanarHandle = data.inputValue(aIsPlanar, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isPlanar = aIsPlanarHandle.asBool();
    const MDataHandle aIsBoundryHandle = data.inputValue(aIsBoundry, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isFaceBoundry = aIsBoundryHandle.asBool();

    const MDataHandle aIsEdgeAngleHandle = data.inputValue(aIsEdgeAngle, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isEdgeAngle = aIsEdgeAngleHandle.asBool();
    const MDataHandle aEdgeAngleRangeHandle = data.inputValue(aEdgeAngleRange, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const double2& edgeAngleRange = aEdgeAngleRangeHandle.asDouble2();
    if (edgeAngleRange[0] > edgeAngleRange[1]) {
        parms.edgeAngleMin = dommy::math::degreesToRadians(edgeAngleRange[1]);
        parms.edgeAngleMax = dommy::math::degreesToRadians(edgeAngleRange[0]);
    } else {
        parms.edgeAngleMin = dommy::math::degreesToRadians(edgeAngleRange[0]);
        parms.edgeAngleMax = dommy::math::degreesToRadians(edgeAngleRange[1]);
    }

    const MDataHandle aIsEdgeLengthHandle = data.inputValue(aIsEdgeLength, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isEdgeLength = aIsEdgeLengthHandle.asBool();
    const MDataHandle aEdgeLengthRangeHandle = data.inputValue(aEdgeLengthRange, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    const double2& edgeLengthRange = aEdgeLengthRangeHandle.asDouble2();
    if (edgeLengthRange[0] > edgeLengthRange[1]) {
        parms.edgeLengthMin = edgeLengthRange[1];
        parms.edgeLengthMax = edgeLengthRange[0];
    } else {
        parms.edgeLengthMin = edgeLengthRange[0];
        parms.edgeLengthMax = edgeLengthRange[1];
    }

    const MDataHandle aIsEdgeBoundryHandle = data.inputValue(aIsEdgeBoundry, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isEdgeBoundry = aIsEdgeBoundryHandle.asBool();

    const MDataHandle aInvertHandle = data.inputValue(aInvert, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_invert = aInvertHandle.asBool();

    if (parms.m_grp_type != D_COMP_PRIM && parms.m_grp_type != D_COMP_POINT &&
        parms.m_grp_type != D_COMP_FACE && parms.m_grp_type != D_COMP_EDGE)
        return MS::kInvalidParameter;
    if ((parms.isFaceArea && (!std::isfinite(parms.faceAreaMin) ||
                              !std::isfinite(parms.faceAreaMax) || parms.faceAreaMin < 0.0)) ||
        (parms.isEdgeAngle && (!std::isfinite(parms.edgeAngleMin) ||
                               !std::isfinite(parms.edgeAngleMax) || parms.edgeAngleMin < 0.0)) ||
        (parms.isEdgeLength && (!std::isfinite(parms.edgeLengthMin) ||
                                !std::isfinite(parms.edgeLengthMax) || parms.edgeLengthMin < 0.0)))
        return MS::kInvalidParameter;
    return validateGroupParams(parms, false);
}

MStatus DMGroup::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDM)
        return MS::kUnknownParameter;
    DGroupParams params;
    DOMMY_RETURN_IF_FAILURE(evalParms(data, params));
    const DMeshData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDM(inDM, data, input));
    if (!input) return MS::kFailure;
    MStatus status = dommy::maya::setPluginDataOutput<
        DMeshData>(data, outDM, DMeshData::id, [&](DMeshData& output) -> MStatus {
        DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input));
        DMeshData* outDMeshes = &output;
        MStatus stat;
        const size_t meshCount = output.numberOfMesh();
        if (meshCount || params.m_grp_type == D_COMP_PRIM) {

            if (params.m_grp_type == D_COMP_PRIM) {

                DPIdList newGrpIds(meshCount, false);

                if (params.isNumeric) {

                    DOMMY_RETURN_IF_FAILURE(generateNumericGroup(params, meshCount, newGrpIds));
                }

                if (params.isBound) {

                    DPointList primCenterList(meshCount);
                    size_t n = 0;
                    for (auto mIt = outDMeshes->beginMeshes(); mIt != outDMeshes->endMeshes();
                         ++mIt) {
                        DPoint c = (*mIt)->getCenter();
                        primCenterList[n++] = c;
                    }

                    DOMMY_RETURN_IF_FAILURE(generateBoundGroup(params, primCenterList, newGrpIds));
                }

                if (params.isNormal) {
                    DOMMY_RETURN_IF_FAILURE(
                        generateAttributeNormalGroup(params, *outDMeshes, meshCount, newGrpIds));
                }

                if (!dommy::group::publishCombinedGroup(*outDMeshes, params.m_grp_name, meshCount,
                                                        newGrpIds, params.m_exist_act,
                                                        params.m_invert))
                    return MS::kInvalidParameter;

            } else if (params.m_grp_type == D_COMP_POINT) {

                for (auto mIt = outDMeshes->beginMeshes(); mIt != outDMeshes->endMeshes(); ++mIt) {

                    size_t ptCount = (*mIt)->numberOfPoints();

                    DPIdList newGrpIds(ptCount, false);

                    if (params.isNumeric) {

                        DOMMY_RETURN_IF_FAILURE(generateNumericGroup(params, ptCount, newGrpIds));
                    }

                    if (params.isBound) {

                        const DPointList* ptsPtr = (*mIt)->getPositionPtrConst();
                        DOMMY_RETURN_IF_FAILURE(generateBoundGroup(params, *ptsPtr, newGrpIds));
                    }

                    if (params.isNormal) {
                        DOMMY_RETURN_IF_FAILURE(
                            generateAttributeNormalGroup(params, **mIt, ptCount, newGrpIds));
                    }

                    if (!dommy::group::publishCombinedGroup(**mIt, params.m_grp_name, ptCount,
                                                            newGrpIds, params.m_exist_act,
                                                            params.m_invert))
                        return MS::kInvalidParameter;
                }

            } else if (params.m_grp_type == D_COMP_FACE) {

                for (auto mIt = outDMeshes->beginMeshes(); mIt != outDMeshes->endMeshes(); ++mIt) {

                    if ((*mIt)->numFace() == 0) {
                        if (!(*mIt)->combineCompGrp(D_COMP_FACE, params.m_grp_name, 0U, DPIdList(),
                                                    params.m_exist_act, params.m_invert))
                            return MS::kInvalidParameter;
                        continue;
                    }
                    MFnMesh meshFn;
                    MObject mObj;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::createMeshDataObject(mObj));

                    const MObject geometry = meshFn.create((*mIt)->numberOfPoints(), (*mIt)->numFace(),
                                  (*mIt)->getMPosition(), (*mIt)->getFaceList(),
                                  (*mIt)->getConnectList(), mObj, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    if (geometry.isNull()) return MS::kFailure;

                    const int rawFaceCount = meshFn.numPolygons(&stat);
                    if (!stat)
                        return stat;
                    if (rawFaceCount < 0)
                        return MS::kInvalidParameter;
                    const size_t faceCount = static_cast<size_t>(rawFaceCount);

                    if (faceCount > dommy::generation::kMaxGeneratedPointCount)
                        return MS::kInvalidParameter;
                    DPIdList newGrpIds(faceCount, false);

                    if (params.isNumeric) {

                        DOMMY_RETURN_IF_FAILURE(generateNumericGroup(params, faceCount, newGrpIds));
                    }

                    MItMeshPolygon pIt(mObj, &stat);
                    if (!stat)
                        return stat;

                    if (params.isBound) {

                        DPointList faceCenterList(faceCount);
                        for (stat = pIt.reset(); stat && !pIt.isDone(&stat); stat = pIt.next()) {
                            if (!stat)
                                return stat;
                            const int rawIndex = pIt.index(&stat);
                            if (!stat)
                                return stat;
                            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= faceCount)
                                return MS::kInvalidParameter;
                            const size_t componentIndex = static_cast<size_t>(rawIndex);

                            MPoint center = pIt.center(MSpace::kObject, &stat);
                            if (!stat)
                                return stat;
                            if (!dommy::maya::isFinitePoint(center))
                                return MS::kInvalidParameter;
                            faceCenterList[componentIndex] =
                                DPoint(center[0], center[1], center[2]);

                            if (!stat)
                                return stat;
                        }
                        if (!stat)
                            return stat;

                        DOMMY_RETURN_IF_FAILURE(
                            generateBoundGroup(params, faceCenterList, newGrpIds));
                    }

                    if (params.isNormal) {

                        if (!dommy::attribute::validArrayConversionBudget(faceCount,
                                                                          sizeof(Imath::V3d)))
                            return MS::kInvalidParameter;
                        std::vector<Imath::V3d> vectorArray(faceCount);
                        for (stat = pIt.reset(); stat && !pIt.isDone(&stat); stat = pIt.next()) {
                            if (!stat)
                                return stat;
                            const int rawIndex = pIt.index(&stat);
                            if (!stat)
                                return stat;
                            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= faceCount)
                                return MS::kInvalidParameter;
                            const size_t componentIndex = static_cast<size_t>(rawIndex);

                            MVector norm;
                            DOMMY_RETURN_IF_FAILURE(pIt.getNormal(norm));
                            vectorArray[componentIndex] = Imath::V3d(norm[0], norm[1], norm[2]);

                            if (!stat)
                                return stat;
                        }
                        if (!stat)
                            return stat;

                        DOMMY_RETURN_IF_FAILURE(
                            generateNormalGroup(params, vectorArray, newGrpIds));
                    }

                    if (params.isFaceArea || params.isFaceVtxCount || params.isConvex ||
                        params.isPlanar || params.isFaceBoundry) {

                        for (stat = pIt.reset(); stat && !pIt.isDone(&stat); stat = pIt.next()) {
                            if (!stat)
                                return stat;
                            const int rawIndex = pIt.index(&stat);
                            if (!stat)
                                return stat;
                            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= faceCount)
                                return MS::kInvalidParameter;
                            const size_t componentIndex = static_cast<size_t>(rawIndex);

                            if (params.isFaceArea) {
                                double area = 0.0;
                                DOMMY_RETURN_IF_FAILURE(pIt.getArea(area));
                                if (!std::isfinite(area) || area < 0.0)
                                    return MS::kInvalidParameter;

                                if (params.faceAreaMin <= area && params.faceAreaMax >= area) {
                                    newGrpIds[componentIndex] = true;
                                    continue;
                                }
                            }

                            if (params.isFaceVtxCount) {
                                const unsigned int vertexCount = pIt.polygonVertexCount(&stat);
                                if (!stat)
                                    return stat;
                                const unsigned int minimumCount =
                                    params.faceVtxMin > 0
                                        ? static_cast<unsigned int>(params.faceVtxMin)
                                        : 0U;
                                const unsigned int maximumCount =
                                    params.faceVtxMax > 0
                                        ? static_cast<unsigned int>(params.faceVtxMax)
                                        : 0U;
                                if (vertexCount >= minimumCount && vertexCount <= maximumCount) {
                                    newGrpIds[componentIndex] = true;
                                    continue;
                                }
                            }

                            if (params.isConvex) {
                                const bool matched = pIt.isConvex(&stat);
                                if (!stat)
                                    return stat;
                                if (matched) {
                                    newGrpIds[componentIndex] = true;
                                    continue;
                                }
                            }

                            if (params.isPlanar) {
                                const bool matched = pIt.isPlanar(&stat);
                                if (!stat)
                                    return stat;
                                if (matched) {
                                    newGrpIds[componentIndex] = true;
                                    continue;
                                }
                            }

                            if (params.isFaceBoundry) {
                                const bool matched = pIt.onBoundary(&stat);
                                if (!stat)
                                    return stat;
                                if (matched) {
                                    newGrpIds[componentIndex] = true;
                                    continue;
                                }
                            }

                            if (!stat)
                                return stat;
                        }
                        if (!stat)
                            return stat;
                    }

                    if (!(*mIt)->combineCompGrp(D_COMP_FACE, params.m_grp_name, faceCount,
                                                newGrpIds, params.m_exist_act, params.m_invert))
                        return MS::kInvalidParameter;
                }

            } else if (params.m_grp_type == D_COMP_EDGE) {

                for (auto mIt = outDMeshes->beginMeshes(); mIt != outDMeshes->endMeshes(); ++mIt) {

                    if ((*mIt)->numFace() == 0) {
                        if (!(*mIt)->combineCompGrp(D_COMP_EDGE, params.m_grp_name, 0U, DPIdList(),
                                                    params.m_exist_act, params.m_invert))
                            return MS::kInvalidParameter;
                        continue;
                    }
                    MFnMesh meshFn;
                    MObject mObj;
                    DOMMY_RETURN_IF_FAILURE(dommy::maya::createMeshDataObject(mObj));

                    const MObject geometry = meshFn.create((*mIt)->numberOfPoints(), (*mIt)->numFace(),
                                  (*mIt)->getMPosition(), (*mIt)->getFaceList(),
                                  (*mIt)->getConnectList(), mObj, &stat);
                    CHECK_MSTATUS_AND_RETURN_IT(stat);
                    if (geometry.isNull()) return MS::kFailure;

                    const int rawEdgeCount = meshFn.numEdges(&stat);
                    if (!stat)
                        return stat;
                    if (rawEdgeCount < 0)
                        return MS::kInvalidParameter;
                    const size_t edgeCount = static_cast<size_t>(rawEdgeCount);

                    if (edgeCount > dommy::generation::kMaxGeneratedPointCount)
                        return MS::kInvalidParameter;
                    DPIdList newGrpIds(edgeCount, false);

                    if (params.isNumeric) {

                        DOMMY_RETURN_IF_FAILURE(generateNumericGroup(params, edgeCount, newGrpIds));
                    }

                    MItMeshEdge eIt(mObj, &stat);
                    if (!stat)
                        return stat;

                    if (params.isBound) {

                        DPointList faceCenterList(edgeCount);
                        for (stat = eIt.reset(); stat && !eIt.isDone(&stat); stat = eIt.next()) {
                            if (!stat)
                                return stat;
                            const int rawIndex = eIt.index(&stat);
                            if (!stat)
                                return stat;
                            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= edgeCount)
                                return MS::kInvalidParameter;
                            const size_t componentIndex = static_cast<size_t>(rawIndex);

                            MPoint center = eIt.center(MSpace::kObject, &stat);
                            if (!stat)
                                return stat;
                            if (!dommy::maya::isFinitePoint(center))
                                return MS::kInvalidParameter;
                            faceCenterList[componentIndex] =
                                DPoint(center[0], center[1], center[2]);

                            if (!stat)
                                return stat;
                        }
                        if (!stat)
                            return stat;

                        DOMMY_RETURN_IF_FAILURE(
                            generateBoundGroup(params, faceCenterList, newGrpIds));
                    }

                    if (params.isNormal) {

                        if (!dommy::attribute::validArrayConversionBudget(edgeCount,
                                                                          sizeof(Imath::V3d)))
                            return MS::kInvalidParameter;
                        std::vector<Imath::V3d> vectorArray(edgeCount);
                        for (stat = eIt.reset(); stat && !eIt.isDone(&stat); stat = eIt.next()) {
                            if (!stat)
                                return stat;
                            const int rawIndex = eIt.index(&stat);
                            if (!stat)
                                return stat;
                            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= edgeCount)
                                return MS::kInvalidParameter;
                            const size_t componentIndex = static_cast<size_t>(rawIndex);

                            MIntArray faceIds;
                            eIt.getConnectedFaces(faceIds, &stat);
                            if (!stat)
                                return stat;

                            MVector totalvec(0, 0, 0);

                            for (unsigned int i = 0U; i < faceIds.length(); ++i) {
                                MVector tmpNormVec;
                                DOMMY_RETURN_IF_FAILURE(
                                    meshFn.getPolygonNormal(faceIds[i], tmpNormVec));
                                totalvec += tmpNormVec;
                            }

                            MVector normalized;
                            if (!dommy::maya::normalizedVectorOrZero(totalvec, normalized))
                                return MS::kInvalidParameter;
                            totalvec = normalized;

                            vectorArray[componentIndex] =
                                Imath::V3d(totalvec[0], totalvec[1], totalvec[2]);

                            if (!stat)
                                return stat;
                        }
                        if (!stat)
                            return stat;

                        DOMMY_RETURN_IF_FAILURE(
                            generateNormalGroup(params, vectorArray, newGrpIds));
                    }

                    if (params.isEdgeAngle || params.isEdgeLength || params.isEdgeBoundry) {

                        for (stat = eIt.reset(); stat && !eIt.isDone(&stat); stat = eIt.next()) {
                            if (!stat)
                                return stat;
                            const int rawIndex = eIt.index(&stat);
                            if (!stat)
                                return stat;
                            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= edgeCount)
                                return MS::kInvalidParameter;
                            const size_t componentIndex = static_cast<size_t>(rawIndex);

                            if (params.isEdgeAngle) {

                                MIntArray faceIds;
                                eIt.getConnectedFaces(faceIds, &stat);
                                if (!stat)
                                    return stat;

                                if (faceIds.length() == 2) {
                                    MVector vecA, vecB;
                                    DOMMY_RETURN_IF_FAILURE(
                                        meshFn.getPolygonNormal(faceIds[0], vecA));
                                    DOMMY_RETURN_IF_FAILURE(
                                        meshFn.getPolygonNormal(faceIds[1], vecB));

                                    if (!dommy::maya::isFiniteVector(vecA) ||
                                        !dommy::maya::isFiniteVector(vecB))
                                        return MS::kInvalidParameter;
                                    const double angle = vecA.angle(vecB);
                                    if (!std::isfinite(angle))
                                        return MS::kInvalidParameter;
                                    if (params.edgeAngleMin <= angle &&
                                        params.edgeAngleMax >= angle) {
                                        newGrpIds[componentIndex] = true;
                                        continue;
                                    }
                                }
                            }

                            if (params.isEdgeLength) {

                                double length = 0.0;
                                DOMMY_RETURN_IF_FAILURE(eIt.getLength(length));
                                if (!std::isfinite(length) || length < 0.0)
                                    return MS::kInvalidParameter;

                                if (length >= params.edgeLengthMin &&
                                    length <= params.edgeLengthMax) {
                                    newGrpIds[componentIndex] = true;
                                    continue;
                                }
                            }

                            if (params.isEdgeBoundry) {
                                const bool matched = eIt.onBoundary(&stat);
                                if (!stat)
                                    return stat;
                                if (matched) {
                                    newGrpIds[componentIndex] = true;
                                    continue;
                                }
                            }

                            if (!stat)
                                return stat;
                        }
                        if (!stat)
                            return stat;
                    }

                    if (!(*mIt)->combineCompGrp(D_COMP_EDGE, params.m_grp_name, edgeCount,
                                                newGrpIds, params.m_exist_act, params.m_invert))
                        return MS::kInvalidParameter;
                }
            }
        }
        return MS::kSuccess;
    });
    if (!status)
        return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DMGroup")




MStatus DMGroup::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDM = tAttr.create("inDM", "idm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDM));

    const MObject defaultString1 = fnStringData.create("", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aGroupName = tAttr.create("GroupName", "grpn", MFnData::kString, defaultString1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupName));

    MFnEnumAttribute grpTypeEnumFn;
    aGroupType = grpTypeEnumFn.create( "GroupType", "grpty", D_COMP_PRIM, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(grpTypeEnumFn.addField( "Primitive", D_COMP_PRIM ));
    DOMMY_RETURN_IF_FAILURE(grpTypeEnumFn.addField( "point", D_COMP_POINT ));
    DOMMY_RETURN_IF_FAILURE(grpTypeEnumFn.addField( "face", D_COMP_FACE ));
    DOMMY_RETURN_IF_FAILURE(grpTypeEnumFn.addField( "edge", D_COMP_EDGE ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGroupType));

    MFnEnumAttribute enumExistFn;
    aExistActionType = enumExistFn.create( "ExistActionType", "eat", DGRP_EXT_REPLACE, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Replace", DGRP_EXT_REPLACE ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Union", DGRP_EXT_UNION ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Substcut", DGRP_EXT_SUBSTRUCT ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Intersect", DGRP_EXT_INTERSECT ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aExistActionType));

    //////////////////////////////////Numeric Action//////////////////////////////////

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

    //////////////////////////////////Bound Action//////////////////////////////////

    aIsBoundingAction = nAttr.create( "IsBoundingAction", "isbndact", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsBoundingAction));

    aBoundingType = enumExistFn.create( "BoundingType", "bndty", DGRP_BOUND_BOX, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Box", DGRP_BOUND_BOX ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Sphere", DGRP_BOUND_SPHERE ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Input Object", DGRP_BOUND_OBJECT ));
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

    //////////////////////////////////Normal Action//////////////////////////////////

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

    //////////////////////////////////Face Action//////////////////////////////////

    aIsFaceArea = nAttr.create( "IsFaceArea", "ifaar", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsFaceArea));

    aFaceAreaRange = nAttr.create( "FaceAreaRange", "faarrg", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFaceAreaRange));

    aIsVertexCount = nAttr.create( "IsVertexCount", "ivcnt", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsVertexCount));

    aVertexCountRange = nAttr.create( "VertexCountRange", "vtxcntrg", MFnNumericData::k2Int, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0, 3));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVertexCountRange));

    aIsConvex = nAttr.create( "IsConvex", "iconv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsConvex));

    aIsPlanar = nAttr.create( "IsPlanar", "ipln", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsPlanar));

    aIsBoundry = nAttr.create( "IsBoundry", "ibnd", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsBoundry));

    //////////////////////////////////Edge Action//////////////////////////////////

    aIsEdgeAngle = nAttr.create( "IsEdgeAngle", "ieagl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEdgeAngle));

    aEdgeAngleRange = nAttr.create( "EdgeAngleRange", "eaglrng", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(80.0, 100.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeAngleRange));

    aIsEdgeLength = nAttr.create( "IsEdgeLength", "ielen", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEdgeLength));

    aEdgeLengthRange = nAttr.create( "EdgeLengthRange", "elenrng", MFnNumericData::k2Double, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0, 1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEdgeLengthRange));

    aIsEdgeBoundry = nAttr.create( "IsEdgeBoundry", "iebnd", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEdgeBoundry));

    //////////////////////////////////Common//////////////////////////////////

    aInvert = nAttr.create( "Invert", "inv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));

    //create custom data
    outDM = tAttr.create("outDM", "odm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDM));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDM, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGroupName, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGroupType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExistActionType, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsNumberAction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNumberType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPattern, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aStartEnd, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSelectOf, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExpression, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsBoundingAction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingType, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingSize, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingCenter, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObject, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInvert, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsNormalAction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalDirection, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpreadAngle, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalAttributeName, outDM ));

//	DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsFaceAction, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsFaceArea, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aFaceAreaRange, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsVertexCount, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aVertexCountRange, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsConvex, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsPlanar, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsBoundry, outDM ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsEdgeAngle, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aEdgeAngleRange, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsEdgeLength, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aEdgeLengthRange, outDM ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsEdgeBoundry, outDM ));

    DOMMY_RETURN_IF_FAILURE(DMGroupManip::addToManipConnectTable(typeId));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DMGroup", "initialize", std::current_exception());
}
