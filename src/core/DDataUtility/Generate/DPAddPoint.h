//
//  DPAddPoint.hpp
//  dommyTool
//
//  Created by yuzo on 2016/11/19.
//
//

#pragma once


#include <maya/MPxNode.h>

class DPAddPoint final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    // Attributes
    static MObject aPoint;
    
    
    static MObject outDP;
    
};


