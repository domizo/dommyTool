//
//  DPAnimationEmit.h
//  dommyTool
//
//  Created by yuzo on 2015/03/25.
//
//

#pragma once

#include <vector>

#include <maya/MPxNode.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>

enum {
    DP_ANIM_EMIT_MATRIX = 0,
    DP_ANIM_EMIT_POINT,
    DP_ANIM_EMIT_RANDOM,
    DP_ANIM_EMIT_TYPE_COUNT
};typedef int DP_ANIM_EMIT_TYPE;

class DPAnimationEmit final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    static MObject inputGeometry;
    static MObject transformMatrix;
    
    static MObject emitType;
    static MObject seed;
    static MObject randomCount;
    
    
    static	 MObject timeOffset;
    static	 MObject timeLength;
    static	 MObject timeSampling;
    static	 MObject timeLengthRandom;
    
//    static  MObject time;
    
    static MObject outputPointArray;

private:
    MStatus getPlugMatrices(std::vector<MMatrix>& matrices,
                            const MPlug& sourcePlug,
                            unsigned int count,
                            double step,
                            double start) const;
};
