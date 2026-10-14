//
//  DPPointToFloat.hpp
//  dommyTool
//
//  Created by yuzo on 3/11/16.
//
//

#pragma once

#include <maya/MPxNode.h>

enum {
	DP_PTF_MAGNITUDE = 0,
	DP_PTF_LUMINANCE,
	DP_PTF_TYPE_COUNT
};typedef int DP_PTF_TYPE;

class DPPointToFloat final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inputPointArray;
	static MObject convertType;
	
	static MObject outDataArray;
	
};

