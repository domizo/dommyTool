// DOMMY_THIRD_PARTY_NOTICE_BEGIN
/*
Bullet Continuous Collision Detection and Physics Library Maya Plugin
Copyright (c) 2008 Walt Disney Studios
 
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
 
Written by: Nicola Candussi <nicola@fluidinteractive.com>

Modified by Roman Ponomarev <rponom@gmail.com>
01/22/2010 : Constraints reworked

Modified by Dongsoo Han <dongsoo.han@amd.com>
04/11/2012 : Added contactCount, contactName and contactPosition attributes and void clearContactInfo(), void addContactInfo(..) 
			 in order to provide contact information through MEL script.
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/rigidBodyNode.h

//
//  DBRigidBodyNode.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//

#pragma once


#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxLocatorNode.h>

#include "dbSolver_t.h"

class DBRigidBodyNode final : public MPxLocatorNode
{
public:
    DBRigidBodyNode();
    ~DBRigidBodyNode() override;
    
    bool setInternalValueInContext(const MPlug& plug,
                                   const MDataHandle& dataHandle,
                                   MDGContext& ctx) override;
    
    //  virtual MStatus      setDependentsDirty ( const  MPlug & plug,  MPlugArray & plugArray);
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    void draw( M3dView & view, const MDagPath & path,
               M3dView::DisplayStyle style,
               M3dView::DisplayStatus status ) override;
    
    
    bool isBounded() const override;
    MBoundingBox boundingBox() const override;
    
    bool excludeAsLocator() const override { return false; }
    bool isTransparent() const override { return false; }
    
    static  void *      creator();
    static  MStatus     initialize();
    
public:
    
    MStatus evaluate_rigid_body(dbRigidBody_t::pointer& rigidBody);
    const dbRigidBody_t::pointer& cached_rigid_body() const {
        return m_rigid_body;
    }
    MStatus update();
    
public:
    
    //Attributes
    static  MObject     ia_collisionShape;
    static  MObject     ia_solver;
    static  MObject     ia_mass;
    static  MObject     ia_restitution;
    static  MObject     ia_friction;
    static  MObject     ia_linearDamping;
    static  MObject     ia_angularDamping;
    
    static  MObject     ia_initialPosition;
    static  MObject     ia_initialRotation;
    static  MObject     ia_initialVelocity;
    static  MObject     ia_initialSpin;
    
    static  MObject     ia_externalForce;
    static  MObject     ia_externalTorque;
    
    
    static  MObject     ca_rigidBody;
    static  MObject     ca_rigidBodyParam;
    static  MObject     ca_solver;
    
    static  MObject        oa_contactCount;
    static  MObject        oa_contactName;
    static  MObject        oa_contactPosition;
    
public:
    static  MTypeId     typeId;
    const static  MString     typeName;
    
    //private:
    void destroyRigidBody();
    MStatus computeRigidBody(const MPlug& plug, MDataBlock& data);
    MStatus computeWorldMatrix(const MPlug& plug, MDataBlock& data);
    MStatus computeRigidBodyParam(const MPlug& plug, MDataBlock& data);
    
    MStatus clearContactInfo();
    MStatus addContactInfo(
        const MString& contactObjectName,
        const MVector& point);
    
public:
    static void nodeRemoved(MObject& node, void *clientData);
    
private:
    dbRigidBody_t::pointer       m_rigid_body;
    int m_contactCount;
    
};
