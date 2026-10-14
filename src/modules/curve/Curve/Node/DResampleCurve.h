//
//  DResampleCurve.hpp
//  dommyTool
//
//  Created by yuzo on 2018/03/29.
//


#pragma once

#include <maya/MPxNode.h>

//ベースカーブから新しリサンプルされたカーブを生成



class DResampleCurve final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inCurve;
    
    static MObject aSampling;
    
    static MObject outCurve;
};


