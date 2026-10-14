//
//  DPointSurfaceEmit.h
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#pragma once

#include <maya/MPxNode.h>

enum {
	D_SURFACE_SAMPLING_ALL = 0,
	D_SURFACE_SAMPLING_RANGE,
	D_SURFACE_SAMPLING_CV,
	D_SURFACE_SAMPLING_TYPE_COUNT
};typedef int D_SURFACE_SAMPLING_TYPE;

class DPointSurfaceEmit final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	// Attributes
	static MObject inSurfaceObject;
	
	static	MObject samplingType;
	
	static	MObject samplingU;
	static	MObject samplingV;
	
	static	MObject	 uLength;
	static	MObject	 vLength;
	static	MObject	 uOffset;
	static	MObject	 vOffset;
    
    
    
    static MObject numU;
    static MObject numV;
    
    static MObject stepU;
    static MObject stepV;
    
    static MObject offsetU;
    static MObject offsetV;
	
	
	static MObject outDP;
	
};

