//
//  DPArraySize.hpp
//  dommyTool
//
//  Created by yuzo on 3/5/16.
//
//

//インプットデータのサイズを返します。


#pragma once

#include <maya/MPxNode.h>

class DPArraySize final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
    static MObject inDD;
    
	static MObject inObject;
	static MObject inputPointArray;
    
    static MObject aCountType;
	
	static MObject resultCount;
	
};


