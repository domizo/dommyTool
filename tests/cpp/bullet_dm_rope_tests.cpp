// Production endpoint selection, point reads, transform helpers and complete
// lock updates, with host doubles and optionally real Debug Bullet bodies.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#include "maya/MStatus.h"
#include "mvl/vec.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "Collision/DShapeFrameMath.h"

using vec3f = mvl::vec<float, 3>;
using quatf = mvl::vec<float, 4>;

#ifdef DOMMY_NATIVE_BULLET
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include "Solver/DBulletDynamicsWorld.h"
#else
static int bodyCopies = 0;
struct btVector3 {
    float v[3];
    btVector3(float x=0,float y=0,float z=0):v{x,y,z} {}
    float operator[](int i) const {return v[i];}
    float x() const {return v[0];} float y() const {return v[1];} float z() const {return v[2];}
};
struct btQuaternion {
    float v[4];
    btQuaternion(float x=0,float y=0,float z=0,float w=1):v{x,y,z,w} {}
    float x() const {return v[0];} float y() const {return v[1];}
    float z() const {return v[2];} float w() const {return v[3];}
};
struct btTransform {
    btQuaternion rotation;btVector3 origin;
    btTransform(btQuaternion q={},btVector3 p={}):rotation(q),origin(p) {}
    btQuaternion getRotation() const {return rotation;}
    btVector3 getOrigin() const {return origin;}
};
struct btDefaultMotionState {
    btTransform transform;
    explicit btDefaultMotionState(btTransform t={}):transform(t) {}
    void setWorldTransform(const btTransform& t) {transform=t;}
    void getWorldTransform(btTransform& t) const {t=transform;}
};
struct btBoxShape {explicit btBoxShape(btVector3) {}};
struct btRigidBody {
    btDefaultMotionState* motion;btTransform transform;int references=1;
    btRigidBody(float,btDefaultMotionState* m,btBoxShape*,btVector3={}):motion(m),transform(m->transform) {}
    btRigidBody(const btRigidBody& rhs):motion(rhs.motion),transform(rhs.transform),references(rhs.references) {++bodyCopies;}
    btDefaultMotionState* getMotionState() {return motion;}
    btTransform getWorldTransform() const {return transform;}
    int getNumConstraintRefs() const {return references;}
};
struct btPoint2PointConstraint {
    btRigidBody& a;
    bool enabled=true;
    btPoint2PointConstraint(btRigidBody& first,btRigidBody&,btVector3,btVector3):a(first) {}
    btRigidBody& getRigidBodyA() {return a;}
    void setEnabled(bool value) {enabled=value;}
    bool isEnabled() const {return enabled;}
};
using btTypedConstraint = btPoint2PointConstraint;
struct btSoftRigidDynamicsWorld {};
struct btSoftBodyWorldInfo {};
#endif

static btSoftRigidDynamicsWorld* ResolveBulletWorld(btSoftRigidDynamicsWorld* value) {return value;}
struct TestName {const char* asChar() const {return "rope";}};
static void EchoWarning(const char*,const char*) {}

