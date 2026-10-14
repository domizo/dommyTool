//
//  DMToMesh.hpp
//  dommyTool
//
//  Created by yuzo on 2/10/17.
//
//

//DMeshをmayaのメッシュオブジェクトに変換

#pragma once


#include <maya/MPxNode.h>


class DMToMesh final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
    
    static MObject aConvertType;
    static MObject aGroup;
    
	
	static MObject outMesh;
	
private:
	
};
