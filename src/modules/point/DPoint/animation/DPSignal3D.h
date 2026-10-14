//
//  DPSignal3D.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/02.
//

#pragma once

#include <vector>

#include <maya/MPxNode.h>


///3D数字データを生成
class DPSignal3D final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject aDefaultValue;
    
    
    //ramp
    static MObject aValueRampX;
    static MObject aValueRampY;
    static MObject aValueRampZ;
    static MObject aRampValueRange;
    static MObject aTimeRange;
    
    
    //adjust
    static MObject aStrength;
    static MObject aUniformStrength;
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
    
    static MObject aIsAbsoluteX;
    static MObject aIsAbsoluteY;
    static MObject aIsAbsoluteZ;
    
    //trigonometry
    static MObject aUseTrigonometry;
    static MObject aTrigonometryFunctionX;
    static MObject aTrigonometryFunctionY;
    static MObject aTrigonometryFunctionZ;
    
    static MObject aTrigonometryMethodX;
    static MObject aTrigonometryMethodY;
    static MObject aTrigonometryMethodZ;
    static MObject aTrigonometryAmplitude;
    static MObject aTrigonometryFrequency;
    static MObject aTrigonometryOffset;
    static MObject aTrigonometryAbsoluteX;
    static MObject aTrigonometryAbsoluteY;
    static MObject aTrigonometryAbsoluteZ;
    
    //time
    static MObject aTimeScale;
    static MObject aCurrentTime;
    
    
    
    static MObject aOutValue;
    
};
