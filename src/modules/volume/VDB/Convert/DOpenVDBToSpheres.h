//
//  DOpenVDBToSpheres.hpp
//  dommyTool
//
//  Created by yuzo on 2017/01/08.
//
//

#pragma once




#include <maya/MPxNode.h>



class DOpenVDBToSpheres final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    
    static MObject aIsovalue;
    static MObject aUseWorldSpaceUnits;
    static MObject aMinRadiusInVoxels;
    static MObject aMaxRadiusInVoxels;
	static MObject aMinRadiusInWorld;
	static MObject aMaxRadiusInWorld;
	
    static MObject aMaxSpheres;
    static MObject aScatterPoints;
    static MObject aOverlapping;
    static MObject aPreserveAttributesAndGroups;
    static MObject aAddIDAttribute;
    static MObject aAddPScaleAttribute;
	
	static MObject aMeshDetail;
	
    static MObject outMesh;
    
};


