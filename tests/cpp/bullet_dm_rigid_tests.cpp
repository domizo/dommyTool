// Complete DM/2D rigid callbacks with host doubles and native Bullet shapes,
// bodies, broadphase proxies and injected registration failures.
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
#include "db_constraint_base.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "BulletCollision/CollisionShapes/btConvex2dShape.h"
#include "mvl/mathUtils.h"
#include "Collision/DShapeFrameMath.h"
#include "Solver/DPassiveTransformMath.h"
#include "Constraint/DConstraintFrame.h"
#include "common/utility/DStatusOperation.h"

#define DM_BULLET_RIGID_DEBUG 0
#define DC_BULLET_RIGID_DEBUG 0
#define DOMMY_MAYA_DEBUG(...) ((void)0)
#include "dm_rigid_parameters.inc"
#include "dm_body_mode.inc"
#include "dm_rigid_attributes.inc"
static int reads=0,failedRead=0;
static double inputMass=1;
static float inputMargin=0.04f;
static MVector inputScale(1,1,1);
static bool deform=false,inputFailure=false;
struct TestTransform {
    MVector position,scale{1,1,1};
    quatf rotation{1,0,0,0};
};
using MMatrix=TestTransform;
namespace dommy {namespace maya {
inline bool isFiniteMatrix(const MMatrix& value) {
    for(int i=0;i<3;++i)if(!std::isfinite(value.position[i]) || !std::isfinite(value.scale[i]))return false;
    for(int i=0;i<4;++i)if(!std::isfinite(value.rotation[i]))return false;
    return true;
}
}}
namespace MSpace {enum {kTransform};}
struct MTransformationMatrix {
    TestTransform value;
    MTransformationMatrix()=default;
    explicit MTransformationMatrix(const TestTransform& input):value(input) {}
    MStatus getRotationQuaternion(double& x,double& y,double& z,double& w)const {
        x=value.rotation[1];y=value.rotation[2];z=value.rotation[3];w=value.rotation[0];
        return ++reads==failedRead?MS::kFailure:MS::kSuccess;
    }
    MVector getTranslation(int,MStatus* status)const {
        *status=++reads==failedRead?MS::kFailure:MS::kSuccess;return value.position;
    }
    MStatus getScale(double* scale,int)const {
        for(int i=0;i<3;++i)scale[i]=value.scale[i];
        return ++reads==failedRead?MS::kFailure:MS::kSuccess;
    }
    MStatus setRotationQuaternion(double x,double y,double z,double w) {
        value.rotation=quatf(w,x,y,z);return ++reads==failedRead?MS::kFailure:MS::kSuccess;
    }
    MStatus setTranslation(MVector position,int) {
        value.position=position;return ++reads==failedRead?MS::kFailure:MS::kSuccess;
    }
    MMatrix asMatrix()const{return value;}
};
struct MPlug {int attribute;MPlug(int,int a):attribute(a) {}};
template<class T>MStatus readPlugValue(MPlug plug,T& value) {
    if(++reads==failedRead)return MS::kFailure;
    if(plug.attribute==aMass)value=static_cast<T>(inputMass);
    if(plug.attribute==aCollisionMargin)value=static_cast<T>(inputMargin);
    if(plug.attribute==aIsCollisionDeform)value=static_cast<T>(deform);
    if(plug.attribute==aCollisionType)value=static_cast<T>(0);
    return MS::kSuccess;
}
static MStatus readPlugValue(MPlug plug,MVector& value) {
    if(++reads==failedRead)return MS::kFailure;
    if(plug.attribute==aCollisionScale)value=inputScale;
    return MS::kSuccess;
}
using MUuid=int;
struct MString {const char* asChar() const {return "rigid";}};
struct MFnDependencyNode {int id;explicit MFnDependencyNode(int v):id(v) {} int uuid()const{return id;}MString name()const{return {};}};
struct MDataHandle {};
namespace dommy {namespace maya {
struct ScopedPlugDataHandle {MDataHandle value;MDataHandle& handle(){return value;}MStatus read(MPlug){return inputFailure?MS::kFailure:MS::kSuccess;}};
}}
struct db_collision_shape_base {
    static int live;
    std::size_t id=0;
    std::size_t itemIndex()const{return id;}
    void set_itemIndex(std::size_t value){id=value;}
    std::unique_ptr<btConvexShape> child;
    std::unique_ptr<btCollisionShape> native;
    db_collision_shape_base(bool twoD,float extent=1) {
        ++live;
        if(twoD) {
            auto* hull=new btConvexHullShape;
            for(float x:{-extent,extent})for(float y:{-extent,extent})hull->addPoint(btVector3(x,y,0),false);
            hull->recalcLocalAabb();child.reset(hull);native.reset(new btConvex2dShape(child.get()));
        } else native.reset(new btBoxShape(btVector3(extent,extent,extent)));
    }
    explicit db_collision_shape_base(btCollisionShape* shape):native(shape){++live;}
    ~db_collision_shape_base(){--live;}
    btCollisionShape* shape()const{return native.get();}
    btCollisionShape* getBulletCollisionShape()const{return shape();}
};
int db_collision_shape_base::live=0;
struct Primitive {float extent=3;};
using ConstDMItem=const Primitive*;using ConstDCItem=const Primitive*;
struct Geometry {
    Primitive primitive;
    std::vector<Primitive> extras;
    size_t numberOfMesh()const{return 1+extras.size();}size_t numberOfCurves()const{return numberOfMesh();}
    const Primitive* getMeshPtrConst(size_t i)const{return i?&extras.at(i-1):&primitive;}
    const Primitive* getCurvePtr(size_t i)const{return getMeshPtrConst(i);}
};
using DMeshData=Geometry;using DCurveData=Geometry;
static Geometry geometry;
static const Geometry* getInputDMfromHandle(MDataHandle&){return &geometry;}
static const Geometry* getInputDCfromHandle(MDataHandle&){return &geometry;}
static db_collision_shape_base* createCollisionShape(short,const ConstDMItem* p){return new db_collision_shape_base(false,(*p)->extent);}
static db_collision_shape_base* create2DCollisionShape(const ConstDCItem* p){return new db_collision_shape_base(true,(*p)->extent);}
struct customDBData {MUuid nodeUuid;std::string prefixName;int index=0;};
#include "dm_rigid_world_reference.inc"
struct DBulletRigidBodyDeleter {void operator()(btRigidBody*)const noexcept;};
using DBulletRigidBodyPtr=std::unique_ptr<btRigidBody,DBulletRigidBodyDeleter>;
void ReleaseBulletRigidBodies(std::vector<btRigidBody*>&);
#include "dm_rigid_ownership.inc"
#include "dm_rigid_shape_helpers.inc"
#include "dm_rigid_kinematic.inc"
#include "dm_rigid_output_helpers.inc"
#include "dm_rope_transform.inc"

