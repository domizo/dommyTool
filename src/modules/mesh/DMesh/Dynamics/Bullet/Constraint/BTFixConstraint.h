//
//  BTFixConstraint.hpp
//  dommyTool
//
//  Created by yuzo on 2017/10/27.
//


#pragma once

#include <vector>

#include <maya/MPxNode.h>

#include "btBulletCollisionCommon.h"
#include "btBulletDynamicsCommon.h"

#include "shared_ptr.h"
#include "dommyMath.h"

#include "db_constraint_base.h"


//BulletEnginのためのBTFixConstraintを生成
class BTFixConstraint final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    
    
    
    
    
    static MObject aUseBreakThreshold;
    static MObject aBreakThreshold;
    
    static MObject aOverrideIterations;
    
    static MObject aDebugDisplaySize;
    static MObject aDisableCollision;
    
    static MObject outConst;
    
public:
    
    MStatus evalParms(DbtConstraintParams& parms);
    //    db_constraint_base* createConstraint();
    DbtConstraintParams getParam();
    
private:
    
    
};
