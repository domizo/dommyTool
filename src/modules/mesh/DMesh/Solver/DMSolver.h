//
//  DMSolver.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/05.
//
//

#pragma once

#include <maya/MPxNode.h>

class DMSolver final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    
    static MObject aCurrentTime;
    static MObject prevDM;
    static MObject nextDM;
    
    static MObject outDM;
};
