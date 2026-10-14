//
//  DCUVMap.hpp
//  dommyTool
//
//  Created by yuzo on 1/29/17.
//
//

//カーブのUVmapを生成


#pragma once



#include <maya/MPxNode.h>


class DCUVMap final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	void postConstructor() override;
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDC;
	
	static MObject aUVAttributeName;
	
	static MObject aUVType;
	static MObject aCalcType;
    
	static MObject aUVRange;
	static MObject aUVRamp;
	
	static MObject aOffset;
	static MObject aScale;
	
	static MObject outDC;
	
private:
	
};