struct CommonNode {
    DBulletWeakWorldReference m_worldReference;
    std::vector<db_collision_shape_base*> collisionShapes,compoundCollisionLists;
    std::vector<btTransform> startTransformList,compoundStartTransformList;
    std::vector<TestTransform> kinematicTransformList;
    std::vector<MVector> startScaleList;
    std::vector<btRigidBody*> rbList,compRbList;
    size_t preCollsionCount=0;
    void detachFromWorld()noexcept {
        auto world=m_worldReference.lock();
        DetachAndReleaseBulletRigidBodies(world,rbList);
        DetachAndReleaseBulletRigidBodies(world,compRbList);
        m_worldReference.reset();
    }
    ~CommonNode(){detachFromWorld();for(auto* p:collisionShapes)delete p;for(auto* p:compoundCollisionLists)delete p;}
    void addShape(bool twoD){collisionShapes.push_back(new db_collision_shape_base(twoD));collisionShapes.back()->set_itemIndex(collisionShapes.size()-1);startTransformList.push_back(btTransform::getIdentity());startScaleList.emplace_back(1,1,1);}
};
struct DMbtRigid : CommonNode {
    int thisMObject(){return 1;}
    MStatus evalParms(btRigidParams&);
    MStatus resetRigidBodies(btSoftRigidDynamicsWorld*,const DBulletWorldReferencePtr&);
    MStatus resetRigidBodiesKinematic(btSoftRigidDynamicsWorld*,const DBulletWorldReferencePtr&);
    MStatus updateRigidBodies(btSoftRigidDynamicsWorld*,DBulletBodyMode = DBulletBodyMode::Dynamic);
    MStatus updateCollisionShape(btSoftRigidDynamicsWorld*);
    MStatus updateRigidBodiesKinematic(btSoftRigidDynamicsWorld*,float);
};
struct DCbtRigid2D : CommonNode {
    int thisMObject(){return 2;}
    MStatus evalParms(btRigidParams&);
    MStatus resetRigidBodies(btDiscreteDynamicsWorld*,const DBulletWorldReferencePtr&);
    MStatus resetRigidBodiesKinematic(btDiscreteDynamicsWorld*,const DBulletWorldReferencePtr&);
    MStatus updateRigidBodies(btDiscreteDynamicsWorld*,DBulletBodyMode = DBulletBodyMode::Dynamic);
    MStatus updateCollisionShape(btDiscreteDynamicsWorld*);
    MStatus updateRigidBodiesKinematic(btDiscreteDynamicsWorld*,float);
};
#include "dm_rigid_callbacks.inc"

