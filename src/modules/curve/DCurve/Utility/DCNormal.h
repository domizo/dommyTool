//
//  DCNormal.hpp
//  dommyTool
//
//  Created by yuzo on 2/24/17.
//
//

//カーブのノーマルベクターを作成


#pragma once



#include <maya/MPxNode.h>


class DCNormal final : public MPxNode
{
public:
	
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDC;
	
	
	static MObject aNormalAttributeName;
	
	static MObject aGenerateType;
	
	static MObject aBaseVector;
	
	static MObject aTwistRamp;
	static MObject aTwistScale;
	
	
	static MObject aSeed;
	static MObject aRandomMin;
	static MObject aRandomMax;
	
	
	
	static MObject outDC;
	
private:
	
};


