//
//  DCWeightMap.hpp
//  dommyTool
//
//  Created by yuzo on 2017/02/22.
//
//




#pragma once

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

//カーブの特定の範囲のパラメーターアトリビュートを作成
class DCWeightMap final : public MPxNode
{
public:
    void postConstructor() override;
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
    static MObject inDC;
    
    
    
	static MObject aParamType;
    static MObject aBaseParam;
	static MObject aParamAttributeName;
	
    
    static MObject aDistance;
	
	static MObject aOffset;
    static MObject aWrapMode;
    
    static MObject aWeightAttributeName;
    
    static MObject aWeightRamp;
    static MObject aWeightScale;
    
    static MObject aRepeatWeight;
    
    static MObject outDC;
};
