//
//  DMDebrisGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/22.
//





#pragma once

#include <maya/MPxNode.h>


///DMeshを破片の形状に編集
class DMDebrisGen final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    
    
    static MObject inDM;
    
    
    
    
    static MObject outDM;
    
};
