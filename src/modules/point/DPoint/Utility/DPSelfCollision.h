//
//  DPSelfCollision.hpp
//  dommyTool
//
//  Created by yuzo on 2017/04/05.
//
//




//特定の距離にあるポイントの衝突を検出



#pragma once

#include <vector>

#include <maya/MPxNode.h>
#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"


class DPSelfCollision final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDP;
    
    static MObject aSearchRadius;
    
    
    static MObject aRadius;
    static MObject aRadiusAttribute;
    
    static MObject aUseMass;
    static MObject aMassAttribute;
    
    static MObject aDamping;
    static MObject aAvoidPower;
    
    static MObject outDP;
    
};
