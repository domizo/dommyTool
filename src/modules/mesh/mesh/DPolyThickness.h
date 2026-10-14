//
//  DPolyThickness.hpp
//  dommyTool
//
//  Created by yuzo on 2018/05/30.
//


#pragma once


#include <maya/MPxNode.h>

//メッシュの厚さを計算する

class DPolyThickness final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inBaseMesh;
    static MObject outMesh;
    
private:
    
};
