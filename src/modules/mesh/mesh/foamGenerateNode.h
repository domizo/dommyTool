//
//  foamGenerateNode.h
//  dommyTool
//
//  Created by yuzo on 4/5/13.
//
//

#pragma once

#include <maya/MTypeId.h>
#include <maya/MDataBlock.h>
#include <maya/MPxNode.h>

class foamGenerateNode final : public MPxNode {
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
	
	//attribute
	
	//for particle data
	static MObject inPosition;
	static MObject radiusArray;
	
	//for ball
    static MObject frequency;
	static MObject radiusOffset;
	static MObject radiusResultOffset;
	
	//for deform
    static MObject selfDeform;
	
	
	
	//output
    static MObject outPutMesh;
};
