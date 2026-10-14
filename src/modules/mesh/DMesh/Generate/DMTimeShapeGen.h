//
//  DMTimeShapeGen.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/08.
//


#pragma once

#include <maya/MPxNode.h>


///DCurveからwire形状のDMeshを生成
class DMTimeShapeGen final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inMesh;
    
    
    static MObject aStartFrame;
    static MObject aEndFrame;
    
    static MObject aTimeSample;
    
    
    
    static MObject outDM;
    
};



