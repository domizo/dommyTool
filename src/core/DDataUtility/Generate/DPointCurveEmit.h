//
//  DPointCurveEmit.h
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#pragma once

#include <maya/MPxNode.h>
#include <maya/MObjectArray.h>

#include "DPData.h"


class DPointCurveEmit final : public MPxNode
{
public:
	
	void postConstructor() override;
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
    
    
    
	// Attributes
	static MObject inCurve;
    static MObject inDC;
    
	
	static	MObject samplingType;
	
	static MObject sampling;
	static MObject curveLength;
    static MObject stepSize;
	static MObject offset;
	static MObject aFillLength;
	
	//tangent
	static MObject aTangentMode;
	static MObject aTangentVector;
	static MObject aInTangentCurve;
	
    //tube
    static MObject aFillSampling;
    static MObject aRadius;
	static MObject aRadiusRamp;
    
    //tube outer
    static MObject aOuterType;
    static MObject aOuterCircleSampling;
    static MObject aOuterRandomSampling;
    
    //pyro
    static MObject aDisplaceAmplitude;
    
    
    //noise
    static MObject aSeed;
    static MObject aAmplitube;
    static MObject aFrequency;
    static MObject aOctave;
    static MObject aLacunarity;
    static MObject aPersistence;
    
    static MObject aNoiseScale;
    static MObject aNoiseOffset;
	
	static MObject outDP;

private:
	MStatus processGeneratePoint(MDataBlock& data,
	                          const MObjectArray& curves,
	                          DPointList& outputPoints);
};
