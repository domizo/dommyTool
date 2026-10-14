//
//  DCNoise.hpp
//  dommyTool
//
//  Created by yuzo on 2/21/17.
//
//

#pragma once

#include <maya/MPxNode.h>

#include "DNoiseOp.h"

class DCNoise final : public MPxNode {
  public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;

    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;

    MStatus evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms);

    static MObject inDC;

    static MObject aNoiseType;
    static MObject aSeed;
    static MObject aUseSeedOffset;

    // attach attribute
    static MObject aIsNoiseAttribute;
    static MObject aNoiseAttributeName;

    // deform
    static MObject aDeformType;
    static MObject aDeformVector;
    static MObject aVectorAttributeName;

    // generate calc position
    static MObject aNoiseCalcPosition;
    static MObject aPositionAttribute;

    // noise
    static MObject aAmplitube;
    static MObject aFrequency;
    static MObject aOctave;
    static MObject aLacunarity;
    static MObject aPersistence;

    // voronoi
    static MObject aIsVoronoiDistance;
    static MObject aVoronoiDisplacement;

    // turbulence
    static MObject aTurbulencePower;
    static MObject aTurbulenceRoughnesss;

    // wood
    static MObject aWooodFrequency;
    static MObject aWooodScale;
    static MObject aWooodBias;
    static MObject aWooodGrain;
    static MObject aWooodAngle;

    static MObject aScale;
    static MObject aUniformScale;
    static MObject aOffset;

    static MObject outDC;
};
