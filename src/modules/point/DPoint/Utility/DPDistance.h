//
//  DPDistant.hpp
//  dommyTool
//
//  Created by yuzo on 3/11/16.
//
//

#pragma once

#include <maya/MPxNode.h>

class DPDistance final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDD;
    
    static MObject distAttrName;
    
    static MObject aComponentType;
    
    
    static MObject targetType;
    
    
    
    static MObject targetPoint;
    static MObject inObject;
    static MObject inTargetDP;
	
    
    static MObject aAddNearID;
    static MObject aNearIDAttributeName;
    
	static MObject outDD;
	
};

