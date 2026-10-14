//
//  DCDelete.hpp
//  dommyTool
//
//  Created by yuzo on 1/30/17.
//
//

//特定条件下のカーブを削除


#pragma once



#include <maya/MPxNode.h>


class DCDelete final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDC;
	
	static MObject aDeleteType;
	
	static MObject outDC;
	
private:
	
};
