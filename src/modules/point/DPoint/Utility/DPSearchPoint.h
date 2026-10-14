//
//  DPSearchPoint.hpp
//  dommyTool
//
//  Created by yuzo on 2017/12/22.
//


#pragma once

#include <maya/MPxNode.h>


#include "DMayaHeaders.h"



//指定された距離内に
class DPSearchPoint final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDD;
    
    static MObject aTargetDP;
    
    static MObject aPrimAttributeNames;
    static MObject aIndexAttributeNames;
    
    static MObject aRadius;
    static MObject aMaxCount;
    static MObject aDetectNearestPoint;
    
    static MObject outDD;
};


