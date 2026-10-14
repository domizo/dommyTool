//
//  DComponentListGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/12/20.
//






#pragma once



#include <maya/MPxNode.h>

//ComponentList Dataの生成
class DComponentListGen final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    
    static MObject aComponentType;
    
    static MObject aSize;
    static MObject aMode;
    static MObject aPattern;
    static MObject inputDataArray;
    
    static MObject outComponentList;
    
    
private:
    
};
