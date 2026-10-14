//
//  DPVecMaxComponent.hpp
//  dommyTool
//
//  Created by yuzo on 2/20/17.
//
//

#pragma once

#include <maya/MPxNode.h>

class DPVecMaxComponent final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	
	static MObject inDD;
	
	static MObject inSourceDataArray;
	static MObject aSourceAttribute;
	
	
	
	static MObject aOutAttribute;
	static MObject outDD;
	
	static MObject outDataArray;
	
	
};

