//
//  DCAttributeTransfer.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/30.
//


#pragma once


#include <maya/MPxNode.h>

///DCurveのアトリビュートをDPointの距離を元に作成、編集
class DCAttributeTransfer final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    static MObject aTargetDP;
    
    
    static MObject aAttributeNames;
    static MObject aDistance;
    
    static MObject aTransferRamp;
    static MObject aValueRange;
    static MObject aScale;
    
    
    static MObject outDC;
    
    
private:
    
};
