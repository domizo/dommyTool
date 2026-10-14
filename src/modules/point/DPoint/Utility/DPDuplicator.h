//
//  DPDuplicator.hpp
//  dommyTool
//
//  Created by yuzo on 5/18/17.
//
//




#pragma once

#include <vector>

#include <maya/MPxNode.h>


//DPointをDPointを元に複製
class DPDuplicator final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	static MObject inDP;
	static MObject aTargetDP;
	
	static MObject outDP;
	
};
