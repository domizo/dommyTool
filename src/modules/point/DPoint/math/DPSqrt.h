//
//  DPSqrt.hpp
//  dommyTool
//
//  Created by yuzo on 2/19/17.
//
//

#pragma once



#include <maya/MPxNode.h>

class DPSqrt final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	
	static MObject inDD;
	
	static MObject inSourceDataArray;
	static MObject aSourceAttribute;
	
	static MObject aIsOutAttribute;
	static MObject aIsDeleteOriginal;
	static MObject aOutAttribute;
	
	static MObject outDD;
	static MObject outDataArray;
	
	
};
