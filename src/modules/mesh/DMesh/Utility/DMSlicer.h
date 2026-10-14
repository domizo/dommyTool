//
//  DMSlicer.hpp
//  dommyTool
//
//  Created by yuzo on 4/23/17.
//
//





#pragma once



#include <maya/MPxNode.h>

//DMeshをスライスしてDCurveを生成
class DMSlicer final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
	
	static MObject aPlanePosition;
	static MObject aPlaneNormal;
	
	static MObject aCutCount;
	static MObject aStep;
	
	
	//curve charactor
	static MObject aDegree;
	static MObject aIsOpen;
	static MObject aIsMultiEndKnot;
	
	static MObject outDC;
	
	
private:
	
};
