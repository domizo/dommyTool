//
//  DMFaceInOut.hpp
//  dommyTool
//
//  Created by yuzo on 5/1/17.
//
//




#pragma once



#include <maya/MPxNode.h>

///DMeshの内側、外側かのチェック
///主にボロノイのメッシュに使用
class DMFaceInOut final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
	
	
	static MObject aInObject;
	
	static MObject aInnerName;
	static MObject aOuterName;
	
	static MObject aTolerance;
	
	static MObject outDM;
	
	
private:
	
};
