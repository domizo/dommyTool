//
//  DOpenVDBPrune.hpp
//  dommyTool
//
//  Created by yuzo on 2017/12/15.
//


#pragma once
#include <maya/MPxNode.h>




class DOpenVDBPrune final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    
    
    static MObject aMode;
    static MObject aTolerance;
    
    static MObject aVdbOutput;
    
};
