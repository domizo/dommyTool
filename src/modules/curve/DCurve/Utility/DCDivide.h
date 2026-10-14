//
//  DCDivide.hpp
//  dommyTool
//
//  Created by yuzo on 1/25/17.
//
//




#pragma once



#include <maya/MPxNode.h>

//カーブを分割
class DCDivide final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDC;
	
	static MObject aDivideType;
	
	static MObject aCount;
    static MObject aSampling;
    static MObject aOffset;
	
	static MObject aSpaceLength;
	static MObject aStepLength;
	
	static MObject aIsEdgeSpace;
    
	
	
	
	
	static MObject outDC;
	
private:
	
};




