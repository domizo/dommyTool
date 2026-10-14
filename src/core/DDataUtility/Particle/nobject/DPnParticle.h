//
//  DPToNParticle.hpp
//  dommyTool
//
//  Created by yuzo on 2/6/17.
//
//




#pragma once

#include <vector>

#include <maya/MPxNode.h>
#include <maya/MnParticle.h>
#include <maya/MnSolver.h>
#include <maya/MnRigid.h>

#include "DPointDataHeaders.h"



//DPointをnParticleに変換
class DPnParticle final : public MPxNode
{
public:
	DPnParticle();
//	~DPToNParticle();
    
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	SchedulingType schedulingType() const override { return kUntrusted; }
	
	static  void *creator();
	static  MStatus initialize();
	
	
	
	static const MTypeId typeId;
	
	//Particle
    MStatus ensureNParticle();
	MStatus initializeDPDataParticle( DPData &dpoint, MDataBlock& data );
	
	MStatus updateDPDataParticle(DPData &dpoint, MDataBlock& data);
	
	
	//Solver
	MStatus initializeDPSolver(MDataBlock& data);
	MStatus calculateDPSolver(MDataBlock& data, float solverTime);
	
	
	//Collision
	MStatus syncCollisions(MDataBlock& data);
	
	
	
	
	//attribute
	static MObject inDP;
	
	static MObject aStartTime;
	static MObject aCurrentTime;
	static MObject prevDP;
	static MObject nextDP;
	
	
	
	static MObject aVelocityAttribute;
	
	static MObject aThickness;
	static MObject aThickAttribute;
	
	static MObject aInverseMass;
	static MObject aMassAttribute;
	
	static MObject aBounce;
	static MObject aFriction;
	static MObject aDamping;
	
	static MObject aDisableGravity;
	static MObject aLiquidSimulation;
	static MObject aIncompressibility;
	static MObject aRestDensity;
	static MObject aLiquidRadiusScale;
	static MObject aViscosity;
	static MObject aSurfaceTension;
	
	static MObject aMaxIterations;
	static MObject aMaxSelfCollisionIterations;
	
	static MObject aSelfCollisionSoftness;
	static MObject aDragAndLift;
	static MObject aCollide;
	static MObject aSelfCollide;
	static MObject aSelfCollideWidth;
    
    
    //Solver
    static MObject aGravity;
    static MObject aGravityDirection;
    static MObject aAirDensity;
    static MObject aWindSpeed;
    static MObject aWindDirection;
    static MObject aWindNoiseIntensity;
    static MObject aEnable;
    static MObject aSubsteps;
    static MObject aSolverIterations;
    
    
    //Emitter
    static MObject aIsEmit;
	
	static MObject aEmitterList;
    static MObject aEmitDP;
	static MObject aActiveEmit;
	
    
    
    //Force
    static MObject aEnableForce;
	
    static MObject aForceAttribute;
    static MObject aForceEnvelope;
    static MObject aIsRealForce;
    
	
	//collision object
	static MObject aInCollideObject;
	static MObject aInCollideDM;
	
	
	static MObject outDP;

protected:
    enum class PendingParticleSetup { None, Initialization, Update };
    PendingParticleSetup m_pendingParticleSetup = PendingParticleSetup::None;
    MStatus configureDPDataParticle(DPData& dpoint, MDataBlock& data, bool initializing);
	MnParticle  fNObject;
	MnSolver solver;
    bool m_particleInitialized;
	
	std::vector<MnRigid*> colObjList;
	
    
//    std::vector<float> *thickArray;
};
