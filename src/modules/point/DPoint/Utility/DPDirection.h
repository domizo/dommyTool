//
//  DPDirectionGen.hpp
//  dommyTool
//
//  Created by yuzo on 6/26/16.
//
//

#pragma once

#include <maya/MPxNode.h>
#include <maya/MManipData.h>
#include <maya/MPxManipContainer.h>

class DPDirectionManip final : public MPxManipContainer
{
public:
	    DPDirectionManip() = default;
	    ~DPDirectionManip() override = default;
    
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
	
	MDagPath fBaseVectorManip;
	MDagPath fTargetVectorManip;
	MDagPath fUseDistManip;
    MDagPath fDistanceManip;
    MDagPath fScaleManip;
	MDagPath fStateManip;
	
	    // Value prepared for Viewport 2.0 draw
    MPoint fTextPosition;
	
public:
    static MTypeId id;
};



class DPDirection final : public MPxNode
{
public:
	
		MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static MTypeId typeId;
	
	
	static MObject inDD;
	
	
	static MObject dirAttrName;
	static MObject baseVector;
	static MObject targetVector;
	
	static MObject aDirectionType;
	
	static MObject inObject;
	static MObject inTargetDP;
	
	static MObject aCalcType;
	static MObject aUseDistance;
	static MObject aDistance;
	
	
	static MObject aCurveNormalType;
	static MObject aSurfaceNormalType;
	
	static MObject aTargetDPCalcType;
    static MObject aTargetDPAttrName;
    
	static MObject aInvert;
	static MObject aNormalize;
	static MObject aScale;
    
    
    static MObject aAttributeClass;
	
	static MObject outDD;
	
};


