//
//  DPointFromFluid.hpp
//  dommyTool
//
//  Created by yuzo on 11/30/16.
//
//

#pragma once

#include <maya/MPxNode.h>



class DPointFromFluid final : public MPxNode
{
public:
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	static MObject aInFluid;
	static MObject aTime;
	static MObject outDP;
};
