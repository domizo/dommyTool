// Complete production callbacks with host/body doubles; no Maya runtime.
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include "modules/dynamics/DBullet/Collision/collision_shape_impl.h"
#include "modules/dynamics/DBullet/Collision/DShapeFrameMath.h"
#include "modules/dynamics/DBullet/Collision/DCollisionMeshInput.h"
#include "modules/dynamics/DBullet/RigidBody/DBRigidBodyParameters.h"
#include "modules/dynamics/DBullet/SoftBody/DSoftBodyPreparation.h"

namespace MS { enum Code { kSuccess, kFailure, kInvalidParameter }; }
struct MStatus {
    MS::Code code;
    MStatus(MS::Code c=MS::kSuccess):code(c) {}
    operator bool() const { return code==MS::kSuccess; }
};
using float3=float[3];
struct MObject {};
struct MEulerRotation { double x=0,y=0,z=0; };
struct MQuaternion {
    double x=0,y=0,z=0,w=1;
    MQuaternion()=default;
    MQuaternion(double a,double b,double c,double d):x(a),y(b),z(c),w(d) {}
    MEulerRotation asEulerRotation() const { MEulerRotation result; result.z=z*3.141592654; return result; }
};
namespace MSpace { enum Space { kTransform }; }
namespace fixture {
float3 scale={1,1,1}; double mass=1;
bool reject=false, failOutput=false, failReconfigure=false, failCreate=false, failAdd=false, start=false, failShapeQuery=false;
int clean=0, published=0, removed=0, fallbackShapes=0;
int checks=0, failAt=0, exceptionChecks=0, throwAt=0;
float3 initialPosition={100,200,300},initialRotation={11,22,33};
MVector parentPosition;
MQuaternion parentRotation;
void mayThrow() { if(++exceptionChecks==throwAt) throw std::runtime_error("injected"); }
MStatus check() { mayThrow(); return ++checks==failAt?MS::kFailure:MS::kSuccess; }
void reset() {
    scale[0]=scale[1]=scale[2]=1; mass=1;
    reject=failOutput=failReconfigure=failCreate=failAdd=start=failShapeQuery=false;
    clean=published=removed=fallbackShapes=checks=failAt=exceptionChecks=throwAt=0;
    for(int i=0;i<3;++i) { initialPosition[i]=float((i+1)*100); initialRotation[i]=float((i+1)*11); }
    parentPosition=MVector(); parentRotation=MQuaternion();
}
}
namespace dommy { namespace maya {
bool isFloatRepresentableVector(const MVector& v) {
    return numeric::canRepresentAsFloat(v.x) && numeric::canRepresentAsFloat(v.y) && numeric::canRepresentAsFloat(v.z);
}
} }
bool isFiniteQuaternion(const MQuaternion& q) {
    return dommy::numeric::canRepresentAsFloat(q.x) && dommy::numeric::canRepresentAsFloat(q.y) &&
        dommy::numeric::canRepresentAsFloat(q.z) && dommy::numeric::canRepresentAsFloat(q.w);
}
bool isFiniteScale(const double s[3]) { return std::isfinite(s[0]) && std::isfinite(s[1]) && std::isfinite(s[2]); }
struct MPlug {
    MPlug()=default; MPlug(MObject,int) {} MStatus getValue(MObject&) const { return fixture::check(); }
    MStatus getValue(double&) const { return MS::kFailure; }
    MStatus getValue(int&) const { return MS::kFailure; }
    bool isConnected() const { return false; }
    template<class T> bool connectedTo(T&,bool destination,bool source,MStatus* s) const {
        assert(destination && !source);
        *s=fixture::failShapeQuery?MS::kFailure:MS::kSuccess; return false;
    }
    MObject node(MStatus* s) const { *s=MS::kSuccess; return MObject(); }
};
struct MPlugArray { unsigned int length() const { return 0; } MPlug operator[](int) const { return MPlug(); } };
struct MFnDependencyNode {
    MFnDependencyNode(MObject,MStatus* s) { *s=MS::kSuccess; }
    int typeId() const { return 1; }
};
namespace dommy { namespace maya {
template<class T> T* checkedUserNode(const MFnDependencyNode&,int,MStatus* s) { *s=MS::kFailure; return nullptr; }
} }
void EchoWarning(const char*,const char*) {}
struct MDataHandle {
    explicit MDataHandle(int a=0):attribute(a) {}
    int attribute;
    double asDouble() const { return attribute==2 ? fixture::mass : 0; }
    float asFloat() const { return 0; }
    int asInt() const { return 100; }
    float3& asFloat3() const { return attribute==7?fixture::initialPosition:attribute==8?fixture::initialRotation:fixture::scale; }
    void set3Float(float x,float y,float z) {
        float3& values=asFloat3(); values[0]=x; values[1]=y; values[2]=z; fixture::mayThrow();
    }
    void setBool(bool) { ++fixture::published; }
    void set(bool value) { setBool(value); }
    void setClean() { ++fixture::clean; }
};
struct MDataBlock {
    MDataHandle inputValue(int a,MStatus* s) { *s=fixture::check(); return MDataHandle(a); }
    MDataHandle outputValue(int a,MStatus* s) { *s=fixture::failOutput?MS::kFailure:fixture::check(); return MDataHandle(a); }
    MStatus setClean(const MPlug&) { const auto s=fixture::check(); if(s) ++fixture::clean; return s; }
};
struct MFnDagNode {
    MFnDagNode(MObject,MStatus* s) { *s=fixture::check(); }
    MObject parent(int,MStatus* s) { *s=fixture::check(); return MObject(); }
};
struct MFnTransform {
    MFnTransform(MObject,MStatus* s) { *s=fixture::check(); }
    MStatus getScale(double s[3]) { for(int i=0;i<3;++i)s[i]=fixture::scale[i]; return fixture::check(); }
    MVector getTranslation(MSpace::Space,MStatus* s) { *s=fixture::check(); return fixture::parentPosition; }
    MStatus getRotation(MQuaternion& q,MSpace::Space) { q=fixture::parentRotation; return fixture::check(); }
    MStatus setTranslation(MVector p,MSpace::Space) { fixture::parentPosition=p; return fixture::check(); }
    MStatus setRotation(MQuaternion q) { fixture::parentRotation=q; return fixture::check(); }
};
struct FakeShape : collision_shape_impl_t {
    typedef std::shared_ptr<FakeShape> pointer;
    vec3f scale=vec3f(1,1,1), inertia=vec3f(1,2,3), centerValue=vec3f(0,0,0);
    bool tinyActualInertia=false;
    quatf rotationValue=mvl::qidentity<float>();
    scale_update prepare_scale(const vec3f& s, bool = false) override {
        if(fixture::reject || !dommy::bullet::isFiniteVector(s) || !dommy::bullet::isSafeCollisionCoordinate(s[0])) return scale_update();
        const vec3f candidate=tinyActualInertia && s[0]!=1 ? vec3f(1e-40f,1,1) :
            vec3f(s[0]*s[0],2*s[0]*s[0],3*s[0]*s[0]);
        scale_update result(new collision_shape_scale_update([this,s,candidate]() { scale=s; inertia=candidate; }));
        // A compound's conservative bound can hide an unsafe small actual inertia.
        result->inertia=tinyActualInertia ? vec3f(4,8,12) : candidate; return result;
    }
    void gl_draw(size_t) override {}
    void get_scale(vec3f& s) override { s=scale; }
    float volume() override { return 1; }
    const vec3f& local_inertia() override { return inertia; }
    const vec3f& center() override { return centerValue; }
    const quatf& rotation() override { return rotationValue; }
    btCollisionShape* getBulletCollisionShape() override { return nullptr; }
    void getCenterOfMassTransformShift(btTransform&) override {}
};
class btTransform {
public:
    vec3f position=vec3f(0,0,0); quatf rotation=mvl::qidentity<float>();
};
struct btRigidBody {
    btTransform world,interpolation;
    const btTransform& getWorldTransform() const { return world; }
    const btTransform& getInterpolationWorldTransform() const { return interpolation; }
    void setWorldTransform(const btTransform& value) { world=value; }
    void updateInertiaTensor() {} // Native tensor behavior is covered by bullet_pose_native_tests.
    void setInterpolationWorldTransform(const btTransform& value) { interpolation=value; }
};
struct DB_RigidBody_t {
    virtual ~DB_RigidBody_t()=default;
    btRigidBody native;
    int constraintFrame=17;
    btRigidBody* body() { return &native; }
    std::vector<std::function<void()> > capture_constraint_frames() {
        fixture::mayThrow(); const int saved=constraintFrame;
        return {[this,saved]() { constraintFrame=saved; }};
    }
};
struct FakeBody : DB_RigidBody_t {
    typedef std::shared_ptr<FakeBody> pointer;
    std::shared_ptr<FakeShape> shape{new FakeShape()};
    float mass=1; vec3f inertia=vec3f(1,2,3);
    bool kinematic=false;
    std::shared_ptr<FakeShape> collision_shape() { return shape; }
    DB_RigidBody_t* impl() { return this; }
    void get_transform(vec3f& p,quatf& q) { fixture::mayThrow(); p=native.world.position; q=native.world.rotation; }
    void set_transform(const vec3f& p,const quatf& q) {
        native.world.position=p; native.world.rotation=q; native.interpolation=native.world; fixture::mayThrow();
    }
    void set_interpolation_transform(const vec3f& p,const quatf& q) { native.interpolation.position=p; native.interpolation.rotation=q; }
    void update_constraint() { constraintFrame=99; fixture::mayThrow(); }
    float get_mass() const { return mass; }
    void set_mass(float v) { mass=v; }
    void set_inertia(const vec3f& v) { inertia=v; }
    bool set_mass_properties(float m,const vec3f& i,bool k) {
        if(!dommy::bullet::safeBodyMassProperties(m,i)) return false;
        mass=m; inertia=i; kinematic=k; return true;
    }
    void set_kinematic(bool) {}
    void set_restitution(float) {} void set_friction(float) {}
    void set_linear_damping(float) {} void set_angular_damping(float) {}
};
using dbCollisionShape_t=FakeShape;
using dbRigidBody_t=FakeBody;
namespace fixture { std::shared_ptr<FakeShape> sourceShape; }
struct dbSolver_t {
    static bool lock_solver() { fixture::mayThrow(); return true; }
    static FakeShape::pointer create_sphere_shape() { ++fixture::fallbackShapes; return fixture::sourceShape; }
    static FakeBody::pointer create_rigid_body(const FakeShape::pointer& shape) {
        if(fixture::failCreate) return FakeBody::pointer();
        FakeBody::pointer body(new FakeBody()); body->shape=shape; return body;
    }
    static bool add_rigid_body(const FakeBody::pointer&,const char*) { return !fixture::failAdd; }
    static void remove_rigid_body(const FakeBody::pointer&) { ++fixture::removed; }
    static bool reconfigure_rigid_body(const std::shared_ptr<FakeBody>& b,float m,const vec3f& i,bool k,const char*) {
        if(fixture::failReconfigure) return false;
        return b->set_mass_properties(m,i,k);
    }
    static bool reconfigure_rigid_bodies(const std::vector<FakeBody::pointer>& b,float m,
                                       const std::vector<vec3f>& i,bool k,const char*) {
        if(fixture::failReconfigure) return false;
        for(std::size_t j=0;j<b.size();++j) assert(b[j]->set_mass_properties(m,i[j],k));
        return true;
    }
};
struct DBSolverNode { static bool isStartFrame() { return fixture::start; } };
struct Name { const char* asChar() const { return "body"; } };
struct DBRigidBodyNode {
    enum { ca_rigidBody,ca_rigidBodyParam,ia_mass,ia_restitution,ia_friction,ia_linearDamping,ia_angularDamping,ia_initialPosition,ia_initialRotation,ia_collisionShape };
    std::shared_ptr<FakeBody> m_rigid_body{new FakeBody()};
    MObject thisMObject() const { return MObject(); }
    Name name() const { return Name(); }
    MStatus computeWorldMatrix(const MPlug&,MDataBlock&);
    MStatus computeRigidBody(const MPlug&,MDataBlock&);
    MStatus computeRigidBodyParam(const MPlug&,MDataBlock&);
};
struct DBRigidBodyArrayNode {
    enum { ca_rigidBodies=0,ca_rigidBodyParam=1,ia_mass=2,ia_restitution,ia_friction,ia_linearDamping,ia_angularDamping };
    std::vector<FakeBody::pointer> m_rigid_bodies{FakeBody::pointer(new FakeBody()),FakeBody::pointer(new FakeBody())};
    MObject thisMObject() const { return MObject(); }
    Name name() const { return Name(); }
    MStatus computeRigidBodyParam(const MPlug&,MDataBlock&);
};
struct DBCollisionShapeNode {
    enum { typeId=1 };
    enum { ca_collisionShape,ia_scale,ca_collisionShapeParam };
    std::shared_ptr<FakeShape> m_collision_shape{new FakeShape()};
    MObject thisMObject() const { return MObject(); }
    MStatus computeCollisionShapeParam(const MPlug&,MDataBlock&);
    MStatus evaluate_collision_shape(FakeShape::pointer& shape) { shape=m_collision_shape; return MS::kSuccess; }
};
struct FakeSoftBody {
    float mass=1;
    std::size_t vertex_count() const { return 4; }
    bool set_mass(float m) {
        std::vector<float> values;
        if(fixture::failReconfigure || !dommy::bullet::prepareSoftBodyMass(m,{.25,.25,.25,.25},values)) return false;
        mass=m; return true;
    }
    bool set_num_clusters(std::size_t) { return true; }
    bool set_parameters(const dommy::bullet::SoftBodyParameters& p) { return set_mass(p.mass); }
    void set_dynamic_friction_coeff(float) {} void set_collision_margin(float) {}
};
struct DBSoftBodyNode {
    enum {ca_softBody,ca_softBodyParam,ia_mass,ia_dynamicFrictionCoeff,ia_collisionMargin,ia_numClusters};
    std::shared_ptr<FakeSoftBody> m_soft_body{new FakeSoftBody()};
    MObject thisMObject() const { return MObject(); }
    MStatus readSoftBodyParameters(dommy::bullet::SoftBodyParameters&,MDataBlock* = nullptr) const;
    MStatus computeSoftBodyParam(const MPlug&,MDataBlock&);
};
MStatus readRigidBodyParameters(MDataBlock&,int,int,int,int,int,dommy::bullet::RigidBodyParameters& p) {
    const auto status=fixture::check(); if(!status) return status;
    return dommy::bullet::validateRigidBodyParameters(fixture::mass,0,0,0,0,p) ? MS::kSuccess : MS::kInvalidParameter;
}

