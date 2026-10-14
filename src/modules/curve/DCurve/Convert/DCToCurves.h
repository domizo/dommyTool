//
//  DCToCurves.hpp
//  dommyTool
//
//  Created by yuzo on 2017/03/14.
//
//

#pragma once


#include <maya/MPxNode.h>


class DCToCurves final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDC;
    
    static MObject outCurves;
    
    
};
