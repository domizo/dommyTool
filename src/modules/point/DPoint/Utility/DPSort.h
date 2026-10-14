//
//  DPSort.hpp
//  dommyTool
//
//  Created by yuzo on 6/30/16.
//
//

#pragma once


#include <maya/MPxNode.h>

class DPSort final : public MPxNode
{
public:
	
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	static MObject aSortType;
	
	static MObject aIsReverse;
	
	static MObject aSeed;
	static MObject aOffset;
	static MObject aTargetPoint;
	static MObject aAlongVector;
	
	static MObject aAttributeName;
	static MObject aAttrComponent;
	
	
	static MObject outDP;
	
};

