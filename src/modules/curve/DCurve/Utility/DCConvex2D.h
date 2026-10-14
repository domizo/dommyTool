//
//  DCConvex2D.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/26.
//

#pragma once



#include <maya/MPxNode.h>


#include "DType.h"

//指定したグループ毎でのConvexCurveを生成
class DCConvex2D final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    static MObject aPieceGroupName;
    
    static MObject outDC;
    
private:
    
};