struct TestWorld : btSoftRigidDynamicsWorld {
    int failedRegistrations=0;
    using btSoftRigidDynamicsWorld::btSoftRigidDynamicsWorld;
    void addRigidBody(btRigidBody* body,int group,int mask)override {
        btSoftRigidDynamicsWorld::addRigidBody(body,group,mask);
        if(failedRegistrations>0){--failedRegistrations;throw std::bad_alloc();}
    }
    void addRigidBody(btRigidBody* body)override {
        btSoftRigidDynamicsWorld::addRigidBody(body);
        if(failedRegistrations>0){--failedRegistrations;throw std::bad_alloc();}
    }
};
struct World {
    btDefaultCollisionConfiguration config;btCollisionDispatcher dispatcher{&config};
    btDbvtBroadphase broadphase;btSequentialImpulseConstraintSolver solver;
    TestWorld world{&dispatcher,&broadphase,&solver,&config};
    DBulletWorldReferencePtr reference{new DBulletWorldReference(&world)};
};
static void checkInertia(btRigidBody& body) {
    const float mass=body.getInvMass()==0?0:1/body.getInvMass();
    btVector3 expected(0,0,0);if(mass>0)body.getCollisionShape()->calculateLocalInertia(mass,expected);
    for(int axis=0;axis<3;++axis)assert(std::abs(body.getInvInertiaDiagLocal()[axis]-(expected[axis]==0?0:1/expected[axis]))<1e-5);
    const btMatrix3x3 basis=body.getWorldTransform().getBasis();
    const btMatrix3x3 tensor=basis.scaled(body.getInvInertiaDiagLocal())*basis.transpose();
    for(int row=0;row<3;++row)for(int column=0;column<3;++column)
        assert(std::abs(body.getInvInertiaTensorWorld()[row][column]-tensor[row][column])<1e-5);
}
template<class Node>static void checkCallbacks(bool twoD) {
    World owner;Node node;node.addShape(twoD);
    reads=failedRead=0;inputMass=1;inputScale=MVector(2,3,1);deform=false;
    btRigidParams params;assert(node.evalParms(params));const int count=reads;
    assert(node.resetRigidBodies(&owner.world,owner.reference));assert(owner.world.getNumCollisionObjects()==1);
    auto* previous=node.rbList[0];checkInertia(*previous);
    for(int axis=0;axis<3;++axis) {
        assert(previous->getCollisionShape()->getLocalScaling()[axis]==inputScale[axis]);
    }
    for(int failure=1;failure<=count;++failure) {
        reads=0;failedRead=failure;assert(!node.resetRigidBodies(&owner.world,owner.reference));
        assert(node.rbList[0]==previous && owner.world.getNumCollisionObjects()==1);
    }
    reads=failedRead=0;
    for(double scale:{0.0,1e-100,std::numeric_limits<double>::infinity()}) {
        inputScale=MVector(scale,1,1);
        assert(!node.resetRigidBodies(&owner.world,owner.reference));
        assert(node.rbList[0]==previous && previous->getCollisionShape()->getLocalScaling().x()==2);
    }
    inputScale=MVector(2,3,1);
    for(double mass:{-1.0,1e-100,std::numeric_limits<double>::infinity()}) {
        inputMass=mass;assert(!node.resetRigidBodies(&owner.world,owner.reference));assert(node.rbList[0]==previous);
    }
    const btVector3 gravity=owner.world.getGravity();
    inputMass=1e20;owner.world.setGravity(btVector3(0,-1e20,0));
    assert(!node.resetRigidBodies(&owner.world,owner.reference));assert(node.rbList[0]==previous);
    owner.world.setGravity(gravity);
    inputMass=1;node.startScaleList.clear();assert(!node.resetRigidBodies(&owner.world,owner.reference));
    node.startScaleList.emplace_back(1,1,1);assert(node.rbList[0]==previous);
    node.addShape(twoD);
    inputMass=1e20;owner.world.setGravity(btVector3(0,-1e20,0));
    assert(!node.updateRigidBodies(&owner.world));assert(node.rbList.size()==1);
    owner.world.setGravity(gravity);inputMass=1;
    assert(node.updateRigidBodies(&owner.world));assert(node.rbList.size()==2);checkInertia(*node.rbList[1]);
    // Input cardinality matches the prepared bodies (shrink requires StartTime).
    geometry.extras.resize(1);
    deform=true;inputScale=MVector(1,1,1);inputFailure=true;
    auto* oldShape=node.collisionShapes[0];assert(!node.updateCollisionShape(&owner.world));assert(node.collisionShapes[0]==oldShape);
    inputFailure=false;owner.world.failedRegistrations=1;
    assert(!node.updateCollisionShape(&owner.world));assert(node.collisionShapes[0]==oldShape && previous->getCollisionShape()==oldShape->shape());
    assert(previous->isInWorld() && owner.world.getNumCollisionObjects()==2);checkInertia(*previous);
    previous->setLinearVelocity(btVector3(1,2,3));previous->setAngularVelocity(btVector3(0,0,2));
    assert(node.updateCollisionShape(&owner.world));assert(node.collisionShapes[0]!=oldShape);checkInertia(*previous);
    assert(previous->getLinearVelocity()==btVector3(1,2,3) && previous->getAngularVelocity()==btVector3(0,0,2));
    btVector3 minimum,maximum;previous->getCollisionShape()->getAabb(previous->getWorldTransform(),minimum,maximum);
    assert(previous->getBroadphaseHandle()->m_aabbMin.x()<=minimum.x());
    assert(previous->getBroadphaseHandle()->m_aabbMax.x()>=maximum.x());
    for(float mass:{0.0f,2.0f}) {
        inputMass=mass;assert(node.updateCollisionShape(&owner.world));checkInertia(*previous);
        assert(previous->isStaticObject()==(mass==0));
        assert(previous->getBroadphaseHandle()->m_collisionFilterGroup==(mass==0?btBroadphaseProxy::StaticFilter:btBroadphaseProxy::DefaultFilter));
        if(mass>0)assert(previous->getGravity()==owner.world.getGravity() && previous->isActive());
        else previous->setGravity(btVector3(0,0,0));
    }
    oldShape=node.collisionShapes[0];owner.world.failedRegistrations=2;
    assert(!node.updateCollisionShape(&owner.world));
    assert(node.collisionShapes[0]==oldShape && previous->getCollisionShape()==oldShape->shape());
    assert(!previous->isInWorld());
    inputMass=1;assert(node.resetRigidBodiesKinematic(&owner.world,owner.reference));
    assert(node.rbList[0]->isKinematicObject());assert(node.updateCollisionShape(&owner.world));
    assert(node.rbList[0]->isKinematicObject() && node.rbList[0]->getInvMass()==0);checkInertia(*node.rbList[0]);
    inputMass=-1;assert(!node.updateCollisionShape(&owner.world));inputMass=1;
    geometry.extras.clear();
    std::printf("%s rigid callbacks: %d read failures, reset preservation, scaled/new/deformed inertia, rollback/recovery, proxy filters and kinematic mass passed\n",twoD?"2D":"3D",count);
}
static void checkCompound() {
    World owner;db_collision_shape_base child(false);DCbtRigid2D node;
    auto* compound=new btCompoundShape;compound->addChildShape(btTransform::getIdentity(),child.shape());
    node.compoundCollisionLists.push_back(new db_collision_shape_base(compound));node.compoundStartTransformList.push_back(btTransform::getIdentity());
    inputMass=1;inputScale=MVector(2,3,1);inputMargin=0.1f;
    assert(node.resetRigidBodies(&owner.world,owner.reference));checkInertia(*node.compRbList[0]);
    assert(child.shape()->getMargin()==inputMargin);
    node.detachFromWorld();
}
template<class Node>static void checkKinematic(bool twoD) {
    World owner;Node node;node.addShape(twoD);
    reads=failedRead=0;inputMass=1;inputScale=MVector(1,1,1);
    assert(node.resetRigidBodiesKinematic(&owner.world,owner.reference));
    node.kinematicTransformList.resize(1);
    btRigidBody* body=node.rbList[0];
    auto& target=node.kinematicTransformList[0];
    target.rotation=quatf(-1,0,0,0);
    assert(node.updateRigidBodiesKinematic(&owner.world,.5f));
    btTransform result;body->getMotionState()->getWorldTransform(result);
    assert(dommy::bullet::finiteQuaternion(quatf(result.getRotation().w(),result.getRotation().x(),
                                                result.getRotation().y(),result.getRotation().z())));
    assert(std::abs(std::abs(result.getRotation().w())-1)<1e-5f);
    const float angle=170.0f*SIMD_PI/180.0f;
    body->setWorldTransform(btTransform(btQuaternion(btVector3(0,0,1),angle),btVector3(0,0,0)));
    target.rotation=quatf(std::cos(-angle/2),0,0,std::sin(-angle/2));
    assert(node.updateRigidBodiesKinematic(&owner.world,.5f));
    body->getMotionState()->getWorldTransform(result);
    assert((result.getBasis()*btVector3(1,0,0)-btVector3(-1,0,0)).length2()<1e-8f);
    const btTransform previous=result;
    target.position=MVector(10,0,0);target.scale=MVector(0,1,1);
    assert(!node.updateRigidBodiesKinematic(&owner.world,.5f));
    body->getMotionState()->getWorldTransform(result);assert(result==previous);
    target.scale=MVector(1,1,1);
    for(int failure=1;failure<=4;++failure) {
        reads=0;failedRead=failure;assert(!node.updateRigidBodiesKinematic(&owner.world,.5f));
        body->getMotionState()->getWorldTransform(result);assert(result==previous);
    }
    failedRead=0;
    for(float fraction:{-1.f,2.f,std::numeric_limits<float>::quiet_NaN()})
        assert(!node.updateRigidBodiesKinematic(&owner.world,fraction));
    std::puts("Full kinematic callback: antipodal/shortest rotation, invalid scale/read/fraction preservation passed");
}
template<class Node>static void checkKinematicAppend(bool planar) {
    World owner;Node node;node.addShape(planar);
    reads=failedRead=0;inputMass=2;inputScale=MVector(1,1,1);
    assert(node.resetRigidBodiesKinematic(&owner.world,owner.reference));
    node.addShape(planar);
    assert(node.updateRigidBodies(&owner.world,DBulletBodyMode::Kinematic));
    assert(node.rbList.size()==2);
    for(auto* body:node.rbList) {
        assert(body->isKinematicObject() && body->getInvMass()==0);
        assert(body->getActivationState()==DISABLE_DEACTIVATION);
        assert(body->getLinearVelocity().length2()==0 && body->getAngularVelocity().length2()==0);
        assert(body->getBroadphaseHandle()->m_collisionFilterGroup==btBroadphaseProxy::StaticFilter);
        checkInertia(*body);
        if(planar)assert(body->getLinearFactor()==btVector3(1,1,0) && body->getAngularFactor()==btVector3(0,0,1));
    }
    node.kinematicTransformList.resize(2);
    node.kinematicTransformList[1].position=MVector(12,3,0);
    assert(node.updateRigidBodiesKinematic(&owner.world,1));
    btTransform result;node.rbList[1]->getMotionState()->getWorldTransform(result);
    assert(result.getOrigin()==btVector3(12,3,0));
    std::printf("%s kinematic additions retain zero mass, flags, activation, filters and animation\n",planar?"2D":"3D");
}
static void checkCompoundKinematicRejection() {
    World owner;db_collision_shape_base child(false);DCbtRigid2D node;
    auto* compound=new btCompoundShape;compound->addChildShape(btTransform::getIdentity(),child.shape());
    node.compoundCollisionLists.push_back(new db_collision_shape_base(compound));
    node.compoundStartTransformList.push_back(btTransform::getIdentity());
    inputMass=1;inputScale=MVector(1,1,1);
    assert(node.resetRigidBodies(&owner.world,owner.reference));auto* previous=node.compRbList[0];
    assert(!node.resetRigidBodiesKinematic(&owner.world,owner.reference));
    assert(node.compRbList[0]==previous && owner.world.getNumCollisionObjects()==1);
    std::puts("Compound kinematic reset fails before detaching the previous native body");
}
namespace OutputTests {
struct Primitive {
    MMatrix matrix;
    MMatrix getTransformMatrix()const{return matrix;}
    void setTransformMatrix(const MMatrix& value){matrix=value;}
};
struct Geometry {
    std::vector<Primitive> items;
    std::size_t numberOfCurves()const{return items.size();}
    std::size_t numberOfMesh()const{return items.size();}
    Primitive* getCurvePtr(std::size_t i){return &items.at(i);}
    Primitive* getMeshPtr(std::size_t i){return getCurvePtr(i);}
};
using DCurveDataPtrList=std::vector<Geometry*>;
using DMeshDataPtrList=DCurveDataPtrList;
using UuidList=std::vector<MUuid>;
struct DCBullet2DSolver {
    TestWorld* m_dynamicsWorld;
    MStatus updateRigidBodies(DCurveDataPtrList*,UuidList*);
};
struct DMBulletTest {
    TestWorld* m_dynamicsWorld;
    MStatus updateRigidBodies(DMeshDataPtrList*,UuidList*);
};
#include "dm_rigid_output_callbacks.inc"
}

