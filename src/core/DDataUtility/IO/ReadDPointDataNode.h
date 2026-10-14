//
//  readDPointDataNode.h
//  dommyTool
//
//  Created by yuzo on 3/14/15.
//
//

#pragma once

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

class ReadDPointDataNode final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static	 MObject filePath;
	
	static	 MObject outputPointArray;
	
};
