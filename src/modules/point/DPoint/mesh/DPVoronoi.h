//
//  DPDelaunayMesh.hpp
//  dommyTool
//
//  Created by yuzo on 7/14/16.
//
//


#pragma once

#include <maya/MPxNode.h>


class DPVoronoi final : public MPxNode
{
public:
	MStatus compute(const MPlug& plug, MDataBlock& block) override;
	SchedulingType schedulingType() const override { return kSerial; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	static MObject inDP;
	static MObject inObject;
    
    static MObject aGenerateType;
	
//    static MObject aCreateInsideSurface;
	
	static MObject aCellSpace;
	static MObject aCellPeriodic;
	
	static MObject aUseCropObject;
	static MObject aIsCheckCell;
	static MObject aTolerance;
	
	static MObject aCellContainerType;
	
	static MObject aCheckSamePoint;
	
	
    //color set
//    static MObject aColorType;
	
//	static MObject testPlaneVec;
//	static MObject testPlanePos;
//    static MObject testCutPointIndex;
//    static MObject testmaxCutNum;
    
//    static MObject aOutMeshType;
	
//    static MObject outMesh;
	static MObject outDC;
	static MObject outDM;
};



