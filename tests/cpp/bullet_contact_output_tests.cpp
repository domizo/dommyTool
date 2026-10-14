// Complete production contact/output callbacks with Maya and native-world
// doubles. No Maya lifetime or Bullet collision behavior is simulated here.
#include "maya/MStatus.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>
#define DOMMY_WITH_DG_FILE_WRITERS 0
using MString = std::string;
using MTypeId = int;
struct vec3f { float v[3]; float& operator[](int i) { return v[i]; } const float& operator[](int i) const { return v[i]; } };
struct quatf {
    float v[4];
    quatf(float w=1,float x=0,float y=0,float z=0):v{w,x,y,z} {}
    float& operator[](int i) { return v[i]; }
    const float& operator[](int i) const { return v[i]; }
};
struct MVector { double x,y,z; MVector(double a=0,double b=0,double c=0):x(a),y(b),z(c) {} };
struct MEulerRotation { double x,y,z; MEulerRotation(double a,double b,double c):x(a),y(b),z(c) {} };
namespace fixture {
int calls=0, failAt=0, writes=0, contactsWritten=0, lookups=0;
bool invalidEuler=false, solverAvailable=true, worldAvailable=true, dispatcherAvailable=true;
std::function<void()> onContactWrite;
std::vector<double> values;
std::vector<MString> names;
MStatus operation() { return ++calls==failAt ? MS::kFailure : MS::kSuccess; }
void reset() {
    calls=failAt=writes=contactsWritten=lookups=0;
    invalidEuler=false; solverAvailable=worldAvailable=dispatcherAvailable=true;
    onContactWrite={}; values.clear(); names.clear();
}
MStatus write(double value) {
    const MStatus status=operation(); if (!status) return status;
    assert(std::isfinite(value)); ++writes; values.push_back(value); return MS::kSuccess;
}
}
struct MQuaternion {
    double x,y,z,w;
    MQuaternion(double a,double b,double c,double d):x(a),y(b),z(c),w(d) {}
    MEulerRotation asEulerRotation() const {
        return {fixture::invalidEuler ? std::numeric_limits<double>::infinity() : x,y,z};
    }
};
double rad2deg(double v) { return v*180/3.141592653589793; }
namespace MSpace { enum Space { kTransform }; }
// Small limits let the fixture execute the exact boundary without huge allocations.
namespace dommy { namespace generation {
const std::size_t kMaxGeneratedTransformCount=3;
} }
struct NodeState {
    int type=1; bool valid=true, alive=true; unsigned parents=1;
    float mass=1; void* user=nullptr;
    std::vector<std::shared_ptr<NodeState>> constraints;
};
struct MObject {
    std::shared_ptr<NodeState> state;
    bool operator!=(const MObject& rhs) const { return state!=rhs.state; }
};
struct MObjectHandle {
    MObject value;
    explicit MObjectHandle(MObject object):value(object) {}
    bool isValid() const { return value.state && value.state->valid; }
    bool isAlive() const { return value.state && value.state->alive; }
    MObject object() const { return value; }
    bool operator!=(MObject rhs) const { return value!=rhs; }
};
struct MFnDependencyNode {
    MObject object;
    MFnDependencyNode(MObject o,MStatus* status):object(o) { ++fixture::lookups; *status=fixture::operation(); }
    MTypeId typeId(MStatus* status=nullptr) const { if(status)*status=fixture::operation(); return object.state->type; }
};
struct MFnDagNode : MFnDependencyNode {
    using MFnDependencyNode::MFnDependencyNode;
    unsigned parentCount(MStatus* status) const { *status=fixture::operation(); return object.state->parents; }
    MObject parent(int,MStatus* status) const { *status=fixture::operation(); return object; }
};
namespace dommy { namespace maya {
template<class T> T* checkedUserNode(const MFnDependencyNode& fn,int type) {
    if(fn.object.state->type!=type) return nullptr;
    return static_cast<T*>(fn.object.state->user);
}
} }
struct MPlugArray;
struct MPlug {
    MObject object; int attribute=0;
    MPlug()=default; MPlug(MObject o,int a):object(o),attribute(a) {}
    MObject node() const { return object; }
    MStatus getValue(float& v) const { v=object.state->mass; return fixture::operation(); }
    MPlug elementByLogicalIndex(unsigned,MStatus* status) const { *status=fixture::operation(); return *this; }
    MPlug child(int) const { return *this; }
    MStatus setValue(double v) const { return fixture::write(v); }
    bool connectedTo(MPlugArray&,bool,bool,MStatus*) const;
};
struct MPlugArray : std::vector<MPlug> {
    using std::vector<MPlug>::operator=;
    unsigned length() const { return static_cast<unsigned>(size()); }
};
bool MPlug::connectedTo(MPlugArray& targets,bool,bool,MStatus* status) const {
    *status=fixture::operation();
    if(!*status)return false;
    for(const auto& target:object.state->constraints) targets.emplace_back(MObject{target},0);
    return !targets.empty();
}
struct MFnTransform {
    MFnTransform(MObject,MStatus* status) { *status=fixture::operation(); }
    MStatus setRotation(const MQuaternion& q) { return fixture::write(q.x+q.y+q.z+q.w); }
    MStatus setRotation(const MEulerRotation& q) { return fixture::write(q.x+q.y+q.z); }
    MStatus setTranslation(const MVector& p,MSpace::Space) { return fixture::write(p.x+p.y+p.z); }
};
struct btCollisionObject {};
struct DB_RigidBody_t { const btCollisionObject* value; const btCollisionObject* body() const { return value; } };
struct dbRigidBody_t {
    using pointer=std::shared_ptr<dbRigidBody_t>;
    DB_RigidBody_t native;
    vec3f position{{1,2,3}}; quatf rotation{1,0,0,0};
    explicit dbRigidBody_t(const btCollisionObject* value):native{value} {}
    DB_RigidBody_t* impl() { return &native; }
    void get_transform(vec3f& p,quatf& q) { p=position; q=rotation; }
};
struct DBRigidBodyNode {
    enum { typeId=1, ia_mass=10, message=100 };
    dbRigidBody_t::pointer body;
    MString label;
    dbRigidBody_t::pointer cached_rigid_body() const { return body; }
    MString name() const { return label; }
    MStatus addContactInfo(const MString& other,const MVector& point) {
        const MStatus status=fixture::operation(); if(!status)return status;
        assert(std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z));
        fixture::names.push_back(other); ++fixture::contactsWritten;
        if(fixture::onContactWrite) fixture::onContactWrite();
        return MS::kSuccess;
    }
};
struct DBRigidBodyArrayNode {
    enum {typeId=2,ia_mass=20,io_position,io_rotation};
    std::vector<dbRigidBody_t::pointer> bodies;
    const std::vector<dbRigidBody_t::pointer>& cached_rigid_bodies() const { return bodies; }
};
struct Constraint {
    using pointer=std::shared_ptr<Constraint>;
    vec3f position{{4,5,6}}; quatf rotation{1,0,0,0};
    void get_world(vec3f& p) const { p=position; }
    void get_world(vec3f& p,quatf& q) const { p=position;q=rotation; }
};
using dbNail_constraint_t=Constraint;
using dbHinge_constraint_t=Constraint;
using dbSlider_constraint_t=Constraint;
using dbSixdof_constraint_t=Constraint;
template<int Id> struct ConstraintNode {
    enum {typeId=Id};
    Constraint::pointer body=std::make_shared<Constraint>();
    Constraint::pointer cached_constraint() const { return body; }
};
using DBNailConstraintNode=ConstraintNode<3>;
using DBHingeConstraintNode=ConstraintNode<4>;
using DBSliderConstraintNode=ConstraintNode<5>;
using DBSixdofConstraintNode=ConstraintNode<6>;
struct btVector3 { float x,y,z; float getX() const{return x;} float getY() const{return y;} float getZ() const{return z;} };
struct btManifoldPoint {
    btVector3 a{1,2,3},b{4,5,6};
    btVector3 getPositionWorldOnA() const{return a;} btVector3 getPositionWorldOnB() const{return b;}
};
struct btPersistentManifold {
    const btCollisionObject* a; const btCollisionObject* b;
    std::vector<btManifoldPoint> points;
    const btCollisionObject* getBody0() const{return a;} const btCollisionObject* getBody1() const{return b;}
    int getNumContacts() const{return static_cast<int>(points.size());}
    const btManifoldPoint& getContactPoint(int i) const{return points.at(i);}
};
struct Dispatcher {
    std::vector<std::unique_ptr<btPersistentManifold>> manifolds;
    int getNumManifolds() const{return static_cast<int>(manifolds.size());}
    btPersistentManifold* getManifoldByIndexInternal(int i){return manifolds.at(i).get();}
};
struct btCollisionWorld {
    Dispatcher dispatcher;
    Dispatcher* getDispatcher(){return fixture::dispatcherAvailable?&dispatcher:nullptr;}
};
struct btSoftRigidDynamicsWorld : btCollisionWorld { btCollisionWorld* getCollisionWorld(){return this;} };
struct DB_BulletSolver_t {
    btSoftRigidDynamicsWorld world;
    btSoftRigidDynamicsWorld* dyanmicsWorld(){return fixture::worldAvailable?&world:nullptr;}
};
static DB_BulletSolver_t backend;
struct dbSolver_t {
    struct LockedSolverAccess { DB_BulletSolver_t* get(){return fixture::solverAvailable?&backend:nullptr;} };
    static LockedSolverAccess lock_solver(){return {};}
};
struct DBSolverNode {
#include "bullet_contact_types.inc"
    typedef std::map<const btCollisionObject*,ContactNode> ContactNodes;
    ContactNodes m_contactNodes;
    MStatus refreshContactNodes(const MPlugArray&);
    MStatus resolveRigidBodyNode(const btCollisionObject*,DBRigidBodyNode*&) const;
    MStatus updateContactInfo(MPlugArray&);
    MStatus updateActiveRigidBodies(MPlugArray&);
    MStatus clearContactRelatedAttributes(MPlugArray&){return fixture::operation();}
    MStatus updateConstraint(MObject&);
};
#include "bullet_contact_output.inc"
struct Scene {
    btCollisionObject nativeA,nativeB,unknown;
    DBRigidBodyNode a,b;
    DBRigidBodyArrayNode array;
    MObject objectA{std::make_shared<NodeState>()},objectB{std::make_shared<NodeState>()},objectArray{std::make_shared<NodeState>()};
    MPlugArray connections;
    Scene() {
        fixture::reset();
        a.body=std::make_shared<dbRigidBody_t>(&nativeA); b.body=std::make_shared<dbRigidBody_t>(&nativeB);
        a.label="A";b.label="B"; objectA.state->user=&a;objectB.state->user=&b;
        array.bodies.push_back(a.body); objectArray.state->type=2;objectArray.state->user=&array;
        connections.emplace_back(objectA,0);connections.emplace_back(objectB,0);
        backend.world.dispatcher.manifolds.clear();
        std::unique_ptr<btPersistentManifold> manifold(new btPersistentManifold{&nativeA,&nativeB,{btManifoldPoint(),btManifoldPoint()}});
        backend.world.dispatcher.manifolds.push_back(std::move(manifold));
    }
};
template<class Node> int checkConstraintOutput() {
    int operations=0;
    // Exercise the complete single-body output path, including propagation of
    // constraint failure after the rigid transform was already written.
    for(int fault=-1;fault<11;++fault) {
        Scene scene; DBSolverNode solver; Node node;
        auto state=std::make_shared<NodeState>();state->type=Node::typeId;state->user=&node;
        scene.objectA.state->constraints.push_back(state);
        if(fault==0)state->user=nullptr;
        if(fault==1)node.body.reset();
        if(fault==2)state->parents=0;
        if(fault>=3&&fault<6)node.body->position[fault-3]=std::numeric_limits<float>::infinity();
        if(fault>=6&&fault<10)node.body->rotation[fault-6]=std::numeric_limits<float>::quiet_NaN();
        if(fault==10){state->type=99;state->parents=0;state->user=nullptr;}
        const bool accepted=fault==-1||fault==10||(Node::typeId==3&&fault>=6&&fault<10);
        const MStatus status=solver.updateConstraint(scene.objectA);
        assert(bool(status)==accepted);
        assert(fixture::writes==(accepted&&fault!=10?2:0));
        if(fault==-1) {
            operations=fixture::calls;
            assert(fixture::values[0]==15);
            assert(fixture::values[1]==(Node::typeId==3?0:1));
        }
        if(!accepted) {
            fixture::writes=0;fixture::values.clear();
            assert(!solver.updateActiveRigidBodies(scene.connections));
            assert(fixture::writes==2);
        }
    }
    for(int failure=1;failure<=operations;++failure) {
        Scene scene; DBSolverNode solver; Node node;
        auto state=std::make_shared<NodeState>();state->type=Node::typeId;state->user=&node;
        scene.objectA.state->constraints.push_back(state);
        fixture::failAt=failure;
        assert(!solver.updateConstraint(scene.objectA));
        assert(fixture::writes==(failure==operations?1:0));
    }
    return operations;
}
int main() {
    const int constraintFailures=checkConstraintOutput<DBNailConstraintNode>()+
        checkConstraintOutput<DBHingeConstraintNode>()+
        checkConstraintOutput<DBSliderConstraintNode>()+
        checkConstraintOutput<DBSixdofConstraintNode>();
    {
        Scene scene; DBSolverNode solver; DBRigidBodyNode* found=nullptr;
        assert(solver.refreshContactNodes(scene.connections));
        assert(solver.resolveRigidBodyNode(&scene.nativeA,found)&&found==&scene.a);
        scene.a.body=std::make_shared<dbRigidBody_t>(&scene.unknown);
        assert(!solver.resolveRigidBodyNode(&scene.nativeA,found)&&!found);
        assert(solver.refreshContactNodes(scene.connections));
        assert(solver.resolveRigidBodyNode(&scene.nativeA,found)&&!found);
        assert(solver.resolveRigidBodyNode(&scene.unknown,found)&&found==&scene.a);
        scene.connections.erase(scene.connections.begin());
        assert(solver.refreshContactNodes(scene.connections));
        assert(solver.resolveRigidBodyNode(&scene.unknown,found)&&!found);
    }
    for(int failure=0;failure<6;++failure) {
        Scene scene; DBSolverNode solver; DBRigidBodyNode* found=&scene.b;
        assert(solver.refreshContactNodes(scene.connections));
        auto retained=scene.a.body;
        if(failure==0) scene.objectA.state->valid=false;
        if(failure==1) scene.objectA.state->alive=false;
        if(failure==2) {scene.a.body.reset();scene.array.bodies.clear();retained.reset();}
        if(failure==3) scene.a.body=std::make_shared<dbRigidBody_t>(&scene.nativeA); // Reused address, live old facade.
        if(failure==4) scene.a.body->native.value=&scene.unknown;
        if(failure==5) scene.objectA.state->user=nullptr;
        const int lookups=fixture::lookups;
        assert(!solver.resolveRigidBodyNode(&scene.nativeA,found)&&!found);
        if(failure<3)assert(fixture::lookups==lookups);
    }
    {
        Scene scene; DBSolverNode solver;
        scene.connections.push_back(scene.connections[0]);
        assert(solver.refreshContactNodes(scene.connections)&&solver.m_contactNodes.size()==2);
        scene.b.body=scene.a.body;
        assert(!solver.refreshContactNodes(scene.connections)&&solver.m_contactNodes.empty());
    }
    for(int action=0;action<6;++action) {
        Scene scene; DBSolverNode solver;
        assert(solver.refreshContactNodes(scene.connections));
        scene.a.body=std::make_shared<dbRigidBody_t>(&scene.nativeA);
        fixture::onContactWrite=[&] {
            if(fixture::contactsWritten!=1)return;
            if(action==1)backend.world.dispatcher.manifolds.clear();
            if(action==2)scene.objectB.state->alive=false;
            if(action==3)scene.a.body=std::make_shared<dbRigidBody_t>(&scene.nativeA);
            if(action==4)scene.objectA.state->valid=false;
            if(action==5)scene.b.body.reset();
        };
        assert(bool(solver.updateContactInfo(scene.connections))==(action<2));
        assert(fixture::contactsWritten==(action<2?4:1));
        assert(fixture::names[0]=="B");
        if(action<2)assert(fixture::names[1]=="A");
    }
    for(int failure=0;failure<5;++failure) {
        Scene scene; DBSolverNode solver;
        if(failure==0)fixture::solverAvailable=false;
        if(failure==1)fixture::worldAvailable=false;
        if(failure==2)fixture::dispatcherAvailable=false;
        if(failure==3)backend.world.dispatcher.manifolds[0]->points[1].a.x=std::numeric_limits<float>::infinity();
        if(failure==4)backend.world.dispatcher.manifolds[0]->points.resize(4);
        assert(!solver.updateContactInfo(scene.connections)&&fixture::contactsWritten==0);
    }
    {
        Scene scene; DBSolverNode solver;
        backend.world.dispatcher.manifolds[0]->b=&scene.unknown;
        backend.world.dispatcher.manifolds[0]->points.resize(4);
        backend.world.dispatcher.manifolds[0]->points[0].a.x=std::numeric_limits<float>::infinity();
        assert(solver.updateContactInfo(scene.connections)&&fixture::contactsWritten==0);
    }
    int injected=0;
    for(int path=0;path<3;++path) {
        int operations;
        {
            Scene scene; DBSolverNode solver;
            if(path==2)scene.connections={MPlug(scene.objectArray,0)};
            assert(path==0?solver.updateContactInfo(scene.connections):solver.updateActiveRigidBodies(scene.connections));
            operations=fixture::calls;
        }
        for(int failure=1;failure<=operations;++failure) {
            Scene scene; DBSolverNode solver;
            if(path==2)scene.connections={MPlug(scene.objectArray,0)};
            fixture::failAt=failure;
            assert(!(path==0?solver.updateContactInfo(scene.connections):solver.updateActiveRigidBodies(scene.connections)));
            ++injected;
        }
    }
    for(int kind:{1,2}) for(int failure=0;failure<6;++failure) {
        Scene scene; DBSolverNode solver;
        scene.connections={MPlug(kind==1?scene.objectA:scene.objectArray,0)};
        if(failure==0)(kind==1?scene.objectA:scene.objectArray).state->user=nullptr;
        if(failure==1){scene.a.body.reset();scene.array.bodies[0].reset();}
        if(failure==2){scene.objectA.state->parents=0;scene.array.bodies.resize(4,scene.a.body);}
        if(failure==3)scene.a.body->position[0]=std::numeric_limits<float>::infinity();
        if(failure==4)scene.a.body->rotation[3]=std::numeric_limits<float>::quiet_NaN();
        if(failure==5) {if(kind==1)continue;fixture::invalidEuler=true;}
        assert(!solver.updateActiveRigidBodies(scene.connections)&&fixture::writes==0);
    }
    {
        Scene scene; DBSolverNode solver;
        scene.connections={MPlug(scene.objectArray,0)};scene.array.bodies.clear();
        assert(solver.updateActiveRigidBodies(scene.connections)&&fixture::writes==0);
    }
    std::printf("Legacy contacts/rigid/constraint output: generations, liveness, callback invalidation, finite output and %d + %d injected failures passed\n",injected,constraintFailures);
}
