//
//  DCReform.hpp
//  dommyTool
//
//  Created by yuzo on 1/25/17.
//
//



//カーブの長さ、位置のオフセットの変更等


#pragma once



#include <maya/MPxNode.h>


class DCReform final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDC;
	
	static MObject aLength;
	static MObject aSampling;
	static MObject aOffset;
	
    static MObject isClamp;
    
	static MObject aLengthAttributeName;
	static MObject aOffsetAttributeName;
	
	static MObject outDC;
	
private:
	
};