#define DM_BULLET_ROPE_DEBUG false
#define DOMMY_MAYA_DEBUG(...) ((void)0)
enum {DB_LOCK_POINT=0,DB_LOCK_DPOINT=1,aRootDP=2,aTipDP=3,aIsLockAnimation=4,inDC=5};
struct Curve {std::size_t points=2;std::size_t numberOfPoints()const{return points;}};
struct DCurveData {
    std::vector<Curve> curves{Curve()};
    bool nullCurve=false;
    std::size_t numberOfCurves()const{return curves.size();}
    const Curve* getCurvePtr(std::size_t index)const{return nullCurve?nullptr:&curves[index];}
};
static DCurveData curveInput;
static bool invalidCurveInput=false;
struct DPData {
    std::vector<btVector3> points;
    std::size_t numberOfPoints() const {return points.size();}
    const std::vector<btVector3>* getPositionPtrConst() const {return &points;}
};
using DPointList = std::vector<btVector3>;
static DPData rootInput,tipInput;
static bool rootConnected=true,tipConnected=true,lockAnimation=true;
static int failedRead=-1;
static int failedConnection=-1;
static bool failParameters=false,failFlagRead=false,invalidData=false;
struct MPlug {
    int attribute;
    MPlug(int,int value):attribute(value) {}
    bool asBool() const {return lockAnimation;}
    bool isConnected(MStatus* status=nullptr) const {
        if(status)*status=attribute==failedConnection?MS::kInvalidParameter:MS::kSuccess;
        return attribute==aRootDP?rootConnected:tipConnected;
    }
};
static MStatus readPlugValue(MPlug, bool& value) {value=lockAnimation;return failFlagRead?MS::kFailure:MS::kSuccess;}
struct MDataHandle {const DPData* data=nullptr;};
namespace dommy { namespace maya {
struct ScopedPlugDataHandle {
    MDataHandle value;
    MDataHandle& handle() {return value;}
    MStatus read(MPlug plug) {value.data=plug.attribute==aRootDP?&rootInput:&tipInput;return plug.attribute==failedRead?MS::kFailure:MS::kSuccess;}
};
} }
static const DPData* getInputDPfromHandle(MDataHandle& handle) {return invalidData?nullptr:handle.data;}
static const DCurveData* getInputDCfromHandle(MDataHandle&) {return invalidCurveInput?nullptr:&curveInput;}
struct btRopeParams {
    bool m_lock_root=true,m_lock_tip=true,m_is_root_spring=false;
    int m_behavior_type=0;
    int m_root_lock_type=DB_LOCK_DPOINT,m_tip_lock_type=DB_LOCK_DPOINT;
    btVector3 m_root_lock_pos{1,2,3},m_tip_lock_pos{4,5,6};
};
struct Constraint {
    btTypedConstraint* pointer;
    btTypedConstraint* constraint() {return pointer;}
};
#include "dm_rope_positions.inc"
#include "dm_rope_topology.inc"
struct DCbtRope {
    btRopeParams parameters;
    btRopeParams m_initialParameters;
    btSoftRigidDynamicsWorld* m_worldReference=nullptr;
    bool m_initialized=false;
    std::vector<std::size_t> m_curvePointCounts{2};
    int detachCalls=0;
    std::vector<int> rbList{1};
    std::vector<int> rootRopeShapes,tipRopeShapes;
    std::vector<Constraint*> rootConstLists,tipConstLists;
    int thisMObject() {return 1;}
    TestName name() {return {};}
    MStatus evalParms(btRopeParams& value) {value=parameters;return failParameters?MS::kFailure:MS::kSuccess;}
    MStatus updateRopes(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*);
    void detachFromWorld()noexcept {
        ++detachCalls;rbList.clear();rootRopeShapes.clear();tipRopeShapes.clear();
        rootConstLists.clear();tipConstLists.clear();m_curvePointCounts.clear();
        m_worldReference=nullptr;m_initialized=false;
    }
    MStatus updateLockPosition(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*,float);
    std::pair<std::vector<btVector3>,std::vector<btVector3>> initialPositions(std::size_t count) {
        std::pair<std::vector<btVector3>,std::vector<btVector3>> result;
        const auto read = [&]() -> MStatus {
        struct Curves {std::size_t count;std::size_t numberOfCurves() const {return count;}} curves{count};
        const Curves* dCurves=&curves;
        btRopeParams param=parameters;
#include "dm_rope_initial.inc"
        result = {rootPositions,tipPositions};
        return MS::kSuccess;
        };
        assert(read());
        return result;
    }
};
#include "dm_rope_transform.inc"
#include "dm_rope_update.inc"
#include "dm_rope_flags.inc"

static void expectVector(const btVector3& actual,const btVector3& expected) {
    for(int axis=0;axis<3;++axis) assert(std::abs(actual[axis]-expected[axis])<0.0001f);
}
static void checkInitialPositions() {
    for(int attribute:{aRootDP,aTipDP})for(bool connected:{false,true}){
        failedConnection=attribute;rootConnected=tipConnected=connected;
        std::vector<btVector3> positions{btVector3(1,2,3)};
        assert(getLockPosition(positions,MPlug(1,attribute))==MS::kInvalidParameter);
        assert(positions[0][0]==1&&positions[0][1]==2&&positions[0][2]==3);
    }
    failedConnection=-1;rootConnected=tipConnected=true;
    rootInput.points={{10,11,12},{13,14,15}};
    tipInput.points={{20,21,22},{23,24,25},{26,27,28}};
    for(bool rootPoints:{false,true})for(bool tipPoints:{false,true}) {
        DCbtRope node;
        node.parameters.m_root_lock_type=rootPoints?DB_LOCK_DPOINT:DB_LOCK_POINT;
        node.parameters.m_tip_lock_type=tipPoints?DB_LOCK_DPOINT:DB_LOCK_POINT;
        for(std::size_t count:{0U,1U,5U}) {
            const auto positions=node.initialPositions(count);
            assert(positions.first.size()==count && positions.second.size()==count);
            for(std::size_t i=0;i<count;++i) {
                expectVector(positions.first[i],rootPoints?rootInput.points[i%2]:node.parameters.m_root_lock_pos);
                expectVector(positions.second[i],tipPoints?tipInput.points[i%3]:node.parameters.m_tip_lock_pos);
            }
        }
    }
    DCbtRope node;
    for(bool connected:{false,true}) {
        rootConnected=tipConnected=connected;
        if(connected){rootInput.points.clear();tipInput.points.clear();}
        const auto positions=node.initialPositions(1);
        expectVector(positions.first[0],node.parameters.m_root_lock_pos);
        expectVector(positions.second[0],node.parameters.m_tip_lock_pos);
    }
}

