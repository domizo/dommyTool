//
//  DPSmooth.hpp
//  dommyTool
//
//  Created by yuzo on 11/13/16.
//
//

#pragma once


#include <maya/MPxNode.h>

class DPSmooth final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
    static MObject aRadius;
    static MObject aMaxEffectiveRadius;
    static MObject aIteration;
	static MObject aExpandBound;
    
	static MObject outDP;
	
};
