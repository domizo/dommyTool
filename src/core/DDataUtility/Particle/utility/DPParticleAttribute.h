//
//  DPParticleAttribute.hpp
//  dommyTool
//
//  Created by yuzo on 5/17/17.
//
//





#pragma once

#include <vector>

#include <maya/MPxNode.h>


//Particle Synamicsに必要なアトリビュートを追加
class DPParticleAttribute final : public MPxNode
{
public:
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	static MObject inDP;
	
	static MObject aIsVelocity;
    static MObject aVelocityType;
	static MObject aVelocityAttribute;
	static MObject aVelocityVector;
	static MObject aVelocityTargetAttribute;
	
	static MObject aIsRadius;
    static MObject aRadiusType;
	static MObject aRadiusAttribute;
	static MObject aRadius;
	static MObject aRadiusTargetAttribute;
	
	static MObject aIsMass;
    static MObject aMassType;
	static MObject aMassAttribute;
	static MObject aMass;
	static MObject aMassTargetAttribute;

	
	
	
	
	
	static MObject outDP;
	
};