struct World {
#ifdef DOMMY_NATIVE_BULLET
    btSoftBodyRigidBodyCollisionConfiguration config;
    btCollisionDispatcher dispatcher{&config};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    dommy::bullet::DBoundedSoftDynamicsWorld world{&dispatcher,&broadphase,&solver,&config};
#else
    btSoftRigidDynamicsWorld world;
#endif
    btSoftBodyWorldInfo info;
};

static void checkLockUpdates() {
    rootInput.points={{10,11,12}};tipInput.points={{20,21,22}};
    rootConnected=tipConnected=true;
    for(bool root:{false,true})for(bool points:{false,true}) {
        btBoxShape shape(btVector3(1,1,1));
        btDefaultMotionState motionA(btTransform(btQuaternion(0,0,0,1),btVector3(2,4,6))),motionB;
        btRigidBody bodyA(0,&motionA,&shape),bodyB(1,&motionB,&shape,btVector3(1,1,1));
        btPoint2PointConstraint constraint(bodyA,bodyB,btVector3(0,0,0),btVector3(0,0,0));
        World owner;
#ifdef DOMMY_NATIVE_BULLET
        owner.world.addRigidBody(&bodyA);owner.world.addRigidBody(&bodyB);
        owner.world.addConstraint(&constraint,true);
#endif
        assert(bodyA.getNumConstraintRefs()==1);
        Constraint wrapper{&constraint};DCbtRope node;
        node.m_worldReference=&owner.world;node.m_initialized=true;
        node.parameters.m_root_lock_type=node.parameters.m_tip_lock_type=points?DB_LOCK_DPOINT:DB_LOCK_POINT;
        (root?node.rootRopeShapes:node.tipRopeShapes).push_back(1);
        (root?node.rootConstLists:node.tipConstLists).push_back(&wrapper);
        const btVector3 start=bodyA.getWorldTransform().getOrigin();
        const btVector3 target=points?(root?rootInput.points[0]:tipInput.points[0]):
            (root?node.parameters.m_root_lock_pos:node.parameters.m_tip_lock_pos);
        for(bool enabled:{false,true})for(float fraction:{0.0f,0.25f,1.0f}) {
            lockAnimation=enabled;
            assert(node.updateLockPosition(&owner.world,&owner.info,fraction));
            btTransform actual;motionA.getWorldTransform(actual);
            btVector3 expected(enabled?start[0]+fraction*(target[0]-start[0]):start[0],
                               enabled?start[1]+fraction*(target[1]-start[1]):start[1],
                               enabled?start[2]+fraction*(target[2]-start[2]):start[2]);
            expectVector(actual.getOrigin(),expected);
            assert(&constraint.getRigidBodyA()==&bodyA && bodyA.getNumConstraintRefs()==1);
#ifndef DOMMY_NATIVE_BULLET
            assert(bodyCopies==0);
#endif
        }
#ifdef DOMMY_NATIVE_BULLET
        owner.world.removeConstraint(&constraint);
        owner.world.removeRigidBody(&bodyA);owner.world.removeRigidBody(&bodyB);
#endif
    }
    DCbtRope empty;assert(!empty.updateLockPosition(nullptr,nullptr,1));
}

