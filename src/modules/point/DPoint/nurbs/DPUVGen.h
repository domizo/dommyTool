//
//  DPUVGenFromSurf.hpp
//  dommyTool
//
//  Created by yuzo on 3/1/16.
//
//


//インプットに接続したオブジェクトからUVアトリビュートを作成します。

#pragma once




#include <maya/MPxNode.h>



class DPUVGen final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	// Attributes
    static MObject inDD;
	
	static MObject inObject;
    static MObject uvName;
	
	
	static MObject outDD;
};


