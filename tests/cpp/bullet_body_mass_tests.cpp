// Real Bullet bodies/world; complete production mass/registration methods are
// injected by the runner. Maya, unrelated bridge methods and failure hooks are doubles.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "modules/dynamics/DBullet/Collision/DShapeFrameMath.h"
#include "modules/dynamics/DBullet/Collision/DCollisionMeshInput.h"

static long allocationBudget=-1;
void* operator new(std::size_t size) {
    if(allocationBudget==0) throw std::bad_alloc();
    if(allocationBudget>0) --allocationBudget;
    void* value=std::malloc(size ? size : 1);
    if(!value) throw std::bad_alloc();
    return value;
}
void operator delete(void* value) noexcept { std::free(value); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* value) noexcept { ::operator delete(value); }

class rigid_body_impl_t {
public:
    virtual ~rigid_body_impl_t() {}
    virtual float get_mass() const=0;
    virtual void set_mass(float)=0;
    virtual void set_inertia(const vec3f&)=0;
    virtual void set_kinematic(bool)=0;
    virtual bool set_mass_properties(float,const vec3f&,bool)=0;
    virtual void get_mass_properties(float&,vec3f&,bool&) const=0;
};
class DB_RigidBody_t : public rigid_body_impl_t {
public:
    explicit DB_RigidBody_t(btCollisionShape& shape)
        : m_body(new btRigidBody(btRigidBody::btRigidBodyConstructionInfo(1,nullptr,&shape,btVector3(.4f,.4f,.4f)))) {}
    btRigidBody* body() { return m_body.get(); }
    #include "bullet_native_body.inc"
private:
    std::unique_ptr<btRigidBody> m_body;
    float m_mass=1;
    btVector3 m_inertia=btVector3(.4f,.4f,.4f);
};
class solver_impl_t {
public:
    virtual ~solver_impl_t() {}
    virtual bool add_rigid_body(rigid_body_impl_t*,const char*)=0;
    virtual void remove_rigid_body(rigid_body_impl_t*)=0;
    virtual bool set_gravity(const vec3f&)=0;
    virtual bool can_set_mass_properties(rigid_body_impl_t*,float,const vec3f&,bool) const=0;
};
struct WorldInfo { btVector3 m_gravity; };
class DB_BulletSolver_t : public solver_impl_t {
public:
    DB_BulletSolver_t(btDiscreteDynamicsWorld& world,WorldInfo& info) : m_dynamicsWorld(&world),m_worldInfo(&info) {}
    #include "bullet_native_solver.inc"
private:
    btDiscreteDynamicsWorld* m_dynamicsWorld;
    WorldInfo* m_worldInfo;
};
class FaultSolver : public DB_BulletSolver_t {
public:
    using DB_BulletSolver_t::DB_BulletSolver_t;
    int failAdd=0;
    bool failAfterAdd=false;
    bool add_rigid_body(rigid_body_impl_t* body,const char* name) override {
        if(failAdd>0 && --failAdd==0) {
            if(failAfterAdd) DB_BulletSolver_t::add_rigid_body(body,name);
            return false;
        }
        return DB_BulletSolver_t::add_rigid_body(body,name);
    }
};
class dbRigidBody_t : public DB_RigidBody_t {
public:
    typedef std::shared_ptr<dbRigidBody_t> pointer;
    using DB_RigidBody_t::DB_RigidBody_t;
    rigid_body_impl_t* impl() { return this; }
};
struct dbSolver_t {
    static std::recursive_mutex m_mutex;
    static std::shared_ptr<solver_impl_t> m_impl;
    static std::set<dbRigidBody_t::pointer> m_rigid_bodies;
    static bool add_rigid_body(const dbRigidBody_t::pointer&,const char*);
    static bool reconfigure_rigid_body(dbRigidBody_t::pointer&,float,const vec3f&,bool,const char*);
    static bool reconfigure_rigid_bodies(const std::vector<dbRigidBody_t::pointer>&,float,const std::vector<vec3f>&,bool,const char*);
    static bool set_gravity(const vec3f&);
};
std::recursive_mutex dbSolver_t::m_mutex;
std::shared_ptr<solver_impl_t> dbSolver_t::m_impl;
std::set<dbRigidBody_t::pointer> dbSolver_t::m_rigid_bodies;
#include "bullet_mass_registration.inc"

