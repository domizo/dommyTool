//
//  DMRemesh.hpp
//  dommyTool
//
//  Created by yuzo on 4/16/17.
//
//





#pragma once



#include <maya/MPxNode.h>

//DMeshのトポロジーの調整
class DMRemesh final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	
	static MObject inDM;
    
    static MObject aMethodType;
    
	static MObject aEdgeLength;
    static MObject aAspectBound;
	static MObject aIterate;
    
    
	static MObject outDM;
	
	
private:
	
};
