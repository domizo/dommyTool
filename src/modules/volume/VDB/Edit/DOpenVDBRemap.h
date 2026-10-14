//
//  DOpenVDBRemap.hpp
//  dommyTool
//
//  Created by yuzo on 2017/01/12.
//
//


#pragma once
#include <maya/MPxNode.h>




class DOpenVDBRemap final : public MPxNode
{
public:
    
    void postConstructor() override;
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    
    
    static MObject aBelowMinimum;
    static MObject aAboveMaximum;
    
    static MObject aInputRange;
    static MObject aOutputRange;
    
    static MObject aTransferFunction;
    static MObject aDeactivateBackgroundVoxels;
    
    
    static MObject aVdbOutput;
    
    
};
