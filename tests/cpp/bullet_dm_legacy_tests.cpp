// Full production legacy callbacks/input helpers/registration helpers with
// failure-injectable Maya doubles and real Bullet 2.88 bodies and constraints.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "BulletDynamics/ConstraintSolver/btFixedConstraint.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "mvl/mathUtils.h"
#include "db_constraint_base.h"
#include "Constraint/DConstraintFrame.h"
#include "Collision/DShapeFrameMath.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "dm_body_mode.inc"
#include "dm_legacy_vector_validation.inc"
#include "common/utility/DVectorLookup.h"

#define DOMMY_MAYA_DEBUG(...) ((void)0)
#include "dm_legacy_attributes.inc"
#include "dm_rigid_parameters.inc"
#include "dm_legacy_parameters.inc"
using MObject=int;
using MTypeId=int;
using MUuid=int;
using DPointList=std::vector<vec3f>;
using DPIndexList=std::vector<size_t>;
struct MString {const char* asChar()const{return "legacy";}};
struct MIntArray:std::vector<int> {unsigned length()const{return size();}};
struct Node {virtual ~Node()=default;virtual int type()const=0;};
static std::map<int,Node*> nodes;
using Key=std::pair<int,int>;
static std::map<Key,std::vector<int>> links;
static std::map<Key,double> scalars;
static std::map<Key,MVector> vectors;
static int hostCalls=0,hostFail=0;
static bool badData=false;
static MStatus hostStatus(){return ++hostCalls==hostFail?MS::kFailure:MS::kSuccess;}
struct MPlug;
struct MPlugArray:std::vector<MPlug>{unsigned length()const{return size();}};
struct MPlug {
    int object,attribute,index=-1,axis=-1;
    MPlug(int o,int a):object(o),attribute(a){}
    unsigned numConnectedElements(MStatus* status)const {
        *status=hostStatus();return links[{object,attribute}].size();
    }
    MPlug connectionByPhysicalIndex(unsigned i,MStatus* status)const {
        *status=hostStatus();MPlug result=*this;result.index=i;return result;
    }
    bool connectedTo(MPlugArray& output,bool incoming,bool outgoing,MStatus* status)const {
        assert(incoming && !outgoing);*status=hostStatus();
        const auto& connected=links[{object,attribute}];
        if(index<0)for(int id:connected)output.emplace_back(id,0);
        else output.emplace_back(connected.at(index),0);
        return !output.empty();
    }
    bool isConnected(MStatus* status)const {
        *status=hostStatus();return !links[{object,attribute}].empty();
    }
    MObject node(MStatus* status)const {*status=hostStatus();return object;}
    unsigned numChildren()const{return 3;}
    MPlug child(unsigned i)const{MPlug result=*this;result.axis=i;return result;}
    double asDouble(MStatus* status)const {
        *status=hostStatus();
        return axis<0?scalars[{object,attribute}]:vectors[{object,attribute}][axis];
    }
    bool asBool(MStatus* status)const{return asDouble(status)!=0;}
    short asShort(MStatus* status)const{return static_cast<short>(asDouble(status));}
};
struct MFnDependencyNode {
    int object;
    explicit MFnDependencyNode(int o,MStatus* status=nullptr):object(o){if(status)*status=hostStatus();}
    int typeId(MStatus* status)const {*status=hostStatus();return nodes.at(object)->type();}
    MUuid uuid(MStatus* status)const {*status=hostStatus();return object;}
    MString name(MStatus* status)const {*status=hostStatus();return {};}
};
template<class T>MStatus readPlugValue(MPlug plug,T& value) {
    const MStatus status=hostStatus();if(!status)return status;
    const auto found=scalars.find({plug.object,plug.attribute});
    if(found!=scalars.end())value=static_cast<T>(found->second);
    return MS::kSuccess;
}
static MStatus readPlugValue(MPlug plug,MVector& value) {
    const MStatus status=hostStatus();if(!status)return status;
    const auto found=vectors.find({plug.object,plug.attribute});
    if(found!=vectors.end())value=found->second;
    return MS::kSuccess;
}
struct MDataHandle {int attribute=0;};
namespace dommy {namespace maya {
struct ScopedPlugDataHandle {
    MDataHandle value;
    MStatus read(MPlug input){value.attribute=input.attribute;return hostStatus();}
    MDataHandle handle()const{return value;}
};
template<class T>T* checkedUserNode(const MFnDependencyNode& node,int) {
    return dynamic_cast<T*>(nodes.at(node.object));
}
}}
struct DPData {
    DPointList points{vec3f(1,2,0)};
    size_t numberOfPoints()const{return points.size();}
    DPointList::const_iterator beginPt()const{return points.begin();}
    DPointList::const_iterator endPt()const{return points.end();}
};
struct Curve {
    DPointList points{vec3f(0,0,0),vec3f(2,0,0)};
    MIntArray ids;
    Curve(){ids.push_back(0);ids.push_back(1);}
    bool isExistAttr(const char*)const{return true;}
    const DPointList* getPositionPtrConst()const{return &points;}
    std::size_t numberOfPoints()const{return points.size();}
};
struct DCurveData {
    Curve curve;
    std::vector<const Curve*> curves{&curve};
    std::vector<const Curve*>::const_iterator beginPrims()const{return curves.begin();}
    std::vector<const Curve*>::const_iterator endPrims()const{return curves.end();}
    std::size_t numberOfCurves()const{return curves.size();}
    const Curve* getCurvePtr(std::size_t index)const{return curves[index];}
};
static DPData pointData;
static DCurveData curveData;
static const DPData* getInputDPfromHandle(MDataHandle){return badData?nullptr:&pointData;}
static const DCurveData* getInputDCfromHandle(MDataHandle){return badData?nullptr:&curveData;}
static void getDCPAttributeMIntArrayFromScalar(const Curve* curve,const char*,MIntArray& ids){ids=curve->ids;}
static bool isContainDData(const DPIndexList& ids,size_t id) {
    return std::find(ids.begin(),ids.end(),id)!=ids.end();
}
struct db_collision_shape_base {
    static int live;
    size_t id=0;
    std::unique_ptr<btCollisionShape> native;
    explicit db_collision_shape_base(btCollisionShape* value):native(value){++live;}
    virtual ~db_collision_shape_base(){--live;}
    btCollisionShape* shape()const{return native.get();}
    size_t itemIndex()const{return id;}
    void set_itemIndex(size_t value){id=value;}
};
int db_collision_shape_base::live=0;
struct db_collision_box:db_collision_shape_base {
    explicit db_collision_box(const vec3f& size):db_collision_shape_base(new btBoxShape(btVector3(size[0],size[1],size[2]))){}
};
struct customDBData {MUuid nodeUuid;std::string prefixName;size_t index=0;};
#include "dm_rigid_world_reference.inc"
struct DBulletRigidBodyDeleter {void operator()(btRigidBody*)const noexcept;};
using DBulletRigidBodyPtr=std::unique_ptr<btRigidBody,DBulletRigidBodyDeleter>;
using DBulletConstraintPtr=std::unique_ptr<db_constraint_base>;
void ReleaseBulletRigidBodies(std::vector<btRigidBody*>&);
#include "dm_rigid_ownership.inc"
#include "dm_rigid_shape_helpers.inc"
#include "dm_rigid_kinematic.inc"
#include "dm_rope_transform.inc"
#include "dm_legacy_ownership.inc"
struct DMbtRigid:Node {
    enum {typeId=100};
    std::vector<btRigidBody*> bodies;
    int type()const override{return typeId;}
    std::vector<btRigidBody*>* getRigidBodyData(){return &bodies;}
};
#define SETTINGS_NODE(Name, Id) struct Name:Node { \
    enum {typeId=Id};int type()const override{return typeId;} \
    int thisMObject()const{return typeId;}MStatus evalParms(DbtConstraintParams&);};
