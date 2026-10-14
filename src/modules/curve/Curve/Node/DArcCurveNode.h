//
//  DArcCurveNode.h
//  dommyTool
//
//  Created by yuzo on 2015/01/20.
//
//

#pragma once

#include <maya/MPxNode.h>

class DArcCurveNode final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static	MObject		divide;
    static	MObject     radius;
    static	MObject		width;
    
    static	MObject		startAngle;
    static	MObject		endAngle;
    static	MObject		offsetAngle;
    
    static	MObject		outputMesh;
};
