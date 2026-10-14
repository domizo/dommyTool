//
//  DMAttributeCopy.hpp
//  dommyTool
//
//  Created by yuzo on 5/11/17.
//
//

//DMeshのアトリビュートをDPoint等から複製


#pragma once



#include <maya/MPxNode.h>


class DMAttributeCopy final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
	
	static MObject aTargetDP;
	
	static MObject aAttributeNames;
	
	
	static MObject outDM;
	
	
private:
	
};
