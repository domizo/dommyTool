//
//  DCReflect.hpp
//  dommyTool
//
//  Created by yuzo on 2018/10/25.
//

#pragma once


#include <maya/MPxNode.h>

///DCurveがpolygonにヒットした際に曲げる
class DCReflect final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    static MObject inDC;
    
    
    static MObject aInObject;
    
    static MObject aSampling;
    static MObject aBias;
    
    static MObject outDC;
    
    
    
private:
    
};
