//
//  DPFillSpace.h
//  dommyTool
//
//  Created by yuzo on 4/7/15.
//
//

//ポイント間の距離やIDを元にポイントを追加します


#pragma once

#include <maya/MPxNode.h>

enum {
	DP_FILL_COUNT = 0,
	DP_FILL_SPACE,
	DP_FILL_SAMPLE_TYPE_COUNT
};typedef int DP_FILL_SAMPLE_TYPE;

class DPFillSpace final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	
	static MObject samplingType;
	static MObject samplingCount;
	static MObject samplingSpace;
	
	static MObject isIDStepFill;
	static MObject nextStepSize;
	
	
	static MObject isDistanceFill;
	static MObject distance;
	
	
	
	static MObject outDP;
	
};

