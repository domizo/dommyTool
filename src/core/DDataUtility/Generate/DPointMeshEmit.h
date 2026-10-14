//
//  DPointMeshEmit.h
//  dommyTool
//
//  Created by yuzo on 3/23/15.
//
//

#pragma once



#include <maya/MPxNode.h>

#include "DPData.h"

class DPointMeshEmit final : public MPxNode
{
public:
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	// Attributes
	static MObject inMesh;
	static MObject inDM;
    
	static MObject isVertexEmit;
	
	static MObject isEdgeEmit;
	static MObject edgeEmitType;
	static MObject edgeSampling;
	static MObject stepWidth;
	static MObject isFinishEdgeLength;
    
	static MObject isFaceEmit;
	static MObject faceEmitType;
	static MObject faceSampling;
    
    static MObject aIsFromUV;
	static MObject aUVSamplingType;
	static MObject aUVUniformCount;
	static MObject aUVRandomCount;
	
	static MObject aSeed;
	
    static MObject aURange;
    static MObject aVRange;
	
	static MObject aUVIsRelax;
	static MObject aUVRelaxIteration;
	
    static MObject aUVTolerance;
	
	
	
    
	
	static MObject outDP;
	
private:
	void emitDPointFromUVBase(MDataBlock& data,
	                           const MObject& meshObject,
	                           DPointList& outputPoints);
};


