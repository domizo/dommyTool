//
//  DMCopyUV.hpp
//  dommyTool
//
//  Created by yuzo on 5/10/17.
//
//

//DMeshのUVをinputobjectから参照、生成する

#pragma once



#include <maya/MPxNode.h>


class DMCopyUV final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
	
	
	static MObject aInObject;
	
	
	static MObject outDM;
	
	
private:
	
};
