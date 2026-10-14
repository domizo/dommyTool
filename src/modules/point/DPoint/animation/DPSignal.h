//
//  DPSignal.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/01.
//





#pragma once

#include <vector>

#include <maya/MPxNode.h>


///数字データを生成
class DPSignal final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject aDefaultValue;
    
    //ramp
    static MObject aValueRamp;
    static MObject aRampValueRange;
    static MObject aTimeRange;
    
    
    //adjust
    static MObject aStrength;
    static MObject aPlayMode;
    static MObject aLoopValueMode;
    
    
    //noise
    static MObject aUseNoise;
    static MObject aMethodNoise;
    static MObject aSeed;
    static MObject aAmplitude;
    static MObject aFrequency;
    static MObject aOctave;
    static MObject aLacunarity;
    static MObject aPersistence;
    
    static MObject aIsAbsolute;
    
    //trigonometry
    static MObject aUseTrigonometry;
    static MObject aTrigonometryFunction;
    static MObject aTrigonometryMethod;
    static MObject aTrigonometryAmplitude;
    static MObject aTrigonometryFrequency;
    static MObject aTrigonometryOffset;
    static MObject aTrigonometryAbsolute;
    
    //time
    static MObject aTimeScale;
    static MObject aCurrentTime;
    
    
    
    static MObject aOutValue;
    
};
