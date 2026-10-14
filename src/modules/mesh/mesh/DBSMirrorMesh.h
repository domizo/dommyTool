//
//  DBSMirrorMesh.hpp
//  dommyTool
//
//  Created by yuzo on 2018/03/15.
//







#pragma once



#include <maya/MPxNode.h>

//ブレンドシェイプ用のターゲットメッシュオブジェクトから左右のみ変形したメッシュオブジェクトを生成



class DBSMirrorMesh final : public MPxNode
{
public:
    void postConstructor() override;
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    
    static MObject inBaseMesh;
    static MObject inTargetMesh;
    
    
    static MObject aDistance;
    static MObject aBlendRamp;
    
    
    
    static MObject outLeftMesh;
    static MObject outRightMesh;
    
private:
    
};




