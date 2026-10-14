//
//  DVDBFoamGen.hpp
//  dommyTool
//
//  Created by yuzo on 2/7/17.
//
//

//泡生成

#pragma once


#include <maya/MPxNode.h>


class DVDBFoamGen final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	
	static MObject aRadius;
	static MObject aRadiusOffset;
	
	static MObject aVoxelSize;
	static MObject aBandWidth;
	
	static MObject aVdbOut;
	
	
};
