//
//  DNumericToFloatArray.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/11.
//




#pragma once



#include <maya/MPxNode.h>

//Maya内の数値アトリビュートをFloatのデータアレイに変換
//今現在は主にfluidShapeノードに接続するための配列データを生成するために使用する
class DNumericToFloatArray final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    
    static MObject inputData;
    
    static MObject outDataArray;
    
    
private:
    
};
