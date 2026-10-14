//
//  DPMotionCreate.hpp
//  dommyTool
//
//  Created by yuzo on 11/19/16.
//
//

#pragma once


#include <maya/MPxNode.h>

class DPMotionCreate final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	static MObject aAnimationMatrix;
    
    //time range
    static MObject aGlobalOffset;
    static MObject aOffsetType;
    static MObject aOffsetAttribute;
    static MObject aInOffsetArray;
    
    static MObject aStretchType;
    static MObject aStretchAttribute;
    static MObject aInStretchArray;
	
    
    //axis transform
	static MObject aIsAxisAlign;
    
    static MObject aTargetType;
    static MObject aTargetAxis;
	static MObject aTargetVec;
    static MObject aTargetAtttribute;
	
	static MObject aIsUseUpVector;
    static MObject aUpType;
    static MObject aUpAxis;
    static MObject aUpVec;
    static MObject aUpAtttribute;
    
	
	static MObject outDP;
	
};
