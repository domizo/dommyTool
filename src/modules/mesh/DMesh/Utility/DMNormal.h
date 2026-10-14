//
//  DMNormal.hpp
//  dommyTool
//
//  Created by yuzo on 5/23/17.
//
//
//DMeshのNormalの調整



#pragma once

#include <maya/MPxNode.h>







class DMNormal final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	
	static MObject inDM;
	
    static MObject aIsAddAttribute;
    static MObject aAttributeName;
	static MObject aNormalType;
    static MObject aFaceGroup;
    static MObject aIsGroupEdgeOnly;
    static MObject aIsConcernCorner;
    
    
    
    
	static MObject aReverse;
	
	
	
	static MObject outDM;
	
};

