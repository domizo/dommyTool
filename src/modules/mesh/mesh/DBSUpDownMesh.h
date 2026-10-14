//
//  DBSUpDownMesh.hpp
//  dommyTool
//
//  Created by yuzo on 2018/03/21.
//

#pragma once



#include <maya/MPxNode.h>

//ブレンドシェイプ用のターゲットメッシュオブジェクトから上下変形したメッシュオブジェクトをカラーマップを元に生成

class DBSUpDownMesh final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    
    static MObject inBaseMesh;
    static MObject inTargetMesh;
    
    
    
    
    static MObject aSpace;
    static MObject aInvert;
    
    
    
    static MObject outUpMesh;
    static MObject outDownMesh;
    
private:
    
};
