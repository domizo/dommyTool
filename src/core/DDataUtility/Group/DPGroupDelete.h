//
//  DPGroupDelete.hpp
//  dommyTool
//
//  Created by yuzo on 5/26/16.
//
//

#pragma once


#include <maya/MPxNode.h>




class DPGroupDelete final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	static MObject aGroupName;
	static MObject aIsEnable;
		
	static MObject outDP;
	
};


