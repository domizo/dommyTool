// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Herbert Law
 
This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising
from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:
 
1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must
not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
 
Written by: Herbert Law <Herbert.Law@gmail.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/constraint/hingeConstraintNode.h

//
//  DBHingeConstraintNode.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#pragma once


#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxLocatorNode.h>


#include "dbSolver_t.h"



class DBHingeConstraintNode final : public MPxLocatorNode
{
public:
    DBHingeConstraintNode();
    ~DBHingeConstraintNode() override;
    
    bool setInternalValueInContext(
        const MPlug& plug,
        const MDataHandle& dataHandle,
        MDGContext& ctx) override;
    
    //  virtual MStatus      setDependentsDirty ( const  MPlug & plug,  MPlugArray & plugArray);
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    void draw(
        M3dView& view,
        const MDagPath& path,
        M3dView::DisplayStyle style,
        M3dView::DisplayStatus status) override;
    
    
    bool isBounded() const override;
    MBoundingBox boundingBox() const override;
    
    bool excludeAsLocator() const override { return false; }
    bool isTransparent() const override { return false; }
    
    static  void *      creator();
    static  MStatus     initialize();
    
public:
    
    MStatus evaluate_constraint(
        dbHinge_constraint_t::pointer& constraint);
    const dbHinge_constraint_t::pointer& cached_constraint() const {
        return m_constraint;
    }
    MStatus update();
    
public:
    
    //Attributes
    static MObject ia_rigidBodyA;
    static MObject ia_rigidBodyB;
    static MObject ia_damping;
    static MObject ia_breakThreshold;
    static MObject ia_disableCollide;
    
    static MObject ia_lowerLimit;
    static MObject ia_upperLimit;
    static MObject ia_limitSoftness;
    static MObject ia_biasFactor;
    static MObject ia_relaxationFactor;
    
    static MObject ia_rotationInA;
    static MObject ia_rotationInB;
    
    static MObject ia_pivotInA;
    static MObject ia_pivotInB;
    
    static MObject ca_constraint;
    static MObject ca_constraintParam;
    
    static MObject ia_enableAngularMotor;
    static MObject ia_motorTargetVelocity;
    static MObject ia_maxMotorImpulse;
    
public:
    static MTypeId typeId;
    const static MString typeName;
    
    void destroyConstraint();
    MStatus computeConstraint(const MPlug& plug, MDataBlock& data);
    MStatus computeConstraintParam(const MPlug& plug, MDataBlock& data);
    MStatus computeWorldMatrix(const MPlug& plug, MDataBlock& data);
    
public:
    static void nodeRemoved(MObject& node, void *clientData);
    
private:
    MStatus applyConstraintParameters(MDataBlock& data, const dbHinge_constraint_t::pointer& constraint);
    dbHinge_constraint_t::pointer m_constraint;
    
    vec3f m_PivInA;
    vec3f m_PivInB;
    quatf m_RotInA;
    quatf m_RotInB;
    
};
