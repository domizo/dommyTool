#include "Solver/DBulletDynamicsWorld.h"
#include "DB_SoftBody_t.h"
#include "dbSoftBody_t.h"
#include "Collision/DCollisionMeshInput.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <mutex>
#include <set>

using namespace dommy::bullet;
struct BackendInterface {
    virtual ~BackendInterface() {}
    virtual bool can_step_simulation(float, float, int) const = 0;
    virtual bool step_simulation(float, float, const SoftBodyForces&) = 0;
};
struct Backend : BackendInterface {
    DBoundedSoftDynamicsWorld* m_dynamicsWorld = nullptr;
    #include "bullet_step_backend.inc"
};
class dbSolver_t {
public:
    typedef std::vector<std::pair<dbSoftBody_t::pointer, vec3f>> SoftBodyForces;
    static bool can_step_simulation(float, float, int);
    static bool step_simulation(float, float, const SoftBodyForces&);
    static std::recursive_mutex m_mutex;
    static Backend* m_impl;
    static std::set<dbSoftBody_t::pointer> m_soft_bodies;
    static btSoftBody* native(const dbSoftBody_t::pointer& body) {
        return static_cast<DB_SoftBody_t*>(body->impl())->body();
    }
};
std::recursive_mutex dbSolver_t::m_mutex;
Backend* dbSolver_t::m_impl = nullptr;
std::set<dbSoftBody_t::pointer> dbSolver_t::m_soft_bodies;
#include "bullet_step_facade.inc"

template<class Base> struct Probe : Base {
    using Base::Base;
    btScalar remainder() const { return this->m_localTime; }
};
template<class Base> struct Fixture {
    btSoftBodyRigidBodyCollisionConfiguration config;
    btCollisionDispatcher dispatcher;
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    Probe<Base> world;
    int ticks = 0;
    double simulated = 0;
    Fixture() : dispatcher(&config), world(&dispatcher, &broadphase, &solver, &config) {
        world.setGravity(btVector3(0,0,0));
        world.setInternalTickCallback([](btDynamicsWorld* world, btScalar dt) {
            auto* f = static_cast<Fixture*>(world->getWorldUserInfo());
            ++f->ticks; f->simulated += dt;
        }, this);
    }
};

template<class World> void timeChecks() {
    for (int hz : {60, 6000}) {
        Fixture<World> f;
        assert(f.world.canAdvance(1.f/24, 1.f/hz));
        assert(f.world.stepChecked(1.f/24, 1.f/hz));
        assert(hz == 60 ? f.ticks == 2 : f.ticks >= 249);
    }
    Fixture<World> f;
    const float fixed = 1.f/60;
    assert(f.world.stepChecked(fixed * .5f, fixed));
    assert(f.ticks == 0 && f.world.remainder() > 0);
    const btScalar before = f.world.remainder();
    assert(!f.world.stepChecked(3600, 1e-6f));
    assert(!f.world.stepChecked(1.f/24, 1e-6f));
    assert(!f.world.stepChecked(0, 1e-12f)); // Old-rate remainder also counts.
    assert(f.world.remainder() == before && f.ticks == 0);
    assert(f.world.stepChecked(fixed * 1.5f, fixed));
    assert(f.ticks == 2);
    assert(!f.world.canAdvance(.5f, 1e-4f, 3));
}

static float forceRun(int outer) {
    Fixture<DBoundedSoftDynamicsWorld> f;
    f.world.getWorldInfo().m_gravity = btVector3(0,0,0);
    DB_SoftBody_t body(f.world.getWorldInfo(), {0,0,0,1,0,0,0,1,0}, {0,1,2});
    assert(body.body() && body.set_mass(3));
    body.body()->m_cfg.collisions = 0;
    f.world.addSoftBody(body.body());
    const float fixed = 1.f/60;
    for (int i = 0; i < outer; ++i) {
        SoftBodyForces forces = {{body.body(), btVector3(1,0,0)}};
        assert(f.world.stepChecked(6.f * fixed / outer, fixed, forces));
    }
    assert(f.ticks == 6);
    const float velocity = body.get_average_velocity()[0];
    assert(std::abs(velocity - .1f) < .0001f);
    assert(body.body()->m_nodes[0].m_f.x() == 0);
    assert(f.world.stepChecked(fixed, fixed)); // No borrowed/stale force batch.
    assert(std::abs(body.get_average_velocity()[0] - velocity) < .0001f);
    f.world.removeSoftBody(body.body());
    return velocity;
}

