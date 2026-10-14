//
//  DOpenVDBVectorSplit.hpp
//  dommyTool
//
//  Created by yuzo on 1/7/17.
//
//

#pragma once


#include <maya/MPxNode.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>

class DOpenVDBVectorSplit final : public MPxNode
{
public:
	
	
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject aVdbInput;
	
	static MObject aVdbSelectedGridNames;
	
	static MObject aRemoveSource;
	static MObject aCopyInactiveValue;
	
	static MObject aVdbOutput;
	
	
};

