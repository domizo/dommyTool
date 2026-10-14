//
//  DPAdd.hpp
//  dommyTool
//
//  Created by yuzo on 2/14/17.
//
//


#pragma once



#include <maya/MPxNode.h>

class DPAdd final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
    
    static MObject inDD;
	
    static MObject inSourceDataArray;
	static MObject aSourceAttribute;
	
	
	static MObject aConstantValue;
	static MObject aTargetAttribute;
    static MObject inTargetDataArray;
	
	
	static MObject aIsOutAttribute;
	static MObject aIsDeleteOriginal;
	static MObject aOutAttribute;
	static MObject outDD;
	
	static MObject outDataArray;
	
	
};

