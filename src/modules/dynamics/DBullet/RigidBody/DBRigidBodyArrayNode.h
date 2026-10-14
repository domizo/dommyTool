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
*/
// DOMMY_THIRD_PARTY_NOTICE_END
// Modified for dommyTool; not the original upstream version.
// Source comparison reference: https://storage.googleapis.com/google-code-archive-source/v2/code.google.com/dynamica/source-archive.zip
// Archive member: dynamica/trunk/Extras/MayaPlugin/rigidBodyArrayNode.h

//
//  DBRigidBodyArrayNode.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//


#pragma once

#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MMatrix.h>

#include <vector>

#include "dbSolver_t.h"

class DBRigidBodyArrayNode final : public MPxLocatorNode
{
public:
    DBRigidBodyArrayNode();
    ~DBRigidBodyArrayNode() override;
    
    void postConstructor() override;
    
    bool setInternalValueInContext(
        const MPlug& plug,
        const MDataHandle& dataHandle,
        MDGContext& ctx) override;
    
    //  virtual MStatus      setDependentsDirty ( const  MPlug & plug,  MPlugArray & plugArray);
    
    MStatus compute( const MPlug& plug, MDataBlock& data ) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    void draw(M3dView& view,
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
    
    MStatus evaluate_rigid_bodies(
        const std::vector<dbRigidBody_t::pointer>*& rigidBodies);
    const std::vector<dbRigidBody_t::pointer>&
        cached_rigid_bodies() const {
        return m_rigid_bodies;
    }
    
public:
    
    //Attributes
    static  MObject     ia_collisionShape;
    static  MObject     ia_solver;
    static  MObject     ia_numBodies;
    //ia_active is obsolete and redundant -> use ia_mass == 0 for (ia_active==false), and ia_mass > 0 for (ia_active == true)
    static  MObject     ia_mass;
    static  MObject     ia_restitution;
    static  MObject     ia_friction;
    static  MObject     ia_linearDamping;
    static  MObject     ia_angularDamping;
    
    static  MObject     ia_initialPosition;
    static  MObject     ia_initialRotation;
    static  MObject     ia_initialVelocity;
    static  MObject     ia_initialSpin;
    
    static  MObject     ia_fileIO;
    static  MObject     ia_fioFiles;
    
    static  MObject     io_position;
    static  MObject     io_rotation;
    
    static  MObject     ca_rigidBodies;
    static  MObject     ca_rigidBodyParam;
    static  MObject     ca_solver;
    
public:
    static MTypeId typeId;
    const static MString typeName;
    
    //private:
    MStatus update();
    void destroyRigidBodies();
    MStatus computeRigidBodies(const MPlug& plug, MDataBlock& data);
    MStatus computeRigidBodyParam(const MPlug& plug, MDataBlock& data);
    MStatus computeWorldMatrix(const MPlug& plug, MDataBlock& data);
    
public:
    static void nodeRemoved(MObject& node, void *clientData);
    
private:
    std::vector<dbRigidBody_t::pointer> m_rigid_bodies;
    MMatrix m_worldMatrix;
    
    size_t m_numRigidBodies;
    
};