static void checkGroupedOutputAndDeformation() {
    World owner;db_collision_shape_base first(true),second(true);
    customDBData firstData,secondData;
    firstData.nodeUuid=secondData.nodeUuid=2;firstData.index=0;secondData.index=1;
    first.shape()->setUserPointer(&firstData);second.shape()->setUserPointer(&secondData);
    DCbtRigid2D node;node.addShape(true);node.collisionShapes[0]->set_itemIndex(2);
    auto* compound=new btCompoundShape;
    compound->addChildShape(btTransform::getIdentity(),first.shape());
    compound->addChildShape(btTransform(btQuaternion(0,0,0,1),btVector3(10,0,0)),second.shape());
    node.compoundCollisionLists.push_back(new db_collision_shape_base(compound));
    node.compoundStartTransformList.push_back(btTransform::getIdentity());
    reads=failedRead=0;inputMass=1;inputScale=MVector(1,1,1);
    assert(node.resetRigidBodies(&owner.world,owner.reference));
    assert(static_cast<customDBData*>(node.rbList[0]->getUserPointer())->index==2);
    node.rbList[0]->getMotionState()->setWorldTransform(
        btTransform(btQuaternion(0,0,0,1),btVector3(25,0,0)));
    OutputTests::Geometry output;output.items.resize(3);
    output.items[1].matrix.scale=MVector(-2,3,1);
    OutputTests::DCurveDataPtrList outputs{&output};OutputTests::UuidList ids{2};
    OutputTests::DCBullet2DSolver solver{&owner.world};
    assert(solver.updateRigidBodies(&outputs,&ids));
    assert(output.items[0].matrix.position.x==0 && output.items[1].matrix.position.x==10);
    assert(output.items[2].matrix.position.x==25 && output.items[1].matrix.scale.x==-2);
    geometry.extras.resize(2);geometry.extras[1].extent=9;deform=true;
    for(int repeat=0;repeat<2;++repeat) {
        assert(node.updateCollisionShape(&owner.world));
        assert(node.collisionShapes[0]->itemIndex()==2);
        auto* flat=static_cast<btConvex2dShape*>(node.rbList[0]->getCollisionShape());
        auto* hull=static_cast<btConvexHullShape*>(flat->getChildShape());
        assert(hull->getUnscaledPoints()[0].x()==-9);
    }
    // Incremental singles also carry original IDs, not their compact slot.
    node.addShape(true);node.collisionShapes[1]->set_itemIndex(3);
    assert(node.updateRigidBodies(&owner.world));
    assert(static_cast<customDBData*>(node.rbList[1]->getUserPointer())->index==3);
    geometry.extras.clear();deform=false;
    std::puts("Grouped 2D originals [0,1]+[2], append and repeated deformation mapping passed");
}

