//
//  DOpenVDBVectorMerge.hpp
//  dommyTool
//
//  Created by yuzo on 1/7/17.
//
//

#pragma once


#include <maya/MPxNode.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>

class DOpenVDBVectorMerge final : public MPxNode
{
public:
	
	
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	
	static void addWarningMessage(DOpenVDBVectorMerge* self, const char* msg)
	{ if (self && msg) MGlobal::displayInfo(MString(msg)); }
	
	
	static MObject aVdbInput;
	
	static MObject aXGroup;
	static MObject aYGroup;
	static MObject aZGroup;
	
	static MObject aUseXName;
	static MObject aMergedName;
	static MObject aVectorType;
	
	static MObject aRemoveSource;
	static MObject aCopyInactiveValue;
	
	
	static MObject aVdbOutput;
	
	
	
protected:
	
	
};