#include "bullet_node_methods.inc"

static void preparePoseTest(DBRigidBodyNode& node,bool start) {
    fixture::reset(); fixture::start=start;
    fixture::parentPosition=MVector(5,0,0); fixture::parentRotation=MQuaternion(0,0,1,0);
    node.m_rigid_body->native.interpolation.position=vec3f(9,8,7);
}
static void assertPoseUnchanged(const DBRigidBodyNode& node) {
    assert(node.m_rigid_body->native.world.position[0]==0 && node.m_rigid_body->native.world.rotation[0]==1);
    assert(node.m_rigid_body->native.interpolation.position[0]==9);
    assert(node.m_rigid_body->constraintFrame==17);
    assert(fixture::initialPosition[0]==100 && fixture::initialRotation[2]==33);
    assert(fixture::parentPosition.x==5 && fixture::parentRotation.z==1);
}
static void assertPoseUpdated(const DBRigidBodyNode& node,bool start) {
    if(start) {
        assert(node.m_rigid_body->native.world.position[0]==5 && node.m_rigid_body->native.world.rotation[3]==1);
        assert(node.m_rigid_body->native.interpolation.position[0]==5 && node.m_rigid_body->constraintFrame==99);
        assert(fixture::initialPosition[0]==5 && std::fabs(fixture::initialRotation[2]-180)<1e-4);
        assert(fixture::parentPosition.x==5 && fixture::parentRotation.z==1);
    } else {
        assert(node.m_rigid_body->native.world.position[0]==0 && node.m_rigid_body->constraintFrame==17);
        assert(node.m_rigid_body->native.interpolation.position[0]==9);
        assert(fixture::initialPosition[0]==100 && fixture::initialRotation[2]==33);
        assert(fixture::parentPosition.x==0 && fixture::parentRotation.w==1);
    }
}
static void testPoseRecovery() {
    MPlug plug; MDataBlock data;
    int totalStatus=0,totalExceptions=0;
    for(bool start:{true,false}) {
        DBRigidBodyNode good; preparePoseTest(good,start);
        assert(good.computeWorldMatrix(plug,data)); assertPoseUpdated(good,start);
        const int statusCount=fixture::checks,exceptionCount=fixture::exceptionChecks;
        totalStatus+=statusCount; totalExceptions+=exceptionCount;
        assert(good.computeWorldMatrix(plug,data)); assertPoseUpdated(good,start);
        for(bool exception:{false,true}) {
            for(int failure=1;failure<=(exception?exceptionCount:statusCount);++failure) {
                DBRigidBodyNode node; preparePoseTest(node,start);
                if(exception) fixture::throwAt=failure; else fixture::failAt=failure;
                bool success=false;
                try { success=bool(node.computeWorldMatrix(plug,data)); }
                catch(const std::runtime_error&) { assert(exception); }
                assert(!success && fixture::clean==0); assertPoseUnchanged(node);
                fixture::throwAt=fixture::failAt=0;
                assert(node.computeWorldMatrix(plug,data)); assertPoseUpdated(node,start);
                assert(node.computeWorldMatrix(plug,data)); assertPoseUpdated(node,start);
            }
        }
        for(bool rejectedScale:{false,true}) {
            DBRigidBodyNode node; preparePoseTest(node,start);
            fixture::reject=rejectedScale; fixture::failReconfigure=!rejectedScale;
            assert(!node.computeWorldMatrix(plug,data)); assertPoseUnchanged(node);
        }
    }
    DBRigidBodyNode invalidEuler; preparePoseTest(invalidEuler,true);
    fixture::parentRotation.z=std::numeric_limits<float>::max();
    assert(!invalidEuler.computeWorldMatrix(plug,data));
    assert(invalidEuler.m_rigid_body->native.world.position[0]==0 && fixture::initialRotation[2]==33);
    std::printf("Single rigid pose recovery: %d status failures, %d exceptions, constraints/initial arrays/parent and repeated retry passed\n",
        totalStatus,totalExceptions);
}

