#pragma once
#include <maya/MStatus.h>
#include <cassert>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef DOMMY_SDK_CHECK
#include <maya/MDataBlock.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MTypeId.h>
#else
namespace fixture {
std::vector<std::string> calls;
std::vector<int> updates;
std::map<int,bool> connections;
int failAt=-1,kind=0,published=0,cleaned=0,reports=0;
MStatus api(const std::string& name,MStatus* status=nullptr) {
    calls.push_back(name);
    const bool failed=int(calls.size())-1==failAt;
    if(failed&&kind==2)throw std::bad_alloc();
    if(failed&&kind==3)throw std::runtime_error("Bullet host query");
    const MStatus result=failed?MS::kInvalidParameter:MS::kSuccess;
    if(status)*status=result;return result;
}
}
struct MObject {int id;MObject(int value=0):id(value){}};
using MTypeId=int;
struct MPlug {
    int attr=0,index=-1;
    MPlug()=default;MPlug(MObject,MObject value):attr(value.id){}
    explicit MPlug(int value,int element=-1):attr(value),index(element){}
    bool operator==(const MObject& value)const{return attr==value.id;}
    bool isElement(MStatus* status=nullptr)const{fixture::api("isElement",status);return index>=0;}
    MPlug array(MStatus* status=nullptr)const{fixture::api("array",status);return MPlug(attr);}
    unsigned logicalIndex(MStatus* status=nullptr)const{fixture::api("logicalIndex",status);return unsigned(index);}
    bool isConnected(MStatus* status=nullptr)const{
        fixture::api("connection."+std::to_string(attr),status);return fixture::connections[attr];}
    MStatus getValue(MObject&)const{return fixture::api("value."+std::to_string(attr));}
};
struct MDataHandle {
    void set(bool){++fixture::published;}void setClean(){++fixture::cleaned;}
};
struct MDataBlock {
    MDataHandle inputValue(MObject,MStatus* status){fixture::api("input",status);return {};}
    MDataHandle outputValue(MObject,MStatus* status){fixture::api("output",status);return {};}
    MStatus setClean(const MPlug&){return fixture::api("clean");}
};
namespace MFn {enum Type{kInvalid,kDependencyNode};}
struct MDagPath {int id=0;bool hasFn(MFn::Type)const{return true;}};
struct MItDag {
    enum{kDepthFirst};int index=0;
    MItDag(int,MFn::Type,MStatus* status){fixture::api("iterator",status);}
    bool isDone(MStatus* status=nullptr)const{fixture::api("done",status);return index==7;}
    MStatus next(){++index;return fixture::api("next");}
    MStatus getPath(MDagPath& path)const{path.id=index+1;return fixture::api("path");}
};
struct MFnDependencyNode {
    int id;
    MFnDependencyNode(MObject object,MStatus* status=nullptr):id(object.id){fixture::api("dependency",status);}
    MTypeId typeId(MStatus* status=nullptr)const{fixture::api("type",status);return id;}
};
struct MFnDagNode:MFnDependencyNode {
    MFnDagNode(MDagPath path,MStatus* status):MFnDependencyNode(MObject(path.id),status){}
    bool isIntermediateObject(MStatus* status=nullptr)const{fixture::api("intermediate",status);return false;}
    MObject object(MStatus* status)const{fixture::api("object",status);return MObject(id);}
};
struct MGlobal {static void displayWarning(const char*){}};
#endif
namespace dommy {namespace maya {
MStatus reportComputeException(const char*,const std::exception_ptr&)noexcept;
template<class Node>Node* checkedUserNode(const MFnDependencyNode&,MTypeId);
}}
struct DBRigidBodyNode {
    static MTypeId typeId;
    static MObject ia_collisionShape,ia_solver,ca_rigidBody,ca_rigidBodyParam,ca_solver,worldMatrix;
    MObject thisMObject()const;
    MStatus compute(const MPlug&,MDataBlock&);
    MStatus computeRigidBody(const MPlug&,MDataBlock&);
    MStatus computeRigidBodyParam(const MPlug&,MDataBlock&);
    MStatus computeWorldMatrix(const MPlug&,MDataBlock&);
    MStatus update();
};
struct DBRigidBodyArrayNode {static MTypeId typeId;static MObject ia_collisionShape;MStatus update();};
struct DBSoftBodyNode {static MTypeId typeId;static MObject inputMesh;MStatus update();};
struct DBNailConstraintNode {static MTypeId typeId;static MObject ia_rigidBodyA,ia_rigidBodyB;MStatus update();};
struct DBHingeConstraintNode {static MTypeId typeId;static MObject ia_rigidBodyA,ia_rigidBodyB;MStatus update();};
struct DBSliderConstraintNode {static MTypeId typeId;static MObject ia_rigidBodyA,ia_rigidBodyB;MStatus update();};
struct DBSixdofConstraintNode {static MTypeId typeId;static MObject ia_rigidBodyA,ia_rigidBodyB;MStatus update();};
struct DBSolverNode {static MStatus updateAllRigidBodies();};
#ifndef DOMMY_SDK_CHECK
MTypeId DBRigidBodyNode::typeId=1,DBRigidBodyArrayNode::typeId=2,DBSoftBodyNode::typeId=3,
    DBNailConstraintNode::typeId=4,DBHingeConstraintNode::typeId=5,DBSliderConstraintNode::typeId=6,DBSixdofConstraintNode::typeId=7;
