//
//  DMGroupFromDP.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/23.
//

#pragma once

#include <maya/MPxNode.h>
#include <maya/MManipData.h>
#include <maya/MPxManipContainer.h>



#include "DGroupCommon.h"

///DMeshのプリミティブメッシュをDPointを使って生成
class DMGroupFromDP final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static MTypeId typeId;
    
public:
    
    static MObject inDM;
    
    static MObject aGroupName;
    
    //dpoint
    static MObject aTargetDP;
    static MObject aUseDistance;
    static MObject aDistance;
    
    
    
    
    
    static MObject outDM;
    
    
private:
    
};
