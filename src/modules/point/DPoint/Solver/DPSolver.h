//
//  DPSolver.hpp
//  dommyTool
//
//  Created by yuzo on 2017/04/01.
//
//

#pragma once

#include <maya/MPxNode.h>

class DPSolver final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDP;
    
    static MObject aCurrentTime;
    static MObject prevDP;
    static MObject nextDP;
    
    static MObject outDP;
};
