//
//  DOpenVDBTransform.hpp
//  dommyTool
//
//  Created by yuzo on 1/1/17.
//
//


#pragma once


#include <maya/MPxNode.h>



class DOpenVDBTransform final : public MPxNode
{
public:
	
	
	
	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	
	static void* creator();
	static MStatus initialize();
	static const MTypeId typeId;
	
	
	
	static MObject aVdbInput;
	
	static MObject aVdbSelectedGridNames;
	static MObject aTranslate;
	static MObject aRotate;
	static MObject aScale;
	static MObject aPivot;
	static MObject aUniformScale;
	static MObject aInvert;
	
	static MObject aVdbOutput;
	
	
protected:
	
	
};
