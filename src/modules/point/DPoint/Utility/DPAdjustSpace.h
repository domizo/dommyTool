//
//  DPAdjustSpace.hpp
//  dommyTool
//
//  Created by yuzo on 11/13/16.
//
//

#pragma once


#include <maya/MPxNode.h>

class DPAdjustSpace final : public MPxNode
{
public:
	
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	static MObject aNeighborRadius;
	static MObject aIteration;
    static MObject aIsUniform;
    static MObject aRetainPercentage;
    
	static MObject outDP;
	
};

