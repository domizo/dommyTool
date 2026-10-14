//
//  DVDBPotentialFlow.hpp
//  dommyTool
//
//  Created by yuzo on 2018/02/22.
//




#pragma once
#include <maya/MPxNode.h>


///
class DVDBPotentialFlow final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    
    static MObject aVelocityGridName;
    static MObject aVelocityVdb;
    static MObject aMaskGridName;
    static MObject aMaskVdb;
    static MObject aMaskType;
    
    static MObject aUseIterations;
    static MObject aIterations;
    
    static MObject aUseTolerance;
    static MObject aTolerance;
    
    static MObject aUseWorldSpace;
    static MObject aDilationVoxels;
    static MObject aDilation;
    
    static MObject aUseBackgroundVelocity;
    static MObject aBackgroundVelocity;
    static MObject aApplyBackgroundVelocity;
    
    static MObject aOutputPotential;
    
    
    
    static MObject aVdbOutput;
    
};

