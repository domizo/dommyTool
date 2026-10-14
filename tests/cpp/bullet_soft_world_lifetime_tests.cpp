// Real Bullet bodies plus complete production world/factory/registration
// methods. Only the debug drawer and the unrelated solver API are doubled.
#include "DBulletResourceOwnership.h"
#include "Solver/DBulletDynamicsWorld.h"
#include "SoftBody/dbSoftBody_t.h"
#include "Collision/DCollisionMeshInput.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include "BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h"
#include "btBulletDynamicsCommon.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>

class bt_debug_draw : public btIDebugDraw {
public:
    void drawLine(const btVector3&, const btVector3&, const btVector3&) override {}
    void drawContactPoint(const btVector3&, const btVector3&, btScalar, int, const btVector3&) override {}
    void reportErrorWarning(const char*) override {}
    void draw3dText(const btVector3&, const char*) override {}
    void setDebugMode(int) override {}
    int getDebugMode() const override { return 0; }
};
class DB_BulletSolver_t {
public:
    DB_BulletSolver_t();
    ~DB_BulletSolver_t();
    void createWorld();
    void destroyWorld() noexcept;
    dbSoftBody_t::pointer create_soft_body(const std::vector<float>&,
        const std::vector<int>&, const dommy::bullet::SoftBodyParameters&);
    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btConstraintSolver> m_solver;
    DBulletSoftBodyWorldInfoPtr m_worldInfo;
    DBulletSoftBodyWorldReferencePtr m_softBodyWorldReference;
    std::unique_ptr<btIDebugDraw> m_debugDrawer;
    std::unique_ptr<dommy::bullet::DBoundedSoftDynamicsWorld> m_dynamicsWorld;
    std::map<const void*, std::string> m_nameMap;
#include "bullet_soft_world_registration.inc"
};
// Use the facade's existing friend boundary, without exposing a new API.
class dbSolver_t {
public:
    static soft_body_impl_t* impl(const dbSoftBody_t::pointer& body) { return body->impl(); }
    static DB_SoftBody_t& owner(const dbSoftBody_t::pointer& body) {
        return *dynamic_cast<DB_SoftBody_t*>(body->impl());
    }
};
#include "bullet_soft_world_factory.inc"

static void invalidBody(const dbSoftBody_t::pointer& body) {
    assert(body && !body->valid() && !dbSolver_t::owner(body).body());
    assert(body->vertex_count() == 0 && body->get_total_mass() == 0);
    assert(!body->set_mass(20) && !body->set_num_clusters(2));
    assert(!body->set_parameters(dommy::bullet::SoftBodyParameters()));
    body->set_gravity(vec3f(0, -10, 0)); // Former use-after-free write.
    body->apply_central_force(vec3f(1, 2, 3));
    body->set_dynamic_friction_coeff(.3f);
    body->set_collision_margin(.1f);
    body->set_transform(vec3f(1, 2, 3), quatf(1, 0, 0, 0));
    vec3f position(7, 8, 9); quatf rotation(1, 0, 0, 0);
    body->get_transform(position, rotation);
    assert(position[0] == 7 && position[1] == 8 && position[2] == 9);
    mat4x4f transform; body->get_transform(transform);
    assert(body->get_center()[0] == 0 && body->get_average_velocity()[0] == 0);
    std::vector<int> indices(1, 99); std::vector<vec3f> coordinates(1, position);
    body->get_mesh(indices, coordinates);
    assert(indices.empty() && coordinates.empty());
    body->gl_draw(0); // Invalid bodies must not touch even the debug path.
}

int main() {
    const std::vector<float> xyz = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
    const std::vector<int> faces = {0,1,2, 0,1,3};
    const dommy::bullet::SoftBodyParameters parameters(10, .2f, .04f, 1);
    dbSoftBody_t::pointer retained;
    {
        DB_BulletSolver_t world, other;
        retained = world.create_soft_body(xyz, faces, parameters);
        auto unregistered = world.create_soft_body(xyz, faces, parameters);
        assert(retained && retained->valid() && unregistered->valid());
        auto* impl = dbSolver_t::impl(retained);
        assert(!other.add_soft_body(impl, "foreign"));
        assert(world.add_soft_body(impl, "body") && world.add_soft_body(impl, "body"));
        assert(world.m_dynamicsWorld->getSoftBodyArray().size() == 1);
        btBroadphaseProxy* proxy = dbSolver_t::owner(retained).body()->getBroadphaseHandle();
        other.remove_soft_body(impl);
        assert(dbSolver_t::owner(retained).body()->getBroadphaseHandle() == proxy);
        assert(world.m_dynamicsWorld->getSoftBodyArray().size() == 1);
        assert(world.m_dynamicsWorld->stepChecked(1.f/60, 1.f/60));
        world.remove_soft_body(impl);
        assert(retained->valid()); // Membership removal alone is not destruction.
        assert(world.add_soft_body(impl, "body"));

        // Pointer equality of worldInfo is insufficient: two tokens may refer
        // to the same address, and allocation may reuse an old address.
        {
            auto differentGeneration = std::make_shared<DBulletSoftBodyWorldReference>(world.m_worldInfo.get());
            DB_SoftBody_t foreign(*world.m_worldInfo, xyz, faces, parameters, differentGeneration);
            DB_SoftBody_t borrowed(*world.m_worldInfo, xyz, faces, parameters);
            assert(foreign.body() && borrowed.body());
            assert(!world.add_soft_body(&foreign, "different generation"));
            assert(!world.add_soft_body(&borrowed, "untracked"));
            DB_SoftBody_t mismatched(*other.m_worldInfo, xyz, faces, parameters, world.m_softBodyWorldReference);
            assert(!mismatched.body());
        }

        // Keep a strong token copy to verify explicit invalidation, not only
        // weak_ptr expiry. Teardown with a still-registered body is supported.
        auto generation = world.m_softBodyWorldReference;
        world.destroyWorld();
        assert(!generation->get());
        invalidBody(retained); invalidBody(unregistered);
        assert(!world.add_soft_body(impl, "destroyed"));
        assert(!world.create_soft_body(xyz, faces, parameters));
        for (int cycle = 0; cycle < 4; ++cycle) {
            world.createWorld();
            assert(!world.add_soft_body(impl, "stale"));
            auto fresh = world.create_soft_body(xyz, faces, parameters);
            assert(fresh && fresh->valid());
            assert(world.add_soft_body(dbSolver_t::impl(fresh), "fresh"));
            fresh->set_gravity(vec3f(0, -9, 0));
            assert(world.m_dynamicsWorld->stepChecked(1.f/60, 1.f/60));
            world.destroyWorld();
            invalidBody(fresh);
        }
        world.createWorld();
        retained = world.create_soft_body(xyz, faces, parameters);
        assert(retained && retained->valid());
    }
    invalidBody(retained); // Backend destruction, not only explicit reset.
    std::puts("Native soft-body world lifetime: reset, destruction, stale access, cross-world/generation rejection and fresh stepping passed");
}
