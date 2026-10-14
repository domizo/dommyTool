//
//  DCReplicator.hpp
//  dommyTool
//
//  Created by yuzo on 3/6/17.
//
//




#pragma once



#include <maya/MPxNode.h>

//DCurveを元の形状のと同じカーブを指定して半径内にたくさん複製します。
class DCReplicator final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	static MObject inDC;
	
	static MObject aCount;
	
    
	static MObject aRadius;
	static MObject aGenerateType;
	
	static MObject outDC;
	
private:
	
};
