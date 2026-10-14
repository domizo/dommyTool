//
//  DPStringArrayGen.hpp
//  dommyTool
//
//  Created by yuzo on 6/22/16.
//
//

#pragma once

#include <maya/MPxNode.h>

class DPStringArrayGen final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	
	static MObject value;
	
	static MObject valueSize;
	
	static MObject outDataArray;
	
};