static void checkLockFlagsAndFailures() {
    World owner;
    btBoxShape shape(btVector3(1,1,1));
    btDefaultMotionState rootMotion,tipMotion;
    btRigidBody rootBody(0,&rootMotion,&shape),tipBody(0,&tipMotion,&shape);
    btPoint2PointConstraint root(rootBody,tipBody,btVector3(0,0,0),btVector3(0,0,0));
    btPoint2PointConstraint tip(tipBody,rootBody,btVector3(0,0,0),btVector3(0,0,0));
    Constraint rootWrapper{&root},tipWrapper{&tip};
    DCbtRope node;node.m_initialized=true;node.m_worldReference=&owner.world;
    node.rootConstLists={&rootWrapper};node.tipConstLists={&tipWrapper};
    node.rootRopeShapes={1};node.tipRopeShapes={1};
    for(bool rootFlag:{false,true})for(bool tipFlag:{false,true}) {
        node.parameters.m_lock_root=rootFlag;node.parameters.m_lock_tip=tipFlag;
        assert(node.updateRopes(&owner.world,&owner.info));
        assert(root.isEnabled()==rootFlag && tip.isEnabled()==tipFlag);
    }
    for(int change=0;change<6;++change) {
        DCbtRope candidate=node;
        if(change==0)candidate.m_initialParameters.m_lock_root=false;
        if(change==1)candidate.m_initialParameters.m_lock_tip=false;
        if(change==2)++candidate.parameters.m_root_lock_type;
        if(change==3)++candidate.parameters.m_tip_lock_type;
        if(change==4)candidate.parameters.m_is_root_spring=true;
        if(change==5)++candidate.parameters.m_behavior_type;
        root.setEnabled(false);tip.setEnabled(false);
        assert(!candidate.updateRopes(&owner.world,&owner.info));
        assert(!root.isEnabled() && !tip.isEnabled());
        // Reinitialization captures the newly created configuration.
        candidate.m_initialParameters=candidate.parameters;
        assert(candidate.updateRopes(&owner.world,&owner.info));
        assert(root.isEnabled() && tip.isEnabled());
    }
    lockAnimation=true;rootInput.points={{3,4,5}};tipInput.points={{6,7,8}};
    for(int failure=0;failure<8;++failure) {
        failParameters=failure==0;failFlagRead=failure==1;
        failedRead=failure==2?aRootDP:failure==3?aTipDP:-1;invalidData=failure==4;
        if(failure==5)tipInput.points[0]=btVector3(std::numeric_limits<float>::infinity(),0,0);
        if(failure==6)tipInput.points={{6,7,8}};
        failedConnection=failure==6?aRootDP:failure==7?aTipDP:-1;
        btTransform before;rootMotion.getWorldTransform(before);
        assert(!node.updateLockPosition(&owner.world,&owner.info,1));
        btTransform after;rootMotion.getWorldTransform(after);
        expectVector(before.getOrigin(),after.getOrigin());
    }
    failParameters=failFlagRead=invalidData=false;failedRead=failedConnection=-1;tipInput.points={{6,7,8}};
    assert(node.updateLockPosition(&owner.world,&owner.info,1));
    for(float time:{-1.0f,2.0f,std::numeric_limits<float>::quiet_NaN()})
        assert(!node.updateLockPosition(&owner.world,&owner.info,time));
    assert(!node.updateRopes(nullptr,&owner.info));
    std::puts("DM root/tip flags, structural restart and atomic endpoint read failures passed");
}

