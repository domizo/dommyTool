//
//  DPAttributeVDB.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/03.
//


#pragma once



#include <maya/MPxNode.h>

///DPointのアトリビュートをVdbデータから参照
class DPAttributeVDB final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDP;
    static MObject inVdb;
    
    static MObject aSourceGridName;
    
    static MObject outDP;
    
    
    
};
