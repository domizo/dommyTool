//
//  DPAttributeDelete.hpp
//  dommyTool
//
//  Created by yuzo on 5/25/16.
//
//

#pragma once

#include <maya/MPxNode.h>

class DPAttributeDelete final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	static MObject isEnable;
	static MObject attributeName;
	
	static MObject outDP;
	
};

