//
//  DPolySymmetry.hpp
//  dommyTool
//
//  Created by yuzo on 2018/05/15.
//

#pragma once



#include <maya/MPxNode.h>

//非対称のメッシュオブジェクトをターゲットメッシュのバーテックス情報を元にIDマッチを生成し、
//右、左、どちらかの形状と同期させる
//カラーマップを利用することで無効にすることもできる。（黒色が固定）


class DPolySymmetry final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inBaseMesh;
    static MObject inTargetMesh;
    
    static MObject aType;
    static MObject aTolerance;
    static MObject aUseColor;
    
    static MObject outMesh;
    
private:
    
};


