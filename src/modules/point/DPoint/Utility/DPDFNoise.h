//
//  DPDFNoise.hpp
//  dommyTool
//
//  Created by yuzo on 2016/07/13.
//
//

#pragma once

#include <vector>

#include <maya/MPxNode.h>

#include "DNoiseOp.h"


class DPDFNoise final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    MStatus evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms);
    
    
    
    //attribute
    static MObject inDD;
    
    
    static MObject aIsNoiseAttribute;
    static MObject aNoiseAttributeName;
    
    
    static MObject aEpsilon;
    static MObject aPrecision;
    
    
    //normal vector
    static MObject aNormalVectorType;
    static MObject aNormalVector;
    static MObject aReferenceObject;
    
    
    //deform
    static MObject aIsDeform;
    
    
    //noise
    static MObject aNoiseType;
    
    static MObject aSeed;
    static MObject aUseSeedOffset;
    
    static MObject aAmplitube;
    static MObject aFrequency;
    static MObject aOctave;
    static MObject aLacunarity;
    static MObject aPersistence;
    
    //voronoi
    static MObject aIsVoronoiDistance;
    static MObject aVoronoiDisplacement;
    
    //turbulence
    static MObject aTurbulencePower;
    static MObject aTurbulenceRoughnesss;
    
    //wood
    static MObject aWooodFrequency;
    static MObject aWooodScale;
    static MObject aWooodBias;
    static MObject aWooodGrain;
    static MObject aWooodAngle;
    
    
    static MObject aScale;
    static MObject aUniformScale;
    static MObject aOffset;
    
    static MObject aInvert;
    static MObject aNormalize;
    
    static MObject outDD;
    
};
