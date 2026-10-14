//
//  DCurveFromDP.hpp
//  dommyTool
//
//  Created by yuzo on 1/24/17.
//
//


#pragma once

#include <maya/MPxNode.h>

enum {
	DC_DP_GEN_ALL = 0,
	DC_DP_GEN_STEP,
	DC_DP_GEN_DISTANCE,
	DC_DP_GEN_RANDOM,
	DC_DP_GEN_TARGET,
	DC_DP_GEN_TYPE_COUNT
};typedef int DC_DP_GEN_TYPE;


enum {
	DC_DISTANCE_ALL = 0,
	DC_DISTANCE_ONE,
	DC_DISTANCE_METHOD_TYPE_COUNT
};typedef int DC_DISTANCE_METHOD_TYPE;

enum {
	DC_RANDOM_ALL = 0,
	DC_RANDOM_PAIR,
	DC_RANDOM_METHOD_TYPE_COUNT
};typedef int DC_RANDOM_METHOD_TYPE;




class DCurveFromDP final : public MPxNode
{
public:
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	
	static const MTypeId typeId;
	
public:
	
	static MObject inDP;
    static MObject aGroup;
	
	static MObject aTargetDP;
	
	static MObject aCreateType;
	static MObject aDegreeType;
	
	static MObject aIsClose;
	
	static MObject aStepCount;
	static MObject aSearchDistance;
	static MObject aSearchMaxCount;
	static MObject aDistanceMethod;
	static MObject aSeed;
	static MObject aRandomMethod;
	
	
	static MObject outDC;
	
private:
	
};



