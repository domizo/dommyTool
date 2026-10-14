//
//  DPPyroNoise.hpp
//  dommyTool
//
//  Created by yuzo on 2016/12/04.
//
//

#pragma once


#include <maya/MPxNode.h>

#include "DNoiseOp.h"


class DPPyroNoise final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
	
	
	MStatus evalNoiseBaseParms(MDataBlock& data, NoiseBaseParams& parms);
	
    
    static MObject inDP;
    
    
    static MObject aOutAttribute;
    
	
	//pyro
	static MObject aPyroType;
    static MObject aRadius;
	static MObject aRadiusAttribute;
	
	static MObject aTopRatio;
	static MObject aUnderRatio;
	static MObject aSideRatio;
	
    static MObject aPosition;
	static MObject inObject;
	static MObject inTargetDP;
	
	
	
    //noise
	
	static MObject aNoiseType;
	
    static MObject aSeed;
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
	
	
	
    static MObject outDP;
    
};

