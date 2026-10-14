//
//  DPGroup.hpp
//  dommyTool
//
//  Created by yuzo on 5/25/16.
//
//

#pragma once

#include <maya/MPxNode.h>



#include <maya/MManipData.h>
#include <maya/MPxManipContainer.h>


#include "DGroupCommon.h"


class DPGroupManip final : public MPxManipContainer
{
public:
		DPGroupManip() = default;
		~DPGroupManip() override = default;
		
		static void * creator();
		static MStatus initialize();
		MStatus createChildren() override;
		MStatus connectToDependNode(const MObject & node) override;
		
		// Viewport 2.0 manipulator draw overrides
		void preDrawUI(const M3dView& view) override;
		void drawUI(MHWRender::MUIDrawManager& drawManager,
					const MHWRender::MFrameContext& frameContext) const override;
		
		void draw(M3dView & view,
				  const MDagPath & path,
				  M3dView::DisplayStyle style,
				  M3dView::DisplayStatus status) override;
		MManipData plugToManipConversion(unsigned int manipIndex) override;
		MVector manipulatorOrigin() const;
		MStatus getBBoxSize(MVector &sizeVec, MVector &centerVec, short &typeVal) const;
	
	MDagPath fBBoxManip;
	MDagPath fScaleManip;
	MDagPath fNormalVectorManip;
	MDagPath fSpreadManip;
	MDagPath fStateManip;
	
		MObject nodeObj;
	// Value prepared for Viewport 2.0 draw
	MPoint fTextPosition;
	MVector fBBoxSize;
	MVector fBBoxCenter;
	short fBBoxType;
	
public:
	static MTypeId id;
};









class DPGroup final : public MPxNode
{
public:
	
		MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static MTypeId typeId;
	
	
	MStatus evalParms(MDataBlock& data, DGroupParams& parms);
	
	
	
	
	static MObject inDP;
	
	static MObject aGroupName;
	static MObject aExistActionType;
	
	//number
	static MObject aIsNumberAction;
	static MObject aNumberType;
	static MObject aPattern;
	static MObject aStartEnd;
	static MObject aSelectOf;
	static MObject aExpression;
	
	//bounding
	static MObject aIsBoundingAction;
	static MObject aBoundingType;
	static MObject aBoundingSize;
	static MObject aBoundingCenter;
	static MObject aInObject;
	
	
	//normal
	static MObject aIsNormalAction;
	static MObject aNormalDirection;
	static MObject aSpreadAngle;
	static MObject aNormalAttributeName;
	
	
	
	static MObject aInvert;
	
	static MObject outDP;
	
};
