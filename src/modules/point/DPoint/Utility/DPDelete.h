//
//  DPDelete.hpp
//  dommyTool
//
//  Created by yuzo on 6/25/16.
//
//

#pragma once

#include <maya/MPxNode.h>



#include "DGroupCommon.h"




class DPDelete final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static MTypeId typeId;
	
	
	
	static MObject inDP;
	
	static MObject aDeleteGroups;
	
	
	static MObject aDeleteNumber;
	
	static MObject outDP;
	
};

