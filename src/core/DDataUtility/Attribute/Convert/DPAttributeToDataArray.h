//
//  DPAttributeToDataArray.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/10.
//

#pragma once



#include <maya/MPxNode.h>

///DPointのアトリビュートをMayaのデータ配列に変換
class DPAttributeToDataArray final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDP;
    
    static MObject aAttributeName;
    
    static MObject outDataArray;
    
    
    
};
