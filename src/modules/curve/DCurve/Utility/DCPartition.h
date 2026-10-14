//
//  DCPartition.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/27.
//

#pragma once



#include <maya/MPxNode.h>


#include "DType.h"

//DCurveをPartionで分割したカーブを生成
class DCPartition final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aMode;
    
    
    static MObject outDC;
    
private:
    
};
