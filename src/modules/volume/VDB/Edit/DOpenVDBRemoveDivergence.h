//
//  DOpenVDBRemoveDivergence.hpp
//  dommyTool
//
//  Created by yuzo on 2017/12/27.
//

#pragma once
#include <maya/MPxNode.h>


///
class DOpenVDBRemoveDivergence final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    
    static void* creator();
    static MStatus initialize();
    static const MTypeId typeId;
    
    
    static MObject aVdbInput;
    static MObject aVdbSelectedGridNames;
    
    static MObject aUseiterations;
    static MObject aIterations;
    static MObject aUsetolerance;
    static MObject aTolerance;
    static MObject aUsecollider;
    static MObject aCollidertype;
    static MObject aCollideVdb;
    static MObject aCollideVdbName;
    static MObject aInvertCollider;
    static MObject aPressure;
    
    static MObject aVdbOutput;
    
};
