//
//  BTFixConstraint.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/27.
//

#include "BTFixConstraint.h"
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


const MTypeId BTFixConstraint::typeId( ID_BT_FIX_CONSTRAINT );


MObject BTFixConstraint::aUseBreakThreshold;
MObject BTFixConstraint::aBreakThreshold;

MObject BTFixConstraint::aOverrideIterations;

MObject BTFixConstraint::aDebugDisplaySize;
MObject BTFixConstraint::aDisableCollision;

MObject BTFixConstraint::outConst;



void *BTFixConstraint::creator() {
    return ::dommy::maya::createObject("BTFixConstraint", [&]() {
        return new BTFixConstraint();
    });
}


#pragma mark -
#pragma mark get param


MStatus BTFixConstraint::evalParms(DbtConstraintParams& parms){
    
    parms.m_const_type = DBT_CONST_FIX;
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseBreakThreshold), parms.m_use_break_threshold));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBreakThreshold), parms.m_break_threshold));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aOverrideIterations), parms.m_override_iterations));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDebugDisplaySize), parms.m_debug_display_size));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDisableCollision), parms.m_disable_colide));
    
    
    return MS::kSuccess;
}

DbtConstraintParams BTFixConstraint::getParam() {
    DbtConstraintParams param;
    evalParms(param);
    
    return param;
}



#pragma mark -
#pragma mark compute

MStatus BTFixConstraint::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug == outConst) {
        // Message attributes describe connections; they carry no bool payload.
        return data.setClean(plug);
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("BTFixConstraint")





MStatus BTFixConstraint::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    
    
    
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
    
    
    
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseBreakThreshold, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBreakThreshold, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOverrideIterations, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDebugDisplaySize, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDisableCollision, outConst ));
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "BTFixConstraint", "initialize", std::current_exception());
}

