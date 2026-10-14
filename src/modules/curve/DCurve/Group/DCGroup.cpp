//
//  DCGroup.cpp
//  dommyTool
//
//  Created by yuzo on 2017/08/10.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DCGroup.h"
#include "common/utility/DGroupEvaluation.h"
#include "common/utility/DGroupPublication.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DDependencyNodeValues.h"
#include "common/utility/DLegacyDrawGuard.h"




#include "DMayaHeaders.h"

#include "DCurveData.h"
#include "DCurveUtil.h"

#include "DNodeOperation.h"

#include "dommyMath.h"



#pragma mark DCGroupManip



MManipData DCGroupManip::plugToManipConversion(unsigned int)
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


MVector DCGroupManip::manipulatorOrigin() const
{
	    MVector center;
	    return dommy::maya::readDependencyVector(
	               nodeObj, "BoundingCenter", center)
	               ? center
	               : MVector::zero;
}

MStatus DCGroupManip::getBBoxSize(
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

MTypeId DCGroupManip::id( ID_DC_GROUP_MANIP );


void* DCGroupManip::creator()
{
    return ::dommy::maya::createObject("DCGroupManip", [&]() {
        return new DCGroupManip();
    });
}

MStatus DCGroupManip::initialize()
{
    return MPxManipContainer::initialize();
}

MStatus DCGroupManip::createChildren()
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




MStatus DCGroupManip::connectToDependNode(const MObject &node)
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

void DCGroupManip::draw(M3dView & view, const MDagPath &path,
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
DOMMY_DRAW_CATCH("DCGroupManip")

// Viewport 2.0 manipulator draw overrides
void DCGroupManip::preDrawUI(const M3dView&)
{
	    fTextPosition = manipulatorOrigin();
	    if (!getBBoxSize(fBBoxSize, fBBoxCenter, fBBoxType)) {
	        fBBoxSize = MVector::zero;
	        fBBoxCenter = MVector::zero;
	        fBBoxType = DGRP_BOUND_BOX;
	    }
}

void DCGroupManip::drawUI(MHWRender::MUIDrawManager& drawManager,
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


#pragma mark DCGroup



MTypeId DCGroup::typeId( ID_DC_GROUP_CREATE );

MObject DCGroup::inDC;

MObject DCGroup::aGroupName;
MObject DCGroup::aGroupType;
MObject DCGroup::aExistActionType;

//number
MObject DCGroup::aIsNumberAction;
MObject DCGroup::aNumberType;
MObject DCGroup::aPattern;
MObject DCGroup::aStartEnd;
MObject DCGroup::aSelectOf;
MObject DCGroup::aExpression;

//bounding
MObject DCGroup::aIsBoundingAction;
MObject DCGroup::aBoundingType;
MObject DCGroup::aBoundingSize;
MObject DCGroup::aBoundingCenter;
MObject DCGroup::aInObject;
MObject DCGroup::aInvert;

//normal
MObject DCGroup::aIsNormalAction;
MObject DCGroup::aNormalType;
MObject DCGroup::aNormalDirection;
MObject DCGroup::aSpreadAngle;
MObject DCGroup::aNormalAttributeName;

//curve
MObject DCGroup::aIsCurveLength;
MObject DCGroup::aCurveLength;
MObject DCGroup::aIsArea;
MObject DCGroup::aCurveArea;
MObject DCGroup::aIsPlanar;



MObject DCGroup::outDC;




void *DCGroup::creator() {
    return ::dommy::maya::createObject("DCGroup", [&]() {
        return new DCGroup();
    });
}

MStatus DCGroup::evalParms(MDataBlock& data, DGroupParams& parms) {

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

    const MDataHandle aNormalTypeHandle = data.inputValue(aNormalType, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_normalType = aNormalTypeHandle.asShort();

    const MDataHandle aIsCurveLengthHandle = data.inputValue(aIsCurveLength, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isCurveLength = aIsCurveLengthHandle.asBool();
    const MDataHandle aCurveLengthHandle = data.inputValue(aCurveLength, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.curveLength = aCurveLengthHandle.asDouble();
    const MDataHandle aIsAreaHandle = data.inputValue(aIsArea, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isCurveArea = aIsAreaHandle.asBool();
    const MDataHandle aCurveAreaHandle = data.inputValue(aCurveArea, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.curveArea = aCurveAreaHandle.asDouble();
    const MDataHandle aIsPlanarHandle = data.inputValue(aIsPlanar, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.isPlanar = aIsPlanarHandle.asBool();

    const MDataHandle aInvertHandle = data.inputValue(aInvert, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    parms.m_invert = aInvertHandle.asBool();

    if (parms.m_grp_type != D_COMP_PRIM && parms.m_grp_type != D_COMP_POINT)
        return MS::kInvalidParameter;
    if (parms.isNormal &&
        (parms.m_grp_type == D_COMP_PRIM ||
         (parms.m_normalType != DC_VEC_kTangent && parms.m_normalType != DC_VEC_kNormal &&
          parms.m_normalType != DC_VEC_kAttribute)))
        return MS::kInvalidParameter;
    if ((parms.isCurveLength && (!std::isfinite(parms.curveLength) || parms.curveLength < 0.0)) ||
        (parms.isCurveArea && (!std::isfinite(parms.curveArea) || parms.curveArea < 0.0)) ||
        (parms.m_grp_type == D_COMP_POINT &&
         (parms.isCurveLength || parms.isCurveArea || parms.isPlanar)))
        return MS::kInvalidParameter;
    return validateGroupParams(parms, false);
}

MStatus DCGroup::compute(const MPlug& plug, MDataBlock& data) try {
    if (plug != outDC)
        return MS::kUnknownParameter;
    DGroupParams params;
    DOMMY_RETURN_IF_FAILURE(evalParms(data, params));
    const DCurveData* input = nullptr;
    DOMMY_RETURN_IF_FAILURE(getValidInputDC(inDC, data, input));
    if (!input) return MS::kFailure;
    MStatus status = dommy::maya::setPluginDataOutput<DCurveData>(
        data, outDC, DCurveData::id, [&](DCurveData& output) -> MStatus {
            DOMMY_RETURN_IF_FAILURE(output.copyFrom(*input));
            if (params.m_grp_type == D_COMP_PRIM) {
                const std::size_t count = output.numberOfCurves();
                DPIdList selection(count, false);
                if (params.isNumeric)
                    DOMMY_RETURN_IF_FAILURE(generateNumericGroup(params, count, selection));
                if (params.isBound) {
                    DPointList centers;
                    centers.reserve(count);
                    for (auto it = output.beginCurves(); it != output.endCurves(); ++it) {
                        if (!*it)
                            return MS::kInvalidParameter;
                        centers.push_back((*it)->getCenter());
                    }
                    DOMMY_RETURN_IF_FAILURE(generateBoundGroup(params, centers, selection));
                }
                if (params.isCurveLength || params.isCurveArea || params.isPlanar) {
                    std::size_t index = 0U;
                    for (auto it = output.beginCurves(); it != output.endCurves(); ++it, ++index) {
                        MObject object;
                        DOMMY_RETURN_IF_FAILURE(generateMCurveFromDcurve(*it, object));
                        MStatus queryStatus;
                        MFnNurbsCurve curve(object, &queryStatus);
                        if (!queryStatus)
                            return queryStatus;
                        if (params.isCurveLength) {
                            const double length = curve.length(kMFnNurbsEpsilon, &queryStatus);
                            if (!queryStatus)
                                return queryStatus;
                            if (!std::isfinite(length) || length < 0.0)
                                return MS::kInvalidParameter;
                            selection[index] = selection[index] || length <= params.curveLength;
                        }
                        if (params.isCurveArea) {
                            const double area = curve.area(kMFnNurbsEpsilon, &queryStatus);
                            if (!queryStatus)
                                return queryStatus;
                            if (!std::isfinite(area) || area < 0.0)
                                return MS::kInvalidParameter;
                            selection[index] = selection[index] || area <= params.curveArea;
                        }
                        if (params.isPlanar) {
                            const bool planar = curve.isPlanar(nullptr, &queryStatus);
                            if (!queryStatus)
                                return queryStatus;
                            selection[index] = selection[index] || planar;
                        }
                    }
                }
                if (!dommy::group::publishCombinedGroup(output, params.m_grp_name, count, selection,
                                                        params.m_exist_act, params.m_invert))
                    return MS::kInvalidParameter;
            } else {
                for (auto it = output.beginCurves(); it != output.endCurves(); ++it) {
                    if (!*it)
                        return MS::kInvalidParameter;
                    auto& item = **it;
                    const std::size_t count = item.numberOfPoints();
                    DPIdList selection(count, false);
                    if (params.isNumeric)
                        DOMMY_RETURN_IF_FAILURE(generateNumericGroup(params, count, selection));
                    if (params.isBound)
                        DOMMY_RETURN_IF_FAILURE(
                            generateBoundGroup(params, *item.getPositionPtrConst(), selection));
                    if (params.isNormal) {
                        if (params.m_normalType == DC_VEC_kAttribute) {
                            DOMMY_RETURN_IF_FAILURE(
                                generateAttributeNormalGroup(params, item, count, selection));
                        } else if (count != 0U) {
                            if (!dommy::attribute::validArrayConversionBudget(count,
                                                                              sizeof(Imath::V3d)))
                                return MS::kInvalidParameter;
                            MObject object;
                            DOMMY_RETURN_IF_FAILURE(generateMCurveFromDcurve(*it, object));
                            MStatus queryStatus;
                            MFnNurbsCurve curve(object, &queryStatus);
                            if (!queryStatus)
                                return queryStatus;
                            std::vector<Imath::V3d> vectors;
                            vectors.reserve(count);
                            for (const auto& point : *item.getPositionPtrConst()) {
                                MVector direction;
                                if (!dommy::group::curveDirectionAtClosestPoint(
                                        curve, MPoint(point[0], point[1], point[2]),
                                        params.m_normalType == DC_VEC_kNormal, kMFnNurbsEpsilon,
                                        MSpace::kObject, direction))
                                    return MS::kFailure;
                                vectors.emplace_back(direction[0], direction[1], direction[2]);
                            }
                            DOMMY_RETURN_IF_FAILURE(
                                generateNormalGroup(params, vectors, selection));
                        }
                    }
                    if (!dommy::group::publishCombinedGroup(item, params.m_grp_name, count,
                                                            selection, params.m_exist_act,
                                                            params.m_invert))
                        return MS::kInvalidParameter;
                }
            }
            return MS::kSuccess;
        });
    if (!status)
        return status;
    return data.setClean(plug);
}
DOMMY_COMPUTE_CATCH("DCGroup")

MStatus DCGroup::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;

    // ----------------------- INPUTS --------------------------
    inDC = tAttr.create("inDC", "idc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable( false ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDC));

    const MObject defaultString1 = fnStringData.create("group", &stat);
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

    aNormalType = enumExistFn.create( "NormalType", "normty", DC_VEC_kTangent, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Curve Tangent", DC_VEC_kTangent ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "Curve Normal", DC_VEC_kNormal ));
    DOMMY_RETURN_IF_FAILURE(enumExistFn.addField( "From Attribute", DC_VEC_kAttribute ));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aNormalType));

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

    //////////////////////////////////Curve Action//////////////////////////////////

    aIsCurveLength = nAttr.create( "IsCurveLength", "icrvlen", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsCurveLength));

    aCurveLength = nAttr.create( "CurveLength", "crvlen", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveLength));

    aIsArea = nAttr.create( "IsArea", "icrvarea", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsArea));

    aCurveArea = nAttr.create( "CurveArea", "crvarea", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(5.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( true ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurveArea));

    aIsPlanar = nAttr.create( "IsPlanar", "ipln", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsPlanar));

    //////////////////////////////////Common//////////////////////////////////

    aInvert = nAttr.create( "Invert", "inv", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setKeyable( false ));
    DOMMY_RETURN_IF_FAILURE(nAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInvert));

    //create custom data
    outDC = tAttr.create("outDC", "odc", DCurveData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDC));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( inDC, outDC ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGroupName, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aGroupType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExistActionType, outDC ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsNumberAction, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNumberType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aPattern, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aStartEnd, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSelectOf, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aExpression, outDC ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsBoundingAction, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingSize, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBoundingCenter, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInObject, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aInvert, outDC ));

    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsNormalAction, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalType, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalDirection, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSpreadAngle, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aNormalAttributeName, outDC ));

    //curve
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsCurveLength, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCurveLength, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsArea, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aCurveArea, outDC ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aIsPlanar, outDC ));

    DOMMY_RETURN_IF_FAILURE(DCGroupManip::addToManipConnectTable(typeId));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException("DCGroup", "initialize", std::current_exception());
}
