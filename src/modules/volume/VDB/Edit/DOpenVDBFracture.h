//
//  DOpenVDBFracture.hpp
//  dommyTool
//
//  Created by yuzo on 12/15/16.
//
//

#pragma once


#include <maya/MPxNode.h>



class DOpenVDBFracture final : public MPxNode
{
public:
	
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;

	static MObject aVdbInput;
	static MObject aVdbSelectedGridNames;
	
	static MObject aInObject;
	static MObject inDP;
	
	static MObject aSeparateCutters;
	static MObject aCutterOverlap;
	static MObject aCenterCutter;
	static MObject aRandomizeRotation;
	static MObject aSeed;
	static MObject aSegmentFragments;
	static MObject aFragmentgroup;
	
	static MObject aVdbOutput;
	
	
};
