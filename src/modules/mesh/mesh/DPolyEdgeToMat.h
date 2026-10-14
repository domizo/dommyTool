//
//  DPolyEdgeToMat.hpp
//  dommyTool
//
//  Created by yuzo on 2018/03/17.
//

#pragma once

#include <maya/MPxNode.h>

//ポリゴンエッジの法線と接戦からマトリックスを生成
class DPolyEdgeToMat final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inMesh;
    
    static MObject aEdgeNumber;
    
    static MObject outMatrix;
    static MObject outTranslate;
    static MObject outRotate;
};
