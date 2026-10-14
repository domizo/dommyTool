//
//  DBSFlipMesh.hpp
//  dommyTool
//
//  Created by yuzo on 2018/03/18.
//





#pragma once



#include <maya/MPxNode.h>

//ブレンドシェイプ用のターゲットメッシュオブジェクトから左右を反転したメッシュオブジェクトを生成



class DBSFlipMesh final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inBaseMesh;
    static MObject inTargetMesh;
    
    static MObject aUseMask;
    static MObject aInvert;
    static MObject inMaskMesh;
    
    static MObject aTolerance;
    
    static MObject outMesh;
    
private:
    
};


