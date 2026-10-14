//
//  DMGroupDelete.hpp
//  dommyTool
//
//  Created by yuzo on 5/4/17.
//
//
//DMeshのグループを削除


#pragma once



#include <maya/MPxNode.h>



class DMGroupDelete final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static MTypeId typeId;
	
	
public:
	
	
	static MObject inDM;
	
	static MObject aIsEnable;
	static MObject aPrimitiveGroupName;
	static MObject aPointGroupName;
	static MObject aFaceGroupName;
	static MObject aEdgeGroupName;
	
	
	static MObject outDM;
	
	
private:
	
};
