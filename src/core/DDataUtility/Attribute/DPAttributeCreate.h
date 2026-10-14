//
//  DPAttributeCreate.hpp
//  dommyTool
//
//  Created by yuzo on 5/23/16.
//
//

#pragma once

#include <maya/MPxNode.h>




class DPAttributeCreate final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	static MObject inDC;
    static MObject inDM;
    
	
	static MObject attributeDataList;
    
	static MObject attributeType;
	static MObject aAttributeClass;
	static MObject attributeName;
	
	
	static MObject defaultValue;
	static MObject aSetValue;
	static MObject valueSize;
	
	static MObject aPrecision;
	static MObject existActionType;
	
    static MObject inputData;
	
	
	static MObject outDP;
	static MObject outDC;
    static MObject outDM;
};

