//
//  DPAnimMatrixGen.hpp
//  dommyTool
//
//  Created by yuzo on 2016/03/02.
//
//

#pragma once

#include <maya/MPxNode.h>

class DPAnimMatrix final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDP;
    
    static MObject aAnimAttributeName;
    
    static MObject animationMatrix;
    static MObject globalOffset;
	
    
    static MObject aOffsetType;
    static MObject aOffsetAttribute;
	static MObject aInOffsetArray;
    
    static MObject aStretchType;
    static MObject aStretchAttribute;
	static MObject aInStretchArray;
	
    static MObject aCurrentTime;
    
    static MObject outDP;
    
};
