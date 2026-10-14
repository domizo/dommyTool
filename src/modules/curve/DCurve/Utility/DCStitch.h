//
//  DCStitch.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/17.
//



#pragma once


#include <maya/MPxNode.h>

///複数のDCurve同士間にカーブを生成して繋げる
class DCStitch final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aSampling;
    
    static MObject aRange;
    static MObject aOffset;
    
    static MObject outDC;
    
private:
    
};