template<class Solver,class Rigid>static void checkGeometryOutput(bool planar) {
    World owner;Rigid node;node.addShape(planar);
    reads=failedRead=0;inputMass=1;inputScale=MVector(4,5,1);deform=false;
    node.startScaleList[0]=MVector(2,3,1);
    assert(node.resetRigidBodies(&owner.world,owner.reference));
    OutputTests::Geometry output;output.items.resize(1);
    output.items[0].matrix.scale=MVector(-2,3,.5);
    OutputTests::DCurveDataPtrList outputs{&output};OutputTests::UuidList ids{node.thisMObject()};
    Solver solver{&owner.world};
    const btQuaternion rotation(btVector3(0,0,1),.7f);
    node.rbList[0]->getMotionState()->setWorldTransform(btTransform(rotation,btVector3(7,8,9)));
    assert(solver.updateRigidBodies(&outputs,&ids));
    const auto& matrix=output.items[0].matrix;
    assert(matrix.scale.x==-2 && matrix.scale.y==3 && matrix.scale.z==.5);
    assert(matrix.position.x==7 && matrix.position.y==8 && matrix.position.z==9);
    assert(std::abs(matrix.rotation[3]-rotation.z())<1e-6);
    assert(node.rbList[0]->getCollisionShape()->getLocalScaling()==btVector3(8,15,1));
    for(int failure=1;failure<=2;++failure) {
        output.items[0].matrix.position=MVector(1,2,3);reads=0;failedRead=failure;
        assert(!solver.updateRigidBodies(&outputs,&ids));
        assert(output.items[0].matrix.position.x==1);
    }
    failedRead=0;
    auto* metadata=static_cast<customDBData*>(node.rbList[0]->getUserPointer());
    metadata->index=42;assert(!solver.updateRigidBodies(&outputs,&ids));metadata->index=0;
    auto* data=outputs[0];outputs[0]=nullptr;assert(!solver.updateRigidBodies(&outputs,&ids));outputs[0]=data;
    output.items[0].matrix.scale.x=std::numeric_limits<double>::infinity();
    assert(!solver.updateRigidBodies(&outputs,&ids));output.items[0].matrix.scale.x=1;
    btTransform invalid=btTransform::getIdentity();invalid.setOrigin(btVector3(NAN,0,0));
    node.rbList[0]->getMotionState()->setWorldTransform(invalid);
    assert(!solver.updateRigidBodies(&outputs,&ids));
    std::printf("%s output signed/nonuniform scale, collision-only scaling and failure propagation passed\n",planar?"2D":"3D");
}