int main() {
    testPoseRecovery();
    MPlug plug; MDataBlock data;
    fixture::reset(); fixture::failShapeQuery=true;
    DBRigidBodyNode brokenInput;
    const auto originalBody=brokenInput.m_rigid_body;
    fixture::sourceShape=originalBody->shape;
    assert(!brokenInput.computeRigidBody(plug,data));
    assert(brokenInput.m_rigid_body==originalBody && fixture::fallbackShapes==0);
    assert(fixture::removed==0 && fixture::published==0 && fixture::clean==0);
    fixture::sourceShape.reset();
    for(double mass:{0.0,1.0,1e-40,1e-50}) {
        fixture::reset(); fixture::mass=mass;
        DBSoftBodyNode soft;
        const bool good=mass==0 || mass==1;
        assert(bool(soft.computeSoftBodyParam(plug,data))==good);
        assert(soft.m_soft_body->mass==(good?mass:1));
        assert(fixture::published==(good?1:0));
    }
    fixture::reset(); fixture::failReconfigure=true;
    DBSoftBodyNode soft;
    assert(!soft.computeSoftBodyParam(plug,data) && fixture::published==0);
    fixture::reset();
    DBRigidBodyArrayNode array;
    for(float mass:{0.0f,1.0f,0.0f,2.0f}) {
        fixture::mass=mass;
        assert(array.computeRigidBodyParam(plug,data));
        for(auto& body:array.m_rigid_bodies) {
            assert(body->mass==mass && body->kinematic==(mass==0));
            assert(body->inertia[2]==3*mass);
        }
    }
    for(int failure=0;failure<4;++failure) {
        fixture::reset(); fixture::mass=3;
        DBRigidBodyArrayNode node;
        fixture::failOutput=failure==0; fixture::failReconfigure=failure==1;
        if(failure==2) fixture::mass=1e-40f;
        if(failure==3) node.m_rigid_bodies[1]->shape->inertia[0]=1e-40f;
        assert(!node.computeRigidBodyParam(plug,data));
        for(auto& body:node.m_rigid_bodies) assert(body->mass==1 && !body->kinematic);
        assert(fixture::clean==0 && fixture::published==0);
    }
    for(float invalid:{1e-40f,1e38f}) {
        fixture::reset(); fixture::mass=invalid;
        DBRigidBodyNode node;
        assert(!node.computeRigidBodyParam(plug,data));
        assert(node.m_rigid_body->mass==1 && fixture::published==0);
    }
    for(int failure=0;failure<5;++failure) {
        fixture::reset(); fixture::scale[0]=2;
        fixture::reject=failure==1; fixture::failCreate=failure==2; fixture::failAdd=failure==3;
        if(failure==4) fixture::mass=std::numeric_limits<float>::max();
        DBRigidBodyNode node;
        const auto previous=node.m_rigid_body;
        fixture::sourceShape=previous->shape;
        const auto status=node.computeRigidBody(plug,data);
        assert(bool(status)==(failure==0));
        if(failure) {
            assert(node.m_rigid_body==previous && fixture::sourceShape->scale[0]==1);
            assert(previous->inertia[0]==1 && fixture::removed==0 && fixture::clean==0);
        } else {
            assert(node.m_rigid_body!=previous && fixture::removed==1);
            assert(fixture::sourceShape->scale[0]==2 && node.m_rigid_body->inertia[2]==12);
        }
        fixture::sourceShape.reset();
    }
    for(bool start:{false,true}) {
        fixture::reset(); fixture::start=start; fixture::scale[0]=2;
        DBRigidBodyNode node;
        assert(node.computeWorldMatrix(plug,data));
        assert(node.m_rigid_body->shape->scale[0]==2);
        assert(node.m_rigid_body->inertia[0]==4 && node.m_rigid_body->inertia[2]==12);
    }
    for(bool creating:{false,true}) {
        fixture::reset(); fixture::scale[0]=2;
        DBRigidBodyNode node;
        const auto previous=node.m_rigid_body;
        previous->shape->tinyActualInertia=true;
        fixture::sourceShape=previous->shape;
        assert(!(creating ? node.computeRigidBody(plug,data) : node.computeWorldMatrix(plug,data)));
        assert(node.m_rigid_body==previous && previous->mass==1);
        assert(previous->shape->scale[0]==1 && previous->shape->inertia[0]==1);
        assert(previous->inertia[0]==1 && fixture::removed==0 && fixture::clean==0);
        fixture::sourceShape.reset();
    }
    for(int failure=0;failure<3;++failure) {
        fixture::reset(); fixture::scale[0]=2;
        fixture::reject=failure==0;
        if(failure==1) fixture::mass=std::numeric_limits<float>::max();
        if(failure==2) { fixture::mass=0; fixture::failReconfigure=true; }
        DBRigidBodyNode node;
        assert(!node.computeWorldMatrix(plug,data));
        assert(node.m_rigid_body->shape->scale[0]==1 && node.m_rigid_body->inertia[0]==1);
        assert(node.m_rigid_body->mass==1 && fixture::clean==0);
    }
    for(int failure=0;failure<3;++failure) {
        fixture::reset(); fixture::scale[0]=2;
        fixture::reject=failure==1; fixture::failOutput=failure==2;
        DBCollisionShapeNode node;
        const auto status=node.computeCollisionShapeParam(plug,data);
        assert(bool(status)==(failure==0));
        assert(node.m_collision_shape->scale[0]==(failure==0?2:1));
        assert(fixture::published==(failure==0?1:0));
        if(failure) assert(fixture::clean==0);
    }
    std::puts("Bullet production node callbacks: inertia ordering, rejected scales, mass overflow and world/output failures passed");
}
