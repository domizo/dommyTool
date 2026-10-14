//
//  DMFastNoise.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/13.
//
//


#pragma once

#include <maya/MPxNode.h>


#include "DNoiseOp.h"




class DMFastNoise final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    MStatus evalNoiseParms(MDataBlock& data, FastNoiseParams& parms);
    
    
    static MObject inDM;
    
    static MObject aNoiseType;
    static MObject aInterpType;
    
    
    static MObject aSeed;
    static MObject aUseSeedOffset;
    
    
    //attach attribute
    static MObject aIsNoiseAttribute;
    static MObject aNoiseAttributeName;
    
    
    //deform
    static MObject aDeformType;
    static MObject aDeformVector;
    static MObject aVectorAttributeName;
    
    
    //noise
    static MObject aAmplitube;
    static MObject aFrequency;
    static MObject aOctave;
    static MObject aLacunarity;
    static MObject aGain;
    static MObject aFractalType;
    
    
    //Cellular
    static MObject aCellularDistanceFunction;
    static MObject aCellularReturnType;
//    static MObject aCellularNoiseLookup;
    
    //Position Warping
    static MObject aPositionWarpAmp;
    
    //transform
    static MObject aScale;
    static MObject aUniformScale;
    static MObject aOffset;
    
    
    static MObject outDM;
    
};
