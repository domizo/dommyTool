//
//  DCurveCircleGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/08/27.
//



#pragma once


#include <maya/MPxNode.h>

///Circle形状のDCurveを生成
class DCurveCircleGen final : public MPxNode
{
public:
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
    
    
    
    
    static MObject aRadius;
    static MObject aSampling;
    static MObject aStartAngle;
    static MObject aEndAngle;
    static MObject aOffsetAngle;
    
    //curve option
    static MObject aIsOpen;
    static MObject aDegree;
    static MObject aIsMultiEndKnot;
    
    static MObject outDC;
    
    
    
};


