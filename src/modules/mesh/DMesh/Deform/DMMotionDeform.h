//
//  DMMotionDeform.hpp
//  dommyTool
//
//  Created by yuzo on 2017/06/22.
//

//DMeshをモーションデータによって変形させます

#pragma once

#include <cstddef>
#include <vector>


#include <maya/MPxNode.h>
#include <maya/MPointArray.h>

#include "DMeshData.h"

class DMMotionDeform final : public MPxNode
{
public:
	
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
	
	MStatus getDMeshFromTime(std::vector<std::vector<DPointList>>& ptsArray,
	                         const std::vector<std::size_t>& expectedPointCounts,
	                         std::size_t totalPointCount,
	                         long sampling, double timeRange,
	                         const MTime& currentTime);
	
	
public:
	
	
	static MObject inDM;
	
	
	static MObject aTimeRange;
	static MObject aSampling;
	
	static MObject aAmplitude;
	static MObject aAmplitudeRamp;
	
	
	
	static MObject aWeightAttributeName;
	static MObject aWeight;
	
	
	static MObject aIsExportMotionPower;
	static MObject aMotionPowerName;
	static MObject aIsExportMotionVector;
	static MObject aMotionVectorName;
	
	static MObject outDM;
	
private:
	
};