int main() {
    timeChecks<DBoundedRigidDynamicsWorld>();
    timeChecks<DBoundedSoftDynamicsWorld>();
    const float a = forceRun(1), b = forceRun(6), c = forceRun(12);
    assert(std::abs(a-b) < .0001f && std::abs(b-c) < .0001f);
    Fixture<DBoundedSoftDynamicsWorld> f;
    f.world.getWorldInfo().m_gravity = btVector3(0,0,0);
    DB_SoftBody_t body(f.world.getWorldInfo(), {0,0,0,1,0,0,0,1,0}, {0,1,2});
    const float dt = 1.f/60;
    SoftBodyForces forces = {{body.body(), btVector3(1,0,0)}};
    assert(!f.world.stepChecked(dt, dt, forces)); // Foreign body.
    f.world.addSoftBody(body.body());
    forces.push_back(forces.front());
    assert(!f.world.stepChecked(dt, dt, forces)); // Duplicate force target.
    forces.pop_back();
    forces[0].force.setX(std::numeric_limits<float>::infinity());
    assert(!f.world.stepChecked(dt, dt, forces));
    forces[0].force.setX(1);
    assert(f.world.remainder() == 0 && f.ticks == 0);
    assert(f.world.stepChecked(dt*.5f, dt, forces));
    assert(body.body()->m_nodes[0].m_f.x() == 0); // No tick, no force accumulation.
    forces[0].force.setX(2);
    assert(f.world.stepChecked(dt*.5f, dt, forces));
    assert(f.ticks == 1);
    body.body()->m_nodes[0].m_f.setX(std::numeric_limits<float>::max());
    forces[0].force.setX(std::numeric_limits<float>::max());
    bool caught = false;
    try { f.world.stepChecked(dt, dt, forces); }
    catch (const std::overflow_error&) { caught = true; }
    assert(caught);
    body.body()->m_nodes[0].m_f.setX(0);
    forces[0].force.setX(0);
    assert(f.world.stepChecked(dt, dt, forces)); // Exception detached the batch.
    f.world.removeSoftBody(body.body());
    Backend backend;
    backend.m_dynamicsWorld = &f.world;
    dbSolver_t::m_impl = &backend;
    assert(!dbSolver_t::can_step_simulation(3600, 1e-6f, 1));
    assert(!dbSolver_t::step_simulation(3600, 1e-6f, {}));
    auto facade = std::make_shared<dbSoftBody_t>(f.world.getWorldInfo(),
        std::vector<float>{0,0,0,1,0,0,0,1,0}, std::vector<int>{0,1,2});
    assert(facade->set_mass(3));
    dbSolver_t::SoftBodyForces sampled = {{facade, vec3f(1,0,0)}};
    assert(!dbSolver_t::step_simulation(dt, dt, sampled));
    dbSolver_t::m_soft_bodies.insert(facade);
    f.world.addSoftBody(dbSolver_t::native(facade));
    assert(dbSolver_t::can_step_simulation(6*dt, dt, 1));
    assert(dbSolver_t::step_simulation(6*dt, dt, sampled));
    assert(std::abs(facade->get_average_velocity()[0] - .1f) < .0001f);
    f.world.removeSoftBody(dbSolver_t::native(facade));
    dbSolver_t::m_soft_bodies.clear();
    dbSolver_t::m_impl = nullptr;
    assert(!dbSolver_t::step_simulation(dt, dt, {}));
    std::printf("Native Bullet time/force checks passed; constant-force velocities: %.7f %.7f %.7f\n",a,b,c);
}