static void checkFinite(btRigidBody* body) {
    assert(std::isfinite(body->getInvMass()));
    for(int axis=0;axis<3;++axis) {
        assert(std::isfinite(body->getInvInertiaDiagLocal()[axis]));
        assert(std::isfinite(body->getTotalForce()[axis]));
        for(int j=0;j<3;++j) assert(std::isfinite(body->getInvInertiaTensorWorld()[axis][j]));
    }
}
int main() {
    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher(&configuration);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver constraintSolver;
    btDiscreteDynamicsWorld world(&dispatcher,&broadphase,&constraintSolver,&configuration);
    world.setGravity(btVector3(0,-9.81f,0));
    WorldInfo info{world.getGravity()};
    std::shared_ptr<FaultSolver> solver(new FaultSolver(world,info));
    dbSolver_t::m_impl=solver;
    btSphereShape sphere(1);
    std::vector<dbRigidBody_t::pointer> bodies={
        dbRigidBody_t::pointer(new dbRigidBody_t(sphere)),dbRigidBody_t::pointer(new dbRigidBody_t(sphere))};
    for(auto& body:bodies) assert(dbSolver_t::add_rigid_body(body,"test"));
    const std::vector<vec3f> normal(2,vec3f(.4f,.4f,.4f)),zero(2,vec3f(0,0,0));
    for(int repeat=0;repeat<3;++repeat) {
        assert(dbSolver_t::reconfigure_rigid_bodies(bodies,0,zero,true,"test"));
        for(auto& body:bodies) {
            assert(body->body()->isKinematicObject() && body->body()->getInvMass()==0);
            assert(body->body()->getBroadphaseHandle()->m_collisionFilterGroup==btBroadphaseProxy::StaticFilter);
        }
        assert(dbSolver_t::reconfigure_rigid_bodies(bodies,1,normal,false,"test"));
        for(auto& body:bodies) {
            assert(!body->body()->isStaticOrKinematicObject() && body->body()->getInvMass()==1);
            assert(body->body()->getBroadphaseHandle()->m_collisionFilterGroup==btBroadphaseProxy::DefaultFilter);
            checkFinite(body->body());
        }
    }
    for(bool after:{false,true}) {
        solver->failAdd=2; solver->failAfterAdd=after;
        assert(!dbSolver_t::reconfigure_rigid_bodies(bodies,0,zero,true,"test"));
        assert(world.getNumCollisionObjects()==2 && dbSolver_t::m_rigid_bodies.size()==2);
        for(auto& body:bodies) {
            assert(!body->body()->isStaticOrKinematicObject() && body->body()->getInvMass()==1);
            assert(body->body()->getBroadphaseHandle()->m_collisionFilterGroup==btBroadphaseProxy::DefaultFilter);
        }
    }
    bool completed=false;
    int allocationFailures=0;
    for(long budget=0;budget<30 && !completed;++budget) {
        allocationBudget=budget;
        try {
            const bool result=dbSolver_t::reconfigure_rigid_bodies(bodies,0,zero,true,"test");
            allocationBudget=-1;
            assert(result); completed=true;
        } catch(const std::bad_alloc&) {
            allocationBudget=-1; ++allocationFailures;
            for(auto& body:bodies) assert(!body->body()->isStaticOrKinematicObject() && body->body()->getInvMass()==1);
        }
    }
    assert(completed && allocationFailures>0);
    assert(dbSolver_t::reconfigure_rigid_bodies(bodies,1,normal,false,"test"));
    std::vector<vec3f> invalid=normal; invalid[1][0]=1e-40f;
    assert(!dbSolver_t::reconfigure_rigid_bodies(bodies,1,invalid,false,"test"));
    for(auto& body:bodies) {
        assert(!body->set_mass_properties(1e-40f,vec3f(1,1,1),false));
        assert(!body->set_mass_properties(1,vec3f(1e-40f,1,1),false));
        assert(body->body()->getInvMass()==1); checkFinite(body->body());
    }
    const float smallest=std::numeric_limits<float>::min();
    assert(dbSolver_t::reconfigure_rigid_bodies(bodies,smallest,
        std::vector<vec3f>(2,vec3f(smallest,0,smallest)),false,"test"));
    for(auto& body:bodies) {
        checkFinite(body->body());
        assert(body->body()->getInvMass()>0 && body->body()->getInvInertiaDiagLocal().y()==0);
    }
    assert(dbSolver_t::reconfigure_rigid_bodies(bodies,1,normal,false,"test"));
    assert(dbSolver_t::set_gravity(vec3f(0,-100,0)));
    assert(!dbSolver_t::reconfigure_rigid_bodies(bodies,1e37f,normal,false,"test"));
    for(auto& body:bodies) assert(body->body()->getInvMass()==1);
    {
        dbRigidBody_t::pointer candidate(new dbRigidBody_t(sphere));
        assert(candidate->set_mass_properties(1e37f,vec3f(1,1,1),false));
        assert(!dbSolver_t::add_rigid_body(candidate,"unsafe gravity"));
        assert(!candidate->body()->getBroadphaseHandle());
        assert(world.getNumCollisionObjects()==2 && dbSolver_t::m_rigid_bodies.size()==2);
    }
    assert(dbSolver_t::set_gravity(vec3f(0,-9.81f,0)));
    assert(dbSolver_t::reconfigure_rigid_bodies(bodies,1e37f,normal,false,"test"));
    assert(!dbSolver_t::set_gravity(vec3f(0,-100,0)));
    assert(world.getGravity().y()==-9.81f && info.m_gravity.y()==-9.81f);
    for(auto& body:bodies) {
        assert(body->body()->getGravity().y()==-9.81f);
        body->body()->clearForces(); body->body()->applyGravity(); checkFinite(body->body());
    }
    assert(dbSolver_t::set_gravity(vec3f(0,-1,0)));
    assert(dbSolver_t::reconfigure_rigid_bodies(bodies,1,normal,false,"test"));
    for(auto& body:bodies) body->body()->clearForces();
    world.stepSimulation(1.0f/60,1,1.0f/60);
    for(auto& body:bodies) { checkFinite(body->body()); solver->remove_rigid_body(body->impl()); }
    dbSolver_t::m_rigid_bodies.clear(); dbSolver_t::m_impl.reset();
    std::puts("Bullet native mass: inverse/gravity admission, atomic properties, dynamic/kinematic filters, batch rollback and stepping passed");
}
