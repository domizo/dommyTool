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
// Archive member: dynamica/trunk/Extras/MayaPlugin/collisionShapeNode.h

//
//  DBCollisionShapeNode.hpp
//  dommyTool
//
//  Created by yuzo on 2017/09/28.
//


#pragma once



#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPxNode.h>

#include "dbCollisionShape_t.h"


class DBCollisionShapeNode final : public MPxNode
{
public:
    DBCollisionShapeNode();
    ~DBCollisionShapeNode() override;
    
    bool setInternalValueInContext(const MPlug& plug,
                                   const MDataHandle& dataHandle,
                                   MDGContext& ctx) override;
    
    MStatus compute(const MPlug& plug, MDataBlock& data) override;
    SchedulingType schedulingType() const override { return kUntrusted; }
    
    static  void *      creator();
    static  MStatus     initialize();
    
public:
    
    MStatus evaluate_collision_shape(
        dbCollisionShape_t::pointer& collisionShape);
    const dbCollisionShape_t::pointer& cached_collision_shape() const {
        return m_collision_shape;
    }
    
public:
    
    //Attributes
    static  MObject ia_shape;
    static  MObject ia_type;
    static  MObject ia_scale;
    static  MObject ca_collisionShape;
    static  MObject ca_collisionShapeParam;
    static  MObject oa_collisionShape;
    
public:
    static  MTypeId typeId;
    const static  MString typeName;
    
protected:
    MStatus computeCollisionShape(const MPlug& plug, MDataBlock& data);
    MStatus computeCollisionShapeParam(const MPlug& plug, MDataBlock& data);
    MStatus computeOutputShape(const MPlug& plug, MDataBlock& data);
    
    dbCollisionShape_t::pointer createCollisionShape(const MObject& node);
    dbCollisionShape_t::pointer createCompositeShape(const MPlug& plgInShape);
    
private:
    dbCollisionShape_t::pointer m_collision_shape;
    
};

