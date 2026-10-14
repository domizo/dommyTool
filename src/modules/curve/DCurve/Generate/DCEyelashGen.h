//
//  DCEyelashGen.hpp
//  dommyTool
//
//  Created by yuzo on 2018/07/03.
//


#pragma once

#include <maya/MPxNode.h>

class DCEyelashGen final : public MPxNode
{
public:
    
    //    virtual void postConstructor();
    
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject inCurve;
    
    
    static MObject aCenter;
    
    static MObject aTangentVector;
    static MObject aTangentCurve;
    static MObject aCount;
    static MObject aOffset;
    static MObject aQuatOrder;
    
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
    
    
    
    
    static MObject outDC;
    
    
};
