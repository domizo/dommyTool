//
//  DBSCurve.hpp
//  dommyTool
//
//  Created by yuzo on 2018/03/27.
//




#pragma once

#include <maya/MPxNode.h>

//ベースカーブとターゲットカーブから中間のカーブオブジェクトを生成

class DBSCurve final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inBaseCurve;
    static MObject inTargetCurve;
    
    static MObject aBlend;
    static MObject aSampling;
    
    static MObject outCurve;
};





