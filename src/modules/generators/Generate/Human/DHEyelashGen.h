//
//  DHEyelashGen.hpp
//  dommyTool
//
//  Created by yuzo on 2018/06/25.
//


#pragma once

#include <maya/MPxNode.h>

class DHEyelashGen final : public MPxNode
{
public:
    
    DHEyelashGen() {}
    ~DHEyelashGen() override {}
    
//    virtual void postConstructor();
    
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    SchedulingType schedulingType() const override { return kSerial; }
    
    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;
    
    
    
    static MObject inMesh;
    static MObject inCurve;
    
    
    static MObject aEdgeList;
    static MObject aCenter;
    
    static MObject aCount;
    
    
    //curveParam
    static MObject aLength;
    static MObject aLengthRamp;
    static MObject aSegment;
    
    static MObject aSurfaceOffset;
    static MObject aSurfaceOffsetRamp;
    
    
    //charactor Param
    static MObject aRootRotate;
    static MObject aRootRotateXRamp;
    static MObject aRootRotateYRamp;
    static MObject aRootRotateZRamp;
    
    static MObject aGlobalBend;
    static MObject aGlobalBendRamp;
    
    static MObject aRootBend;
    static MObject aTipBend;
    
    static MObject aRootBendRamp;
    static MObject aTipBendRamp;
    
    static MObject aRootBendPerRamp;
    static MObject aTipBendPerRamp;
    
    //rand param
    static MObject aSeed;
    static MObject aRootNoise;
    
    
    static MObject aLengthRandom;
    
    
    static MObject aRandTranslateMin;
    static MObject aRandTranslateMax;
    
    
    static MObject aRandRotateMin;
    static MObject aRandRotateMax;
    
    
    
    
    static MObject outCurves;
    
    
};

