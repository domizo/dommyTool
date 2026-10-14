//
//  DPArrayToCompound.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/02.
//
//


#pragma once



#include "DMayaHeaders.h"


/// Archived integer-array conversion prototype; other accepted types are unfinished.
class DPArrayToCompound final : public MPxNode
{
public:
    
    
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    
    
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inputData;
    
    
    
    
    static MObject outValue;
    
    
};