SETTINGS_NODE(BTP2PConstraint,101)
SETTINGS_NODE(BTFixConstraint,102)
SETTINGS_NODE(BTConeTwistConstraint,103)
SETTINGS_NODE(BT6DofConstraint,104)
#undef SETTINGS_NODE
#include "dm_legacy_settings.inc"
#include "dm_legacy_inputs.inc"
struct DBPointConstraint:Node {
    enum {typeId=105};int type()const override{return typeId;}
    int thisMObject()const{return typeId;}
    DBulletWeakWorldReference m_worldReference;
    std::vector<db_constraint_base*> constraintLists;
    std::vector<btRigidBody*> rbLists;
    std::vector<db_collision_shape_base*> colShapes;
    MStatus evalParms(DBPointConstParams&);
    MStatus createConstraint(btSoftRigidDynamicsWorld*,const DBulletWorldReferencePtr&);
    MStatus updateConstraint(btSoftRigidDynamicsWorld*,float);
    void detachFromWorld()noexcept;
    ~DBPointConstraint(){detachFromWorld();}
};
struct DBLinkConstraint:Node {
    enum {typeId=106};int type()const override{return typeId;}
    int thisMObject()const{return typeId;}
    DBulletWeakWorldReference m_worldReference;
    std::vector<db_constraint_base*> constraintLists;
    MStatus evalParms(DBLinkConstParams&);
    MStatus createConstraint(btSoftRigidDynamicsWorld*,const DBulletWorldReferencePtr&);
    MStatus updateConstraint(btSoftRigidDynamicsWorld*,float);
    void detachFromWorld()noexcept;
    ~DBLinkConstraint(){detachFromWorld();}
};
struct DMbtFracture:Node {
    enum {typeId=107};int type()const override{return typeId;}
    int thisMObject()const{return typeId;}
    DBulletWeakWorldReference m_worldReference;
    std::vector<btRigidBody*> rbList,clsRbList;
    std::vector<db_collision_shape_base*> collisionShapes,clusteredCollisionLists;
    std::vector<btTransform> startTransformList,clusteredStartTransformList;
    std::vector<DPIndexList> clusteredIdLists;
    std::vector<db_constraint_base*> connectConstLists;
    size_t preCollsionCount=0;
    MStatus evalParms(btRigidParams&);
    MStatus resetRigidBodies(btSoftRigidDynamicsWorld*,const DBulletWorldReferencePtr&);
    MStatus updateRigidBodies(btSoftRigidDynamicsWorld*);
    void detachFromWorld()noexcept;
    ~DMbtFracture(){detachFromWorld();ReleaseBulletCollisionShapes(collisionShapes);ReleaseBulletCollisionShapes(clusteredCollisionLists);}
};
#include "dm_legacy_callbacks.inc"
static void EchoWarning(const char*,const char*){}
struct btRopeParams {
    bool m_lock_root=true,m_lock_tip=true,m_is_root_spring=false;
    int m_root_lock_type=0,m_tip_lock_type=0,m_behavior_type=0;
};
struct DCbtRope {
    enum {inDC=1000};
    DBulletWeakWorldReference m_worldReference;
    bool m_initialized=false;
    btRopeParams m_initialParameters;
    std::vector<std::size_t> m_curvePointCounts;
    std::vector<btRigidBody*> rbList,rootRbList,tipRbList;
    std::vector<db_collision_shape_base*> ropeShapes,rootRopeShapes,tipRopeShapes;
    std::vector<db_constraint_base*> rootConstLists,tipConstLists,midConstLists;
    int thisMObject()const{return 1000;}
    MString name()const{return {};}
    MStatus evalParms(btRopeParams& params){params=m_initialParameters;return hostStatus();}
    MStatus updateRopes(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*);
    void detachFromWorld()noexcept;
    ~DCbtRope(){detachFromWorld();}
};
#include "dm_rope_topology.inc"
#include "dm_rope_lifecycle.inc"
struct TestWorld:btSoftRigidDynamicsWorld {
    int bodyFailure=0,constraintFailure=0;
    using btSoftRigidDynamicsWorld::btSoftRigidDynamicsWorld;
    void addRigidBody(btRigidBody* body)override {
        if(bodyFailure==1)throw std::bad_alloc();
        btSoftRigidDynamicsWorld::addRigidBody(body);
        if(bodyFailure==2)throw std::bad_alloc();
    }
    void addConstraint(btTypedConstraint* constraint,bool disabled=false)override {
        if(constraintFailure==1)throw std::bad_alloc();
        btSoftRigidDynamicsWorld::addConstraint(constraint,disabled);
        if(constraintFailure==2)throw std::bad_alloc();
    }
};
struct World {
    btDefaultCollisionConfiguration config;btCollisionDispatcher dispatcher{&config};
    btDbvtBroadphase broadphase;btSequentialImpulseConstraintSolver solver;
    TestWorld world{&dispatcher,&broadphase,&solver,&config};
    DBulletWorldReferencePtr reference{new DBulletWorldReference(&world)};
};
static bool near(const btVector3& a,const btVector3& b){return (a-b).length2()<1e-8f;}
static void checkPointFrame(db_constraint_base* owner,const btVector3& expected) {
    auto* constraint=static_cast<btPoint2PointConstraint*>(owner->constraint());
    assert(near(constraint->getRigidBodyA().getWorldTransform()*constraint->getPivotInA(),expected));
    assert(near(constraint->getRigidBodyB().getWorldTransform()*constraint->getPivotInB(),expected));
}
static void checkConeFrame(db_constraint_base* owner,const btVector3& expected) {
    auto* constraint=static_cast<btConeTwistConstraint*>(owner->constraint());
    const btTransform first=constraint->getRigidBodyA().getWorldTransform()*constraint->getAFrame();
    const btTransform second=constraint->getRigidBodyB().getWorldTransform()*constraint->getBFrame();
    assert(near(first.getOrigin(),expected) && near(second.getOrigin(),expected));
    for(int row=0;row<3;++row)assert(near(first.getBasis()[row],second.getBasis()[row]));
    assert(near(first.getBasis()*btVector3(1,0,0),btVector3(0,1,0)));
}
static void defaults(int id) {
    scalars[{id,aIsEnable}]=1;scalars[{id,aIsUpdate}]=0;
    scalars[{id,aDebugDisplaySize}]=1;
    links[{id,inRB}]={DMbtRigid::typeId};
    links[{id,inConst}]={BTP2PConstraint::typeId};
    links[{id,inConnectInfoDC}]={1};
}
template<class Settings>static void checkSettings(Settings& settings) {
    DbtConstraintParams params;
    hostCalls=0;assert(settings.evalParms(params));const int count=hostCalls;
    for(int failure=1;failure<=count;++failure) {
        hostCalls=0;hostFail=failure;assert(!settings.evalParms(params));
    }
    hostFail=0;
    const MPlug plug(999,inConst);links[{999,inConst}]={settings.type()};
    assert(getConstraintInfo(plug,params));
    scalars[{settings.type(),aDebugDisplaySize}]=std::numeric_limits<double>::infinity();
    assert(!getConstraintInfo(plug,params));
    scalars.erase({settings.type(),aDebugDisplaySize});
    std::printf("Legacy settings type %d: %d failed reads and invalid numeric settings rejected\n",settings.type(),count);
}
static void checkRegistration() {
    World owner;
    for(int failure:{1,2}) {
        owner.world.bodyFailure=failure;
        std::vector<btRigidBody*> bodies;std::vector<db_collision_shape_base*> shapes;
        std::unique_ptr<db_collision_shape_base> shape(new db_collision_box(vec3f(1,1,1)));
        auto body=MakeSimpleKinematicRigidBody(shape->shape(),btVector3(0,0,0));
        bool caught=false;
        try{AddBulletRigidBodyWithShape(owner.world,bodies,shapes,std::move(body),std::move(shape));}
        catch(const std::bad_alloc&){caught=true;}
        assert(caught && bodies.empty() && shapes.empty() && owner.world.getNumCollisionObjects()==0);
        assert(db_collision_shape_base::live==0);
    }
    owner.world.bodyFailure=0;
    btBoxShape shape(btVector3(1,1,1));btRigidBody first(0,nullptr,&shape),second(0,nullptr,&shape);
    for(int failure:{1,2}) {
        owner.world.constraintFailure=failure;std::vector<db_constraint_base*> constraints;
        DBulletConstraintPtr constraint(new db_p2p_constraint(first,second,btVector3(0,0,0),btVector3(0,0,0)));
        bool caught=false;
        try{AddBulletConstraint(owner.reference,constraints,std::move(constraint),true);}
        catch(const std::bad_alloc&){caught=true;}
        assert(caught && constraints.empty() && owner.world.getNumConstraints()==0);
        assert(first.getNumConstraintRefs()==0 && second.getNumConstraintRefs()==0);
    }
    owner.reference->invalidate(); // Failed insertion must not leave stale registry owners.
    std::puts("Registration helpers: before/after native insertion exceptions leave no registered dangling pointers");
}
static void checkConstraintLifetime() {
    for(bool disableCollisions:{false,true}) for(int order:{0,1,2}) {
        World owner;
        btBoxShape shape(btVector3(1,1,1));
        std::vector<btRigidBody*> bodies;
        std::vector<db_constraint_base*> constraints;
        for(int i=0;i<3;++i)
            AddBulletRigidBody(owner.world,bodies,
                MakeSimpleKinematicRigidBody(&shape,btVector3(i,0,0)));
        for(int i=1;i<3;++i)
            AddBulletConstraint(owner.reference,constraints,
                DBulletConstraintPtr(new db_p2p_constraint(*bodies[0],*bodies[i],
                    btVector3(0,0,0),btVector3(0,0,0))),disableCollisions);
        if(order==1) DetachAndReleaseBulletConstraints(owner.reference,constraints);
        if(order==2) owner.reference->invalidate();
        if(order==0) {
            std::vector<btRigidBody*> first{bodies.front()};
            bodies.erase(bodies.begin());
            DetachAndReleaseBulletRigidBodies(owner.reference,first);
            for(auto* constraint:constraints) assert(!constraint->constraint());
        }
        assert(owner.world.getNumConstraints()==0);
        for(auto* body:bodies) assert(body->getNumConstraintRefs()==0);
        // World invalidation follows native removal during solver teardown.
        if(order==2) for(auto* body:bodies) owner.world.removeRigidBody(body);
        DetachAndReleaseBulletRigidBodies(owner.reference,bodies);
        DetachAndReleaseBulletConstraints(owner.reference,constraints);
        DetachAndReleaseBulletConstraints(owner.reference,constraints);
        assert(owner.world.getNumCollisionObjects()==0);
    }
    std::puts("Constraint lifetimes: body/constraint/world-first teardown, both collision modes and repeated detach passed");
}
static void checkSixDofValues() {
    btBoxShape shape(btVector3(1,1,1));
    btRigidBody first(0,nullptr,&shape),second(0,nullptr,&shape);
    DbtConstraintParams params;
    params.m_linear_lower_limit=MVector(-1,-2,-3);
    params.m_linear_upper_limit=MVector(1,2,3);
    params.m_angular_lower_limit=MVector(-.1,-.2,-.3);
    params.m_angular_upper_limit=MVector(.1,.2,.3);
    params.m_use_spring=true;
    params.m_translate_stiffness=MVector(100,200,300);
    params.m_rotate_stiffness=MVector(10,20,30);
    db_sixdof_constraint normal(first,second,btTransform::getIdentity(),btTransform::getIdentity(),false);
    db_sixdof_spring_constraint spring(first,second,btTransform::getIdentity(),btTransform::getIdentity(),false);
    normal.set_param(params);spring.set_param(params);
    for(auto* constraint:{static_cast<btGeneric6DofConstraint*>(normal.constraint()),
                         static_cast<btGeneric6DofConstraint*>(spring.constraint())}) {
        btVector3 value(0,0,0);
        constraint->getLinearLowerLimit(value);assert(near(value,btVector3(-1,-2,-3)));
        constraint->getLinearUpperLimit(value);assert(near(value,btVector3(1,2,3)));
        constraint->getAngularLowerLimit(value);assert(near(value,btVector3(-.1f,-.2f,-.3f)));
        constraint->getAngularUpperLimit(value);assert(near(value,btVector3(.1f,.2f,.3f)));
    }
    auto* native=static_cast<btGeneric6DofSpringConstraint*>(spring.constraint());
    for(int axis=0;axis<3;++axis) {
        assert(native->getStiffness(axis)==params.m_translate_stiffness[axis]);
        assert(native->getStiffness(axis+3)==params.m_rotate_stiffness[axis]);
    }
    std::puts("6DoF normal/spring: independent linear/angular limits and unconverted stiffness passed");
}
static void checkLegacyConstraints() {
    World owner;db_collision_box shape(vec3f(1,1,1));
    btRigidBody first(0,nullptr,shape.shape()),second(0,nullptr,shape.shape());
    first.setWorldTransform(btTransform(btQuaternion(btVector3(0,0,1),SIMD_HALF_PI),btVector3(0,0,0)));
    second.setWorldTransform(btTransform(btQuaternion(btVector3(0,1,0),.7f),btVector3(2,0,0)));
    owner.world.addRigidBody(&first);owner.world.addRigidBody(&second);
    DMbtRigid rigid;rigid.bodies={&first,&second};nodes[rigid.type()]=&rigid;
    DBPointConstraint point;DBLinkConstraint link;
    defaults(point.type());defaults(link.type());
    vectors[{point.type(),aPosition}]=MVector(1,2,0);
    hostCalls=0;assert(point.createConstraint(&owner.world,owner.reference));const int count=hostCalls;
    assert(point.constraintLists.size()==2 && point.rbLists.size()==1);
    checkPointFrame(point.constraintLists[0],btVector3(1,2,0));
    auto* previous=point.constraintLists[0];
    for(int failure=1;failure<=count;++failure) {
        hostCalls=0;hostFail=failure;
        assert(!point.createConstraint(&owner.world,owner.reference));
        assert(point.constraintLists[0]==previous && owner.world.getNumConstraints()==2 &&
               owner.world.getNumCollisionObjects()==3);
    }
    hostFail=0;
    scalars[{point.type(),aTargetType}]=DB_CONST_TAR_DP;links[{point.type(),aTargetDP}]={1};
    badData=true;assert(!point.createConstraint(&owner.world,owner.reference));assert(point.constraintLists[0]==previous);
    badData=false;
    scalars[{point.type(),aIsUpdate}]=1;scalars[{point.type(),aIsEnable}]=1;
    btTransform before;point.rbLists[0]->getMotionState()->getWorldTransform(before);
    hostCalls=0;assert(point.updateConstraint(&owner.world,.5f));const int updateCount=hostCalls;
    point.rbLists[0]->getMotionState()->getWorldTransform(before);
    for(int failure=1;failure<=updateCount;++failure) {
        hostCalls=0;hostFail=failure;
        assert(!point.updateConstraint(&owner.world,.5f));
        assert(point.constraintLists[0]->constraint()->isEnabled());
        btTransform after;point.rbLists[0]->getMotionState()->getWorldTransform(after);
        assert(after==before);
    }
    hostFail=0;
    pointData.points.clear();assert(point.createConstraint(&owner.world,owner.reference));
    assert(point.rbLists.size()==1);checkPointFrame(point.constraintLists[0],btVector3(1,2,0));
    pointData.points={vec3f(1,2,0)};
    // A foreign-world source is rejected before the old point constraints go away.
    previous=point.constraintLists[0];owner.world.removeRigidBody(&second);
    assert(!point.createConstraint(&owner.world,owner.reference));assert(point.constraintLists[0]==previous);
    owner.world.addRigidBody(&second);
    links[{link.type(),inConst}]={BTConeTwistConstraint::typeId};
    hostCalls=0;assert(link.createConstraint(&owner.world,owner.reference));const int linkCount=hostCalls;
    assert(link.constraintLists.size()==1);checkConeFrame(link.constraintLists[0],btVector3(1,0,0));
    previous=link.constraintLists[0];
    for(int failure=1;failure<=linkCount;++failure) {
        hostCalls=0;hostFail=failure;assert(!link.createConstraint(&owner.world,owner.reference));
        assert(link.constraintLists[0]==previous);
    }
    hostFail=0;badData=true;scalars[{link.type(),aIsUpdate}]=1;
    assert(!link.updateConstraint(&owner.world,.5f));assert(link.constraintLists[0]==previous);
    badData=false;
    // Cross-group links use the same world frame as links within one source group.
    DMbtRigid target;target.bodies={&second};nodes[200]=&target;
    rigid.bodies={&first};links[{link.type(),aTargetRB}]={200};
    curveData.curve.ids[1]=0;assert(link.createConstraint(&owner.world,owner.reference));
    assert(link.constraintLists.size()==1);checkConeFrame(link.constraintLists[0],btVector3(1,0,0));
    scalars[{point.type(),aTargetType}]=DB_CONST_TAR_RB;links[{point.type(),aTargetRB}]={200};
    assert(point.createConstraint(&owner.world,owner.reference));
    assert(point.rbLists.empty());checkPointFrame(point.constraintLists[0],second.getWorldTransform().getOrigin());
    rigid.bodies={&second};assert(link.createConstraint(&owner.world,owner.reference));
    assert(link.constraintLists.empty()); // No self-constraint.
    curveData.curve.ids[1]=1;rigid.bodies={&first,&second};links[{link.type(),aTargetRB}].clear();
    link.detachFromWorld();point.detachFromWorld();
    owner.world.removeRigidBody(&first);owner.world.removeRigidBody(&second);nodes.erase(rigid.type());nodes.erase(200);
    std::printf("Point/Link complete callbacks: %d/%d preflight failures, rotated frames, input types/empty/world validation passed\n",count,linkCount);
}
static void checkFracture() {
    World owner;DMbtFracture fracture;defaults(fracture.type());
    for(size_t i=0;i<2;++i) {
        auto* shape=new db_collision_box(vec3f(1,2,3));shape->id=i;
        fracture.collisionShapes.push_back(shape);
        fracture.startTransformList.emplace_back(btQuaternion(btVector3(0,0,1),.3f+i),btVector3(i*2,0,0));
    }
    hostCalls=0;assert(fracture.resetRigidBodies(&owner.world,owner.reference));const int count=hostCalls;
    assert(fracture.rbList.size()==2 && fracture.connectConstLists.size()==1);
    checkConeFrame(fracture.connectConstLists[0],btVector3(1,0,0));
    auto* previous=fracture.rbList[0];
    for(int failure=1;failure<=count;++failure) {
        hostCalls=0;hostFail=failure;assert(!fracture.resetRigidBodies(&owner.world,owner.reference));
        assert(fracture.rbList[0]==previous && owner.world.getNumCollisionObjects()==2 && owner.world.getNumConstraints()==1);
    }
    hostFail=0;badData=true;assert(!fracture.resetRigidBodies(&owner.world,owner.reference));
    assert(fracture.rbList[0]==previous);badData=false;
    for(double value:{-1.0,1e-100,std::numeric_limits<double>::infinity()}) {
        scalars[{fracture.type(),aMass}]=value;assert(!fracture.resetRigidBodies(&owner.world,owner.reference));
        assert(fracture.rbList[0]==previous);
    }
    scalars[{fracture.type(),aMass}]=2;scalars[{fracture.type(),aCollisionMargin}]=.2;
    assert(fracture.resetRigidBodies(&owner.world,owner.reference));
    btVector3 inertia;fracture.rbList[0]->getCollisionShape()->calculateLocalInertia(2,inertia);
    for(int axis=0;axis<3;++axis)assert(std::abs(1/fracture.rbList[0]->getInvInertiaDiagLocal()[axis]-inertia[axis])<1e-5);
    hostCalls=0;assert(fracture.updateRigidBodies(&owner.world));const int updateCount=hostCalls;
    for(int failure=1;failure<=updateCount;++failure) {
        hostCalls=0;hostFail=failure;assert(!fracture.updateRigidBodies(&owner.world));
        assert(fracture.connectConstLists[0]->constraint()->isEnabled());
    }
    hostFail=0;
    std::printf("Fracture full reset/update: %d/%d input failures, rotated frames, invalid mass and margin-before-inertia passed\n",count,updateCount);
}
static void checkClusterFracture() {
    World owner;db_collision_box child(vec3f(1,2,3));DMbtFracture fracture;defaults(fracture.type());
    auto* compound=new btCompoundShape;
    compound->addChildShape(btTransform::getIdentity(),child.shape());
    fracture.clusteredCollisionLists.push_back(new db_collision_shape_base(compound));
    fracture.clusteredIdLists.push_back({0,1});
    fracture.clusteredStartTransformList.emplace_back(btQuaternion(btVector3(0,0,1),.9f),btVector3(0,0,0));
    auto* single=new db_collision_box(vec3f(1,1,1));single->id=5;
    fracture.collisionShapes.push_back(single);
    fracture.startTransformList.emplace_back(btQuaternion(btVector3(0,1,0),.4f),btVector3(2,0,0));
    curveData.curve.ids[1]=5;
    assert(fracture.resetRigidBodies(&owner.world,owner.reference));
    assert(fracture.clsRbList.size()==1 && fracture.rbList.size()==1 && fracture.connectConstLists.size()==1);
    checkConeFrame(fracture.connectConstLists[0],btVector3(1,0,0));
    assert(std::abs(child.shape()->getMargin()-.2f)<1e-6f);
    auto* previous=fracture.clsRbList[0];fracture.clusteredIdLists.clear();
    assert(!fracture.resetRigidBodies(&owner.world,owner.reference));assert(fracture.clsRbList[0]==previous);
    fracture.clusteredIdLists.push_back({0,1});curveData.curve.ids[1]=1;
    assert(fracture.resetRigidBodies(&owner.world,owner.reference));
    assert(fracture.connectConstLists.empty()); // Both pieces belong to the same body.
    std::puts("Fracture clusters: rotated cluster/single frames, child margin, same-cluster skip and mismatch preservation passed");
}
static void checkRopeLifecycle() {
    World owner;DCbtRope rope;btSoftBodyWorldInfo info;
    rope.m_worldReference=owner.reference;rope.m_initialized=true;rope.m_curvePointCounts={2};
    curveData.curve.points={vec3f(0,0,0),vec3f(2,0,0)};curveData.curves={&curveData.curve};
    for(int i=0;i<3;++i) {
        std::unique_ptr<db_collision_shape_base> shape(new db_collision_box(vec3f(1,1,1)));
        auto body=MakeSimpleKinematicRigidBody(shape->shape(),btVector3(i*2,0,0));
        auto& bodies=i==0?rope.rbList:i==1?rope.rootRbList:rope.tipRbList;
        auto& shapes=i==0?rope.ropeShapes:i==1?rope.rootRopeShapes:rope.tipRopeShapes;
        AddBulletRigidBodyWithShape(owner.world,bodies,shapes,std::move(body),std::move(shape));
    }
    for(auto* list:{&rope.rootConstLists,&rope.tipConstLists,&rope.midConstLists})
        AddBulletConstraint(owner.reference,*list,DBulletConstraintPtr(new db_p2p_constraint(
            *rope.rbList[0],*rope.rootRbList[0],btVector3(0,0,0),btVector3(0,0,0))),true);
    hostCalls=0;assert(rope.updateRopes(&owner.world,&info));
    for(int change=0;change<5;++change) {
        if(change==0){hostCalls=0;hostFail=1;}
        if(change==1)badData=true;
        if(change==2)curveData.curves[0]=nullptr;
        if(change==3)curveData.curve.points.push_back(vec3f(4,0,0));
        if(change==4)curveData.curves.push_back(&curveData.curve);
        assert(!rope.updateRopes(&owner.world,&info));
        assert(owner.world.getNumCollisionObjects()==3 && owner.world.getNumConstraints()==3);
        hostFail=0;badData=false;curveData.curves={&curveData.curve};curveData.curve.points.resize(2);
    }
    curveData.curves.clear();assert(rope.updateRopes(&owner.world,&info));
    assert(owner.world.getNumCollisionObjects()==0 && owner.world.getNumConstraints()==0);
    assert(rope.rbList.empty() && rope.rootRbList.empty() && rope.tipRbList.empty());
    assert(db_collision_shape_base::live==0 && rope.m_curvePointCounts.empty());
    assert(ResolveBulletWorld(rope.m_worldReference)==&owner.world && rope.m_initialized);
    assert(rope.updateRopes(&owner.world,&info));
    curveData.curves={&curveData.curve};assert(!rope.updateRopes(&owner.world,&info));
    // Empty-to-nonempty rope input requires StartTime, not implicit rebuilding.
    rope.detachFromWorld();rope.detachFromWorld();
    owner.reference->invalidate();
    std::puts("Rope native lifecycle: invalid reads/topology preserve bodies; empty detaches all constraints/bodies/shapes; repeated detach safe");
}
int main() {
    BTP2PConstraint p2p;BTFixConstraint fix;BTConeTwistConstraint cone;BT6DofConstraint dof;
    nodes={{p2p.type(),&p2p},{fix.type(),&fix},{cone.type(),&cone},{dof.type(),&dof}};
    checkSettings(p2p);checkSettings(fix);checkSettings(cone);checkSettings(dof);
    scalars[{dof.type(),aUseSpring}]=1;
    for(bool uniform:{false,true}) {
        scalars[{dof.type(),aUseUniformStiffness}]=uniform;
        scalars[{dof.type(),aUseUniformDamping}]=uniform;
        checkSettings(dof);
    }
    checkRegistration();checkConstraintLifetime();checkSixDofValues();
    checkLegacyConstraints();checkFracture();checkClusterFracture();
    checkRopeLifecycle();
    assert(db_collision_shape_base::live==0);
}
