//
//  DPClosestPointVDB.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/03.
//

#pragma once

#include <vector>

#include <maya/MPxNode.h>


#include "DMayaHeaders.h"



//VDBを使用したclpを求める
class DPClosestPointVDB final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDP;
    
    
    static MObject inVdb;
    
    static MObject aIsoOffset;
    
    static MObject aIsDeform;
    static MObject aAddAttribute;
    static MObject aAttributeName;
    
    
    static MObject outDP;
};
