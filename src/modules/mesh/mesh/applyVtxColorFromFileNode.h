//
//  applyVtxColorFromFileNode.h
//  dommyTool
//
//  Created by yuzo on 5/29/15.
//
//

#pragma once

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>



class applyVtxColorFromFileNode final : public MPxNode {
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
	
	
	
	
public:
	
	//attribute
	//input
	static MObject inputMesh;
	
	//for particle data
	static MObject filePath;
	
	static MObject frame;
	static MObject offset;
	static MObject padding;
	
	//output
	static MObject outputMesh;
	
};
