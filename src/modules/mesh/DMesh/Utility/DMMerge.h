//
//  DMMerge.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/24.
//
//





#pragma once

#include <maya/MPxNode.h>


//DMeshの合体
class DMMerge final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    
    
    static MObject inDM;
    
    static MObject targetDM;
    
    
    static MObject outDM;
    
};