static void checkSingleCompoundOutput() {
    World owner;db_collision_shape_base child(false);DMbtRigid node;node.addShape(false);
    auto* compound=new btCompoundShape;compound->addChildShape(btTransform::getIdentity(),child.shape());
    delete node.collisionShapes[0];node.collisionShapes[0]=new db_collision_shape_base(compound);
    reads=failedRead=0;inputScale=MVector(1,1,1);inputMass=1;
    assert(node.resetRigidBodies(&owner.world,owner.reference));
    node.rbList[0]->getMotionState()->setWorldTransform(btTransform(btQuaternion(0,0,0,1),btVector3(12,0,0)));
    OutputTests::Geometry output;output.items.resize(1);
    OutputTests::DMeshDataPtrList outputs{&output};OutputTests::UuidList ids{1};
    OutputTests::DMBulletTest solver{&owner.world};assert(solver.updateRigidBodies(&outputs,&ids));
    assert(output.items[0].matrix.position.x==12);
    std::puts("Single-input compound output uses root metadata, not absent child metadata");
}

int main(){
    checkCallbacks<DMbtRigid>(false);checkCallbacks<DCbtRigid2D>(true);checkCompound();
    checkKinematic<DMbtRigid>(false);checkKinematic<DCbtRigid2D>(true);
    checkKinematicAppend<DMbtRigid>(false);checkKinematicAppend<DCbtRigid2D>(true);
    checkCompoundKinematicRejection();
    checkGroupedOutputAndDeformation();
    checkGeometryOutput<OutputTests::DCBullet2DSolver,DCbtRigid2D>(true);
    checkGeometryOutput<OutputTests::DMBulletTest,DMbtRigid>(false);
    checkSingleCompoundOutput();
    assert(db_collision_shape_base::live==0);
}
