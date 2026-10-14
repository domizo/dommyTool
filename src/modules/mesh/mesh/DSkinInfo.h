//
//  DSkinInfo.hpp
//  dommyTool
//
//  Created by yuzo on 2018/04/17.
//




#pragma once


#include <maya/MPxNode.h>

//スキン目的のメッシュデータの情報を表示

class DSkinInfo final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    
    static MObject inBaseMesh;
    static MObject inTargetMesh;
    
    
    
    
    
    static MObject outMesh;
    
private:
    
};
