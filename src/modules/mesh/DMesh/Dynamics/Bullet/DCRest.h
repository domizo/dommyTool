//
//  DCRest.hpp
//  dommyTool
//
//  Created by yuzo on 2017/11/15.
//


#pragma once

#include <maya/MPxNode.h>


///DCurveのポイントをローカルからワールド、あるいはワールドからローカルに変換する
class DCRest final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inDC;
    
    static MObject aType;
    
    static MObject aRestMatrix;
    
    static MObject outDC;
    
};
