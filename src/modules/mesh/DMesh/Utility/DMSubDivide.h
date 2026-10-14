//
//  DMSubdivide.hpp
//  dommyTool
//
//  Created by yuzo on 2017/05/07.
//
//
//DMeshのedge,faceの分割

#pragma once



#include <maya/MPxNode.h>


class DMSubDivide final : public MPxNode
{
public:
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    
    static  void *creator();
    static  MStatus initialize();
    
    static const MTypeId typeId;
    
public:
    
    
    static MObject inDM;
    
    static MObject aSubdivClassType;
//    static MObject aMethodType;
    static MObject aIteration;
	
	//Open subdiv option
	static MObject aWeight;
	static MObject aSchemeType;
	static MObject aInterpolationType;
	static MObject aFVarLinearInterpolationType;
	static MObject aCreasingMethod;
	static MObject aTriangleSubdivision;
	
    
    //edge option
    static MObject aEdgeSamplingType;
    static MObject aEdgeMaxIteration;
    static MObject aEdgeMinLength;
    static MObject aFaceGroup;
    
    static MObject outDM;
    
    
private:
    
};
