//
//  BTP2PConstraint.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/26.
//

#include "BTP2PConstraint.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "DNodeOperation.h"
#include "common/utility/DComputeGuard.h"



#include "DMayaHeaders.h"
#include "DMayaDebug.h"


#include "DDataOp.h"

#include "commonDBullet.h"
#include "commonBulletRigidBody.h"
#include "commonDBulletConstraint.h"

#include "dmesh_ids.h"


const MTypeId BTP2PConstraint::typeId( ID_BT_P2P_CONSTRAINT );


MObject BTP2PConstraint::aTau;
MObject BTP2PConstraint::aDamping;
MObject BTP2PConstraint::aImpluseClamp;

MObject BTP2PConstraint::aConstraintForceMixing;
MObject BTP2PConstraint::aErrorReductionParameter;

MObject BTP2PConstraint::aUseBreakThreshold;
MObject BTP2PConstraint::aBreakThreshold;

MObject BTP2PConstraint::aOverrideIterations;

MObject BTP2PConstraint::aDebugDisplaySize;
MObject BTP2PConstraint::aDisableCollision;

MObject BTP2PConstraint::outConst;



void *BTP2PConstraint::creator() {
    return ::dommy::maya::createObject("BTP2PConstraint", [&]() {
        return new BTP2PConstraint();
    });
}





#pragma mark -
#pragma mark get param


MStatus BTP2PConstraint::evalParms(DbtConstraintParams& parms){
    
    parms.m_const_type = DBT_CONST_P2P;
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aTau), parms.m_tau));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDamping), parms.m_const_angular_damping));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aImpluseClamp), parms.m_impulseClamp));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aConstraintForceMixing), parms.m_cfm));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aErrorReductionParameter), parms.m_erp));
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseBreakThreshold), parms.m_use_break_threshold));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBreakThreshold), parms.m_break_threshold));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aOverrideIterations), parms.m_override_iterations));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDebugDisplaySize), parms.m_debug_display_size));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDisableCollision), parms.m_disable_colide));
    
    
    return MS::kSuccess;
}


//db_constraint_base* BTP2PConstraint::createConstraint() {
//    
//    
//}

DbtConstraintParams BTP2PConstraint::getParam() {
    DbtConstraintParams param;
    evalParms(param);
    
    return param;
}


#pragma mark -
#pragma mark compute

MStatus BTP2PConstraint::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug == outConst) {
        // Message attributes describe connections; they carry no bool payload.
        return data.setClean(plug);
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("BTP2PConstraint")

MStatus BTP2PConstraint::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    
    
    // ----------------------- INPUTS --------------------------
    
    aTau = nAttr.create("Tau", "tau", MFnNumericData::kDouble, 0.3, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTau));
    
    aDamping = nAttr.create("Damping", "dmp", MFnNumericData::kDouble, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDamping));
    
    aImpluseClamp = nAttr.create("ImpluseClamp", "implclmp", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aImpluseClamp));
    
    
    aConstraintForceMixing = nAttr.create("ConstraintForceMixing", "cfm", MFnNumericData::kDouble, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aConstraintForceMixing));
    
    aErrorReductionParameter = nAttr.create("ErrorReductionParameter", "erp", MFnNumericData::kDouble, 0.2, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aErrorReductionParameter));
    
    
    
    
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
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTau, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDamping, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aImpluseClamp, outConst ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aConstraintForceMixing, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aErrorReductionParameter, outConst ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseBreakThreshold, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBreakThreshold, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOverrideIterations, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDebugDisplaySize, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDisableCollision, outConst ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "BTP2PConstraint", "initialize", std::current_exception());
}
