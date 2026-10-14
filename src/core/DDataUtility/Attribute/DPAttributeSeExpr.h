//
//  DPAttributeSeExpr.hpp
//  dommyTool
//
//  Created by yuzo on 5/22/17.
//
//



#pragma once

#include <maya/MPxNode.h>
#include "DPointDataHeaders.h"


///DPointのアトリビュートをSeExprを使用して作成
class DPAttributeSeExpr final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	static MObject inDD;
	
	static MObject aAttributeName;
	static MObject aAttributeType;
	
	static MObject aExpression;
	
	
	static MObject aUserFloat1;
	static MObject aUserFloat2;
	static MObject aUserFloat3;
	static MObject aUserFloat4;
    
    static MObject aUserVector1;
    static MObject aUserVector2;
    static MObject aUserVector3;
    static MObject aUserVector4;
	
	static MObject aUserColor1;
	static MObject aUserColor2;
	static MObject aUserColor3;
	static MObject aUserColor4;
	
	static MObject outDD;
};
