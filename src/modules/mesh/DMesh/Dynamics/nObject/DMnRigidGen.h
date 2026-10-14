//
//  DMNObjectGen.hpp
//  dommyTool
//
//  Created by yuzo on 5/15/17.
//
//





#pragma once

#include <maya/MPxNode.h>
#include <maya/MnRigid.h>

//DMeshからnObjectを作成
class DMnRigidGen final : public MPxNode
{
public:
	
	DMnRigidGen();
	
	
	MStatus compute( const MPlug& plug, MDataBlock& block ) override;
	SchedulingType schedulingType() const override { return kUntrusted; }
	
	static  void *creator();
	static  MStatus initialize();
	static const MTypeId typeId;
	
	
	
	
	static MObject inDM;
	
	static MObject aIsPassive;
    
    static MObject aThickness;
    static MObject aBounce;
    static MObject aFriction;
    
    static MObject aCollideVertex;
    static MObject aCollideEdge;
    static MObject aCollideFace;
    
	static MObject outNObject;
	
	
	
	MnRigid fNObject;

private:
    MStatus creationStatus_;
    bool topologyInitialized_ = false;
	
};


