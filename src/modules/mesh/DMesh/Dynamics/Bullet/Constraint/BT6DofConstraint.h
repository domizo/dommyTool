//
//  BT6DofConstraint.hpp
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


//BulletEnginのためのBT6DofConstraintを生成
class BT6DofConstraint final : public MPxNode
{
public:
    
    MStatus compute(const MPlug& plug, MDataBlock& block) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *creator();
    static  MStatus initialize();
    static const MTypeId typeId;
    
    //angle
    static MObject aLinearLowerLimit;
    static MObject aLinearUpperLimit;
    static MObject aAngularLowerLimit;
    static MObject aAngularUpperLimit;
    
    
    //shape spring
    static MObject aUseSpring;
    static MObject aUseUniformStiffness;
    static MObject aUseUniformDamping;
    static MObject aUniformStiffness;
    static MObject aUniformDamping;
    
    static MObject aTranslateStiffness;
    static MObject aRotateStiffness;
    static MObject aTranslateDamping;
    static MObject aRotateDamping;
    
    
    
    //common
    static MObject aUseBreakThreshold;
    static MObject aBreakThreshold;
    
    static MObject aOverrideIterations;
    
    static MObject aDebugDisplaySize;
    static MObject aDisableCollision;
    
    //output
    static MObject outConst;
    
public:
    
    MStatus evalParms(DbtConstraintParams& parms);
    //    db_constraint_base* createConstraint();
    DbtConstraintParams getParam();
    
private:
    
    
};
