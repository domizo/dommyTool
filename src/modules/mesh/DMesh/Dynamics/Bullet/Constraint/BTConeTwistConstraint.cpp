//
//  BTConeTwistConstraint.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/27.
//

#include "BTConeTwistConstraint.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"




#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DDataOp.h"
#include "DNodeOperation.h"

#include "commonDBullet.h"
#include "commonBulletRigidBody.h"
#include "commonDBulletConstraint.h"

#include "dmesh_ids.h"


const MTypeId BTConeTwistConstraint::typeId( ID_BT_CONE_TWIST_CONSTRAINT );


MObject BTConeTwistConstraint::aLimitAngle;
MObject BTConeTwistConstraint::aSoftness;
MObject BTConeTwistConstraint::aBiasFactor;
MObject BTConeTwistConstraint::aRelaxationFactor;


MObject BTConeTwistConstraint::aUseBreakThreshold;
MObject BTConeTwistConstraint::aBreakThreshold;

MObject BTConeTwistConstraint::aOverrideIterations;

MObject BTConeTwistConstraint::aDebugDisplaySize;
MObject BTConeTwistConstraint::aDisableCollision;

MObject BTConeTwistConstraint::outConst;



void *BTConeTwistConstraint::creator() {
    return ::dommy::maya::createObject("BTConeTwistConstraint", [&]() {
        return new BTConeTwistConstraint();
    });
}




#pragma mark -
#pragma mark get param


MStatus BTConeTwistConstraint::evalParms(DbtConstraintParams& parms){
    
    parms.m_const_type = DBT_CONST_CONE_TWIST;
    
    
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aLimitAngle),
                      parms.m_limit_angle));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aSoftness), parms.m_softness));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBiasFactor), parms.m_biasFactor));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aRelaxationFactor), parms.m_relaxationFactor));
    
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseBreakThreshold), parms.m_use_break_threshold));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBreakThreshold), parms.m_break_threshold));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aOverrideIterations), parms.m_override_iterations));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDebugDisplaySize), parms.m_debug_display_size));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDisableCollision), parms.m_disable_colide));
    
    
    return MS::kSuccess;
}

DbtConstraintParams BTConeTwistConstraint::getParam() {
    DbtConstraintParams param;
    evalParms(param);
    
    return param;
}



#pragma mark -
#pragma mark compute

MStatus BTConeTwistConstraint::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug == outConst) {
        // Message attributes describe connections; they carry no bool payload.
        return data.setClean(plug);
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("BTConeTwistConstraint")




MStatus BTConeTwistConstraint::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    
    
    
    aLimitAngle = nAttr.create("LimitAngle", "lmtsngl", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(90.0,90.0,90.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLimitAngle));
    
    
    aSoftness = nAttr.create("Softness", "sftns", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSoftness));
    
    aBiasFactor = nAttr.create("BiasFactor", "bifac", MFnNumericData::kDouble, 0.3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(-1.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBiasFactor));
    
    aRelaxationFactor = nAttr.create("RelaxationFactor", "rlxfac", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRelaxationFactor));
    
    
    
    
    aUseBreakThreshold = nAttr.create("UseBreakThreshold", "usebrkthsd", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseBreakThreshold));
    
    aBreakThreshold = nAttr.create("BreakThreshold", "brkthsd", MFnNumericData::kDouble, 3.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBreakThreshold));
    
    
    
    aOverrideIterations = nAttr.create("OverrideIterations", "ovriter", MFnNumericData::kInt, -1, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setMin(-1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(50));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aOverrideIterations));
    
    
    //debug
    aDebugDisplaySize = nAttr.create("DebugDisplaySize", "dbgdspsz", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDebugDisplaySize));
    
    
    
    aDisableCollision = nAttr.create("DisableCollision", "disablecol", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisableCollision));
    
    
    //create custom data
    MFnMessageAttribute fnMsgAttr;
    outConst = fnMsgAttr.create("outConst", "ocnst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(fnMsgAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outConst));
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLimitAngle, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aSoftness, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBiasFactor, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRelaxationFactor, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseBreakThreshold, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBreakThreshold, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOverrideIterations, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDebugDisplaySize, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDisableCollision, outConst ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "BTConeTwistConstraint", "initialize", std::current_exception());
}