static void checkCapsuleGeometry() {
    using dommy::bullet::ropeCapsuleHeight;
    float height=99;
    assert(ropeCapsuleHeight(vec3f(0,0,0),vec3f(0,2,0),0.5,height) && height==1);
    assert(ropeCapsuleHeight(vec3f(0,0,0),vec3f(0,1,0),0.5,height) && height==0);
    for(float length:{0.0f,0.1f,0.999f,-0.1f,std::numeric_limits<float>::infinity()}) {
        height=99;assert(!ropeCapsuleHeight(vec3f(0,0,0),vec3f(0,length,0),0.5,height));assert(height==99);
    }
    for(double radius:{-0.5,0.0,2.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
        assert(!ropeCapsuleHeight(vec3f(0,0,0),vec3f(0,2,0),radius,height));
    assert(!ropeCapsuleHeight(vec3f(std::numeric_limits<float>::max(),0,0),vec3f(-std::numeric_limits<float>::max(),0,0),0.5,height));
    struct Positions {mvl::vec<double,3> m_root_lock_pos{0,0,0},m_tip_lock_pos{0,0,0};} parms;
    const auto checkPositions = [&]() -> MStatus {
#include "dm_rope_position_guard.inc"
    };
    assert(checkPositions());
    for(bool root:{false,true})for(double value:{std::numeric_limits<double>::max(),
            std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        parms=Positions();(root?parms.m_root_lock_pos:parms.m_tip_lock_pos)[0]=value;
        assert(!checkPositions());
    }
    std::puts("DM capsule zero/short/nonfinite/overflow boundaries passed");
}

#ifdef DOMMY_NATIVE_BULLET
struct db_collision_shape_base {
    float m_volume;vec3f m_local_inertia,m_center;quatf m_rotation;
    std::unique_ptr<btCollisionShape> value;
    void set_shape(std::unique_ptr<btCollisionShape> shape) {value=std::move(shape);}
    btCollisionShape* shape() {return value.get();}
    virtual ~db_collision_shape_base()=default;
};
#include "dm_capsule.inc"
static void checkNativeCapsule() {
    for(float height:{0.0f,1.0f}) {
        db_collision_capsule capsule(0.5f,height);
        btVector3 minimum,maximum;
        capsule.shape()->getAabb(btTransform::getIdentity(),minimum,maximum);
        const btVector3 support=static_cast<btCapsuleShape*>(capsule.shape())->localGetSupportingVertex(btVector3(0,1,0));
        assert(support.y()<=maximum.y() && support.y()>=minimum.y());
    }
    for(float height:{-0.9f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected=false;
        try {db_collision_capsule capsule(0.5f,height);}catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    }
    std::puts("DM real Bullet capsule support stays inside bounds; invalid dimensions rejected");
}
static void checkNativeGravity() {
    World owner;auto* m_dynamicsWorld=&owner.world;
    btSoftBodyWorldInfo info;auto* m_worldInfo=&info;
    info.m_broadphase=&owner.broadphase;info.m_dispatcher=&owner.dispatcher;
    m_dynamicsWorld->setGravity(btVector3(0,0,0));
    info.m_gravity=m_dynamicsWorld->getGravity();
    const btVector3 point(0,10,0);const btScalar mass=1;
    std::unique_ptr<btSoftBody> body(new btSoftBody(&info,1,&point,&mass));
    body->m_cfg.collisions=0;
    m_dynamicsWorld->addSoftBody(body.get());
    for(const btVector3& gravity:{btVector3(0,-9.8f,0),btVector3(1,9.8f,2),btVector3(0,0,0)}) {
        const btVector3 previous=body->m_nodes[0].m_v;
#include "dm_gravity_update.inc"
        expectVector(m_dynamicsWorld->getGravity(),gravity);
        expectVector(m_worldInfo->m_gravity,gravity);
        assert(body->getWorldInfo()==m_worldInfo);
        assert(m_dynamicsWorld->stepChecked(1.0f/60,1.0f/60));
        for(int axis=0;axis<3;++axis)
            assert(std::abs(body->m_nodes[0].m_v[axis]-(previous[axis]+gravity[axis]/60))<0.001f);
    }
    m_dynamicsWorld->removeSoftBody(body.get());
    std::puts("DM real Bullet soft gravity change/reversal/zero passed");
}
#endif

static void checkTopologyUpdates() {
    World owner;DCbtRope node;
    node.m_worldReference=&owner.world;node.m_initialized=true;
    curveInput=DCurveData();
    for(int failure=0;failure<6;++failure) {
        if(failure==0)failedRead=inDC;
        if(failure==1)invalidCurveInput=true;
        if(failure==2)curveInput.nullCurve=true;
        if(failure==3)curveInput.curves[0].points=0;
        if(failure==4)curveInput.curves[0].points=3;
        if(failure==5)curveInput.curves[0].points=dommy::generation::kMaxGeneratedTransformCount+2;
        assert(!node.updateRopes(&owner.world,&owner.info));
        assert(node.detachCalls==0 && node.rbList.size()==1);
        failedRead=-1;invalidCurveInput=false;curveInput=DCurveData();
    }
    curveInput.curves.clear();assert(node.updateRopes(&owner.world,&owner.info));
    assert(node.rbList.empty() && node.m_curvePointCounts.empty() && node.m_initialized);
    assert(node.m_worldReference==&owner.world);
    assert(node.updateRopes(&owner.world,&owner.info));
    curveInput=DCurveData();assert(!node.updateRopes(&owner.world,&owner.info));
    std::puts("Rope topology: read/type/null/point-count/budget validation, empty cleanup and restart requirement passed");
}
int main() {
    checkInitialPositions();checkLockUpdates();checkLockFlagsAndFailures();checkCapsuleGeometry();
    checkTopologyUpdates();
#ifdef DOMMY_NATIVE_BULLET
    checkNativeGravity();checkNativeCapsule();
#endif
    std::puts("DM rope endpoints, cyclic points, defaults, interpolation and borrowed bodies passed");
}
