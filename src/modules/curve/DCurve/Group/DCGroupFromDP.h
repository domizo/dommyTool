//
//  DCGroupFromDP.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/26.
//

#pragma once

#include <maya/MPxNode.h>
#include <maya/MManipData.h>
#include <maya/MPxManipContainer.h>

#include "DGroupCommon.h"

///DCurveのGroupをDPointの距離を使って生成
class DCGroupFromDP final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aGroupName;
    
    //dpoint
    static MObject aTargetDP;
    static MObject aUseDistance;
    static MObject aDistance;
    
    
    static MObject outDC;
    
    
private:
    
};
