//
//  DPGoal.hpp
//  dommyTool
//
//  Created by yuzo on 2017/04/11.
//
//





#pragma once

#include <vector>

#include <maya/MPxNode.h>
#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"

//特定の位置に向かうベクトルを作成
class DPGoal final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDP;
    
    
    static MObject aVelocityAttribute;
    
    
    static MObject aGoalType;
    
    static MObject aPosition;
    static MObject aTargetDP;
    
    static MObject aMagnitude;
    static MObject aDamping;
    static MObject aSpringConstant;
    
    static MObject aUseMass;
    static MObject aMassAttribute;
    
    
    static MObject outDP;
    
};