MObject DBRigidBodyNode::ia_collisionShape(10),DBRigidBodyNode::ia_solver(11),
    DBRigidBodyNode::ca_rigidBody(12),DBRigidBodyNode::ca_rigidBodyParam(13),
    DBRigidBodyNode::ca_solver(14),DBRigidBodyNode::worldMatrix(15),
    DBRigidBodyArrayNode::ia_collisionShape(20),DBSoftBodyNode::inputMesh(30),
    DBNailConstraintNode::ia_rigidBodyA(40),DBNailConstraintNode::ia_rigidBodyB(41),
    DBHingeConstraintNode::ia_rigidBodyA(50),DBHingeConstraintNode::ia_rigidBodyB(51),
    DBSliderConstraintNode::ia_rigidBodyA(60),DBSliderConstraintNode::ia_rigidBodyB(61),
    DBSixdofConstraintNode::ia_rigidBodyA(70),DBSixdofConstraintNode::ia_rigidBodyB(71);
MObject DBRigidBodyNode::thisMObject()const{return MObject(1);}
MStatus DBRigidBodyNode::computeRigidBody(const MPlug&,MDataBlock&){fixture::updates.push_back(12);return MS::kSuccess;}
MStatus DBRigidBodyNode::computeRigidBodyParam(const MPlug&,MDataBlock&){fixture::updates.push_back(13);return MS::kSuccess;}
MStatus DBRigidBodyNode::computeWorldMatrix(const MPlug&,MDataBlock&){fixture::updates.push_back(15);return MS::kSuccess;}
#define FIXTURE_UPDATE(Node) MStatus Node::update(){fixture::updates.push_back(typeId);return MS::kSuccess;}
FIXTURE_UPDATE(DBRigidBodyNode)
FIXTURE_UPDATE(DBRigidBodyArrayNode)
FIXTURE_UPDATE(DBSoftBodyNode)
FIXTURE_UPDATE(DBNailConstraintNode)
FIXTURE_UPDATE(DBHingeConstraintNode)
FIXTURE_UPDATE(DBSliderConstraintNode)
FIXTURE_UPDATE(DBSixdofConstraintNode)
#undef FIXTURE_UPDATE
namespace dommy {namespace maya {
MStatus reportComputeException(const char*,const std::exception_ptr&)noexcept{++fixture::reports;return MS::kFailure;}
template<class Node>Node* checkedUserNode(const MFnDependencyNode&,MTypeId){static Node node;return &node;}
}}
#endif
