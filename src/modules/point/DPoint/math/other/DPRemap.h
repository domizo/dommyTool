//
//  DPRemap.hpp
//  dommyTool
//
//  Created by yuzo on 2/23/17.
//
//

#pragma once

#include <maya/MPxNode.h>

#include "DMayaHeaders.h"
#include "DPData.h"


class DPRemap final : public MPxNode
{
public:
	void postConstructor() override;
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	static MObject inDD;
	
	static MObject inSourceDataArray;
	static MObject aSourceAttribute;
	
	
	static MObject aRamp1D;
	static MObject aRamp2D;
	static MObject aRamp3D;
    
    static MObject aRange1D;
    static MObject aRange2D;
	static MObject aRange3D;
    
    
    static MObject aKeepRange;
    
	static MObject aIsOutAttribute;
	static MObject aIsDeleteOriginal;
	static MObject aOutAttribute;
	
	static MObject outDD;
	
	static MObject outDataArray;
};



