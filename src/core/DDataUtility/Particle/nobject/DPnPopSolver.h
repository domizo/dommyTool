//
//  DPnPopSolver.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/19.
//



#pragma once




#include <vector>

#include <maya/MPxNode.h>
#include <maya/MnParticle.h>
#include <maya/MnSolver.h>
#include <maya/MnRigid.h>

#include "DPointDataHeaders.h"



//DPointをnParticleで計算
class DPnPopSolver final : public MPxNode
{
public:
    DPnPopSolver();
    ~DPnPopSolver() override = default;
    
    MStatus compute( const MPlug& plug, MDataBlock& block ) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    
    
    
    static const MTypeId typeId;
    
    //Particle
    MStatus ensureNParticle();
    MStatus initDPParticle(const DPData &dpoint, MDataBlock& data );
    MStatus updateCommonDPParticle( MDataBlock& data );
    
//    void updateDPParticle(DPData &dpoint, MDataBlock& data);
    
    
    //Solver
    MStatus initSolver(MDataBlock& data);
    MStatus calcSolver(MDataBlock& data, float solverTime);
    
    //Collision
    MStatus syncCollisions(MDataBlock& data);
    
    //field
    MStatus applyFields(double deltaTime);
    
    //attribute
    static MObject inDP;
    
    static MObject aStartTime;
    static MObject aCurrentTime;
//    static MObject prevDP;
//    static MObject nextDP;
    
    
    //particle attribute
    static MObject aVelocityAttribute;
    
    static MObject aRadius;
    static MObject aRadiusAttribute;
    
    static MObject aInverseMass;
    static MObject aMassAttribute;
    
    
    static MObject aBounce;
    static MObject aFriction;
    static MObject aDamping;
    
    
    
    
    
    //liquid Attribute
    static MObject aLiquidSimulation;
    static MObject aIncompressibility;
    static MObject aRestDensity;
    static MObject aLiquidRadiusScale;
    static MObject aViscosity;
    static MObject aSurfaceTension;
    
    //Collision
    static MObject aMaxIterations;
    static MObject aMaxSelfCollisionIterations;
    
    static MObject aSelfCollisionSoftness;
    static MObject aDragAndLift;
    static MObject aCollide;
    static MObject aSelfCollide;
    static MObject aSelfCollideWidth;
    
    
    //Solver
    static MObject aEnable;
    static MObject aSubsteps;
    static MObject aSolverIterations;
    
    static MObject aDisableGravity;
    static MObject aGravity;
    static MObject aGravityDirection;
    
    static MObject aAirDensity;
    static MObject aWindSpeed;
    static MObject aWindDirection;
    static MObject aWindNoiseIntensity;
    
    
    
    
    
    
    
    
    //Force
    static MObject aEnableForce;
    
    static MObject aForceAttribute;
    static MObject aForceEnvelope;
    static MObject aIsRealForce;
    
    
    //collision object
    static MObject aInCollideObject;
    
    
    //Pop
    static MObject inPOP;
    
    
    
    static MObject outDP;
    
protected:
    //cache
    MFloatPointArray m_positions;
    
    MnParticle  fNObject;
    MnSolver solver;
    bool m_particleInitialized;
    
    std::vector<MnRigid*> colObjList;
    
};
