//
//  DPolyResetVtx.hpp
//  dommyTool
//
//  Created by yuzo on 2018/05/25.
//




#pragma once



#include <maya/MPxNode.h>

//マスクオブジェクトとしてインプットされたカラー情報をもとに変更された頂点を修正するノード



class DPolyResetVtx final : public MPxNode
{
public:
    
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inBaseMesh;
    static MObject inTargetMesh;
    static MObject inMaskMesh;
    
    static MObject aWeight;
    
    static MObject outMesh;
    
private:
    
};


