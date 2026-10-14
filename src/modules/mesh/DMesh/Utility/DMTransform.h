//
//  DMTransform.hpp
//  dommyTool
//
//  Created by yuzo on 5/3/17.
//
//




#pragma once



#include <maya/MPxNode.h>

//DMeshのprimitive, faceのトランスフォーム
class DMTransform final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
	
	static MObject aAxisType;
	static MObject aAxisVectorAttribute;
	
	static MObject aTranslate;
	static MObject aRotate;
	static MObject aScale;
	static MObject aUniformScale;
	
	static MObject aTranslateWeight;
	static MObject aTranslateWeightAttribute;
	
	static MObject aRotateWeight;
	static MObject aRotateWeightAttribute;
	
	static MObject aScaleWeight;
	static MObject aScaleWeightAttribute;
	
	static MObject aWeight;
	static MObject aWeightAttribute;
	
	
//	static MObject transformMatrix;
	
	static MObject outDM;
	
	
private:
	
};
