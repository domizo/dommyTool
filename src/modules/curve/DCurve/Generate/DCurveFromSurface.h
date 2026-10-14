//
//  DCurveFromSurface.hpp
//  dommyTool
//
//  Created by yuzo on 5/1/16.
//
//




#pragma once

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MTypeId.h>

//サーフェイスの情報を元に複数のカーブを生成
class DCurveFromSurface final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	static MObject inSurface;
	
	static MObject numU;
	static MObject numV;
	
	static MObject sampling;
	
    static MObject offsetU;
	static MObject offsetV;
    static MObject lengthU;
    static MObject lengthV;
	
    static MObject isClampU;
    static MObject isClampV;
    
    static MObject aIsOpenU;
    static MObject aIsOpenV;
    
	static MObject outDC;
};
