//
//  DPointToMeshGen.hpp
//  dommyTool
//
//  Created by yuzo on 2016/10/29.
//
//

#pragma once


#include <maya/MPxNode.h>



class DPToMesh final : public MPxNode
{
public:
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject inDP;
    
    static MObject aGenerateType;
    
    static MObject aNormalAttribute;
	static MObject aNormal;
	
	static MObject aMinAngle;
	static MObject aRadius;
	static MObject aDistance;
	
    static MObject outMesh;
    
};
