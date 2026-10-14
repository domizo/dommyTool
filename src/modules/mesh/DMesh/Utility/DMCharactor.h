//
//  DMCharactor.hpp
//  dommyTool
//
//  Created by yuzo on 4/28/17.
//
//





#pragma once



#include <maya/MPxNode.h>

//DMeshの特徴をデータ化する。
class DMCharactor final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
	
	
	static MObject aAddCenter;
	static MObject aCenterAttributeName;
	
	
	static MObject aAddCurvature;
	static MObject aCurvatureAttributeName;
	static MObject aColorMap;
	static MObject aSmoothIterate;
	static MObject aCurvatureOutputType;
    
	
	static MObject outDM;
	
	
private:
	
};
