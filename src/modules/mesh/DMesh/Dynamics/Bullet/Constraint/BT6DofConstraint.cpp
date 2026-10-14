//
//  BT6DofConstraint.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/27.
//

#include "BT6DofConstraint.h"
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




const MTypeId BT6DofConstraint::typeId( ID_BT_6DOF_CONSTRAINT );


MObject BT6DofConstraint::aLinearLowerLimit;
MObject BT6DofConstraint::aLinearUpperLimit;
MObject BT6DofConstraint::aAngularLowerLimit;
MObject BT6DofConstraint::aAngularUpperLimit;


//shape spring
MObject BT6DofConstraint::aUseSpring;
MObject BT6DofConstraint::aUseUniformStiffness;
MObject BT6DofConstraint::aUseUniformDamping;
MObject BT6DofConstraint::aUniformStiffness;
MObject BT6DofConstraint::aUniformDamping;

MObject BT6DofConstraint::aTranslateStiffness;
MObject BT6DofConstraint::aRotateStiffness;
MObject BT6DofConstraint::aTranslateDamping;
MObject BT6DofConstraint::aRotateDamping;



//common
MObject BT6DofConstraint::aUseBreakThreshold;
MObject BT6DofConstraint::aBreakThreshold;

MObject BT6DofConstraint::aOverrideIterations;

MObject BT6DofConstraint::aDebugDisplaySize;
MObject BT6DofConstraint::aDisableCollision;

//output
MObject BT6DofConstraint::outConst;



void *BT6DofConstraint::creator() {
    return ::dommy::maya::createObject("BT6DofConstraint", [&]() {
        return new BT6DofConstraint();
    });
}



#pragma mark -
#pragma mark get param


MStatus BT6DofConstraint::evalParms(DbtConstraintParams& parms){
    
    
    
    
    
    
    //angle
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aLinearLowerLimit),
                      parms.m_linear_lower_limit));
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aLinearUpperLimit),
                      parms.m_linear_upper_limit));
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aAngularLowerLimit),
                      parms.m_angular_lower_limit));
    DOMMY_RETURN_IF_FAILURE(
        readPlugValue(MPlug(thisMObject(), aAngularUpperLimit),
                      parms.m_angular_upper_limit));
    
    
    //spring
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseSpring), parms.m_use_spring));
    if(parms.m_use_spring) {
        
        parms.m_const_type = DBT_CONST_6DOF_SPRING;//type
        
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseUniformStiffness), parms.m_use_uniform_stiffness));
        
        if (parms.m_use_uniform_stiffness) {
            
            double stiff = 0.0;
            DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUniformStiffness), stiff));
            parms.m_translate_stiffness = MVector(stiff,stiff,stiff);
            parms.m_rotate_stiffness = MVector(stiff,stiff,stiff);
        } else {
            
            DOMMY_RETURN_IF_FAILURE(
                readPlugValue(MPlug(thisMObject(), aTranslateStiffness),
                              parms.m_translate_stiffness));
            DOMMY_RETURN_IF_FAILURE(
                readPlugValue(MPlug(thisMObject(), aRotateStiffness),
                              parms.m_rotate_stiffness));
        }
        
        DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseUniformDamping), parms.m_use_uniform_damping));
        
        if (parms.m_use_uniform_damping) {
            
            double damp = 0.0;
            DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUniformDamping), damp));
            parms.m_translate_damping = MVector(damp,damp,damp);
            parms.m_rotate_damping = MVector(damp,damp,damp);
        } else {
            
            DOMMY_RETURN_IF_FAILURE(
                readPlugValue(MPlug(thisMObject(), aTranslateDamping),
                              parms.m_translate_damping));
            DOMMY_RETURN_IF_FAILURE(
                readPlugValue(MPlug(thisMObject(), aRotateDamping),
                              parms.m_rotate_damping));
        }
        
    } else {
        parms.m_const_type = DBT_CONST_6DOF;//type
    }
    
    
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aUseBreakThreshold), parms.m_use_break_threshold));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aBreakThreshold), parms.m_break_threshold));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aOverrideIterations), parms.m_override_iterations));
    
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDebugDisplaySize), parms.m_debug_display_size));
    DOMMY_RETURN_IF_FAILURE(readPlugValue(MPlug(thisMObject(), aDisableCollision), parms.m_disable_colide));
    
    
    return MS::kSuccess;
}

