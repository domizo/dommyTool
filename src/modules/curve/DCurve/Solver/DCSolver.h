//
//  DCSolver.hpp
//  dommyTool
//
//  Created by yuzo on 2017/03/31.
//
//

#pragma once

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MStatus.h>
#include <maya/MTypeId.h>

class DCSolver final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static void* creator();
    static MStatus initialize();
    
    static const MTypeId typeId;
    
    static MObject inDC;
    
    static MObject aCurrentTime;
    static MObject prevDC;
    static MObject nextDC;
    
    static MObject outDC;
    
};
