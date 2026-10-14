//
//  DPointCircleEmit.h
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#pragma once

#include <maya/MPxNode.h>

enum {
    D_CIRCLE_RADIAL = 0,
    D_CIRCLE_FILL,
    D_CIRCLE_EMIT_TYPE_COUNT
};typedef int D_CIRCLE_EMIT_TYPE;

class DPointCircleEmit final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    bool withinCircle(float x, float y);
    
    // Attributes
    static MObject emitType;
    
    static MObject radius;
    static MObject sampling;
    static MObject startAngle;
    static MObject endAngle;
    static MObject offsetAngle;
    
    static MObject aSeed;
    
//    static MObject count;
    
    static MObject outDP;
    
};

inline bool DPointCircleEmit::withinCircle(float x, float y) {
    return x * x + y * y < 1.0f;
}
