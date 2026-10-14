//
//  DPNull.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/19.
//
//




#pragma once

#include <maya/MPxNode.h>


//DDataを通過する
class DPNull final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDD;
    
    
    static MObject outDD;
    
};