DbtConstraintParams BT6DofConstraint::getParam() {
    DbtConstraintParams param;
    evalParms(param);
    
    return param;
}


#pragma mark -
#pragma mark compute

MStatus BT6DofConstraint::compute(const MPlug& plug, MDataBlock& data) try
{
    if (plug == outConst) {
        // Message attributes describe connections; they carry no bool payload.
        return data.setClean(plug);
    }
    return MS::kUnknownParameter;
}
DOMMY_COMPUTE_CATCH("BT6DofConstraint")


MStatus BT6DofConstraint::initialize() try {
    MStatus stat;
    MFnNumericAttribute nAttr;
    MFnGenericAttribute gAttr;
    MFnTypedAttribute tAttr;
    MFnStringData fnStringData;
    
    
    
    //angle
    aLinearLowerLimit = nAttr.create("LinearLowerLimit", "linlowlmt", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLinearLowerLimit));
    
    aLinearUpperLimit = nAttr.create("LinearUpperLimit", "linuplmt", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLinearUpperLimit));
    
    aAngularLowerLimit = nAttr.create("AngularLowerLimit", "angllowlmt", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAngularLowerLimit));
    
    aAngularUpperLimit = nAttr.create("AngularUpperLimit", "angluplmt", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0,0.0,0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAngularUpperLimit));
    
    
    //spring
    aUseSpring = nAttr.create("UseSpring", "usesprng", MFnNumericData::kBoolean, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseSpring));
    
    aUseUniformStiffness = nAttr.create("UseUniformStiffness", "usunistif", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseUniformStiffness));
    
    aUniformStiffness = nAttr.create("UniformStiffness", "unistif", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(40.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(200.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformStiffness));
    
    aUseUniformDamping = nAttr.create("UseUniformDamping", "usunidmp", MFnNumericData::kBoolean, true, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUseUniformDamping));
    
    aUniformDamping = nAttr.create("UniformDamping", "unidmp", MFnNumericData::kDouble, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(10.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aUniformDamping));
    
    
    aTranslateStiffness = nAttr.create("TranslateStiffness", "transstif", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(40.0,40.0,40.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslateStiffness));
    
    aRotateStiffness = nAttr.create("RotateStiffness", "rotstif", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(40.0,40.0,40.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateStiffness));
    
    aTranslateDamping = nAttr.create("TranslateDamping", "transdmp", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01,0.01,0.01));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aTranslateDamping));
    
    aRotateDamping = nAttr.create("RotateDamping", "rotdmp", MFnNumericData::k3Double, 0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.01,0.01,0.01));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRotateDamping));
    
    
    
    //common
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
    
    
    
    
    
    //angle
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLinearLowerLimit, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aLinearUpperLimit, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngularLowerLimit, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aAngularUpperLimit, outConst ));
    
    
    //spring
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseSpring, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseUniformStiffness, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseUniformDamping, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformStiffness, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUniformDamping, outConst ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslateStiffness, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotateStiffness, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aTranslateDamping, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aRotateDamping, outConst ));
    
    
    
    //common
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aUseBreakThreshold, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aBreakThreshold, outConst ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aOverrideIterations, outConst ));
    
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDebugDisplaySize, outConst ));
    DOMMY_RETURN_IF_FAILURE(attributeAffects( aDisableCollision, outConst ));
    
    
 
    
    return MS::kSuccess;
}
catch (...) {
    return dommy::maya::reportEntryPointException(
        "BT6DofConstraint", "initialize", std::current_exception());
}

