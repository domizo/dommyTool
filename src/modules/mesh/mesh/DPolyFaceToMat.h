//
//  DPolyFaceToMat.hpp
//  dommyTool
//
//  Created by yuzo on 2018/03/16.
//





#pragma once

#include <maya/MPxNode.h>

//ポリゴンメッシュの面情報からマトリックスを生成
class DPolyFaceToMat final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    
    static MObject inMesh;
    
    static MObject aFaceNumber;
    
    
    
    
    static MObject outMatrix;
    static MObject outTranslate;
    static MObject outRotate;
};
