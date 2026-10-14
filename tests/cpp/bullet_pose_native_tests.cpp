// Real Bullet poses/inertia/constraints with production setters and rollback guards.
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "modules/dynamics/DBullet/Constraint/constraint_impl.h"
#include "modules/dynamics/DBullet/mvl/mathUtils.h"

static long allocationBudget = -1;
void* operator new(std::size_t size) {
    if (allocationBudget == 0) throw std::bad_alloc();
    if (allocationBudget > 0) --allocationBudget;
    void* result = std::malloc(size ? size : 1);
    if (!result) throw std::bad_alloc();
    return result;
}
void operator delete(void* value) noexcept { std::free(value); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* value) noexcept { ::operator delete(value); }

struct rigid_body_impl_t {
    virtual ~rigid_body_impl_t() = default;
    virtual void get_transform(vec3f&, quatf&) const = 0;
    virtual void set_transform(const vec3f&, const quatf&) = 0;
    virtual void set_interpolation_transform(const vec3f&, const quatf&) = 0;
    virtual void update_constraint() = 0;
};
struct DB_Constraint_t {
    virtual ~DB_Constraint_t() = default;
    std::shared_ptr<btTypedConstraint> m_constraint;
    explicit DB_Constraint_t(btTypedConstraint* value) : m_constraint(value) {}
    btTypedConstraint* constraint() { return m_constraint.get(); }
    virtual void update_constraint(rigid_body_impl_t*) = 0;
};
struct DB_RigidBody_t : rigid_body_impl_t {
    using pointer = std::shared_ptr<DB_RigidBody_t>;
    struct ShapeShift {
        btTransform transform;
        ShapeShift() : transform(btQuaternion(btVector3(0, 1, 0), .4F), btVector3(2, 3, 4)) {}
        void getCenterOfMassTransformShift(btTransform& value) {
            value = transform;
        }
    } shift;
    ShapeShift* m_collisionShapeObserver = &shift;
    std::unique_ptr<btRigidBody> m_body;
    btAlignedObjectArray<DB_Constraint_t*> m_constraintRef;
    explicit DB_RigidBody_t(btCollisionShape& shape, float mass = 1)
        : m_body(new btRigidBody(mass, nullptr, &shape,
            mass == 0 ? btVector3(0, 0, 0) : btVector3(1, 2, 3))) {}
    btRigidBody* body() { return m_body.get(); }
#include "bullet_pose_native_body.inc"
};
struct ConstraintBase : DB_Constraint_t, constraint_impl_t {
    using DB_Constraint_t::DB_Constraint_t;
};
#include "bullet_pose_native_constraints.inc"

// Minimal host storage for the actual local rollback structs. Full callback
// failure/retry control flow is covered separately with host/body doubles.
using float3 = float[3];
struct MDataHandle {
    float3* value;
    explicit MDataHandle(float3& v) : value(&v) {}
    const float3& asFloat3() const { return *value; }
    void set3Float(float x, float y, float z) {
        (*value)[0] = x; (*value)[1] = y; (*value)[2] = z;
    }
};
struct MQuaternion {};
namespace MSpace { enum Space { kTransform }; }
struct MFnTransform {
    void setTranslation(const MVector&, MSpace::Space) { assert(false); }
    void setRotation(const MQuaternion&) { assert(false); }
};
using dbRigidBody_t = DB_RigidBody_t;
#include "bullet_pose_native_restore.inc"

static bool same(const btTransform& a, const btTransform& b) {
    return a.getBasis() == b.getBasis() && a.getOrigin() == b.getOrigin();
}

static void assertTensor(const btRigidBody& body) {
    const btMatrix3x3& basis = body.getWorldTransform().getBasis();
    const btMatrix3x3 expected = basis.scaled(body.getInvInertiaDiagLocal()) * basis.transpose();
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            assert(std::fabs(body.getInvInertiaTensorWorld()[row][column] - expected[row][column]) < 1e-6F);
}

static void testPoseInertia() {
    btBoxShape shape(btVector3(1, 2, 3));
    const float q = std::sqrt(.5F);
    for (float mass : {0.F, 1.F}) {
        DB_RigidBody_t body(shape, mass);
        body.body()->setLinearVelocity(btVector3(1, 2, 3));
        body.body()->setAngularVelocity(btVector3(4, 5, 6));
        body.body()->setInterpolationLinearVelocity(btVector3(7, 8, 9));
        body.body()->setInterpolationAngularVelocity(btVector3(10, 11, 12));
        for (const quatf& rotation : {quatf(q, 0, 0, q), quatf(q, q, 0, 0), mvl::qidentity<float>()}) {
            body.set_transform(vec3f(2, 4, 6), rotation);
            const btTransform expected = btTransform(btQuaternion(rotation[1], rotation[2], rotation[3], rotation[0]),
                btVector3(2, 4, 6)) * body.shift.transform;
            assert(same(body.body()->getWorldTransform(), expected));
            assert(same(body.body()->getInterpolationWorldTransform(), expected));
            assertTensor(*body.body());
            assert(body.body()->getLinearVelocity() == btVector3(1, 2, 3));
            assert(body.body()->getAngularVelocity() == btVector3(4, 5, 6));
            assert(body.body()->getInterpolationLinearVelocity() == btVector3(7, 8, 9));
            assert(body.body()->getInterpolationAngularVelocity() == btVector3(10, 11, 12));
            const btMatrix3x3 inertia = body.body()->getInvInertiaTensorWorld();
            body.set_interpolation_transform(vec3f(9, 8, 7), quatf(q, 0, q, 0));
            assert(same(body.body()->getWorldTransform(), expected));
            assert(body.body()->getInvInertiaTensorWorld() == inertia);
        }
    }

    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher(&configuration);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &configuration);
    world.setGravity(btVector3(0, 0, 0));
    DB_RigidBody_t body(shape);
    body.shift.transform.setIdentity();
    world.addRigidBody(body.body());
    body.set_transform(vec3f(0, 0, 0), quatf(q, 0, 0, q));
    assert(std::fabs(body.body()->getInvInertiaTensorWorld()[0][0] - .5F) < 1e-6F);
    body.body()->applyTorque(btVector3(1, 0, 0));
    world.stepSimulation(1.F / 60.F, 0);
    assert(std::fabs(body.body()->getAngularVelocity().x() - 1.F / 120.F) < 1e-6F);
    world.removeRigidBody(body.body());
    std::puts("Native pose inertia: rotated center-of-mass frames, static/dynamic bodies, velocity preservation and first-step torque passed");
}

static void testRollbackRetry() {
    btBoxShape shape(btVector3(1, 2, 3));
    const float q = std::sqrt(.5F);
    const quatf rotation(q, 0, 0, q);
    const btTransform interpolation(btQuaternion(btVector3(1, 0, 0), .2F), btVector3(8, 9, 10));
    for (bool array : {false, true}) {
        for (bool exception : {false, true}) {
            auto owner = std::make_shared<DB_RigidBody_t>(shape);
            owner->set_transform(vec3f(0, 0, 0), mvl::qidentity<float>());
            owner->body()->setInterpolationWorldTransform(interpolation);
            const btTransform world = owner->body()->getWorldTransform();
            float3 position = {1, 2, 3}, angles = {4, 5, 6};
            MDataHandle positionHandle(position), rotationHandle(angles);
            std::vector<MDataHandle> handles{positionHandle, rotationHandle};
            std::vector<std::function<void()> > constraints;
            MFnTransform parent;
            auto attempt = [&](bool accept) {
                if (array) {
                    std::vector<BodyUpdate> updates;
                    updates.emplace_back(owner, owner->body(), positionHandle);
                    RestoreUpdates restore(updates);
                    restore.touched = 1;
                    // Exercise the guard's native-frame contract with a rotation,
                    // although the current array callback only requests translation.
                    owner->set_transform(vec3f(3, 4, 5), rotation);
                    positionHandle.set3Float(3, 4, 5);
                    assertTensor(*owner->body());
                    if (!accept && exception) throw std::runtime_error("pose failure");
                    if (accept) restore.touched = 0;
                } else {
                    RestorePose restore(owner->body(), handles, constraints, parent, MVector(), MQuaternion());
                    restore.bodyTouched = true;
                    owner->set_transform(vec3f(3, 4, 5), rotation);
                    positionHandle.set3Float(3, 4, 5);
                    rotationHandle.set3Float(0, 0, 90);
                    assertTensor(*owner->body());
                    if (!accept && exception) throw std::runtime_error("pose failure");
                    restore.accepted = accept;
                }
            };
            try { attempt(false); } catch (const std::runtime_error&) { assert(exception); }
            assert(same(owner->body()->getWorldTransform(), world));
            assert(same(owner->body()->getInterpolationWorldTransform(), interpolation));
            assertTensor(*owner->body());
            assert(position[0] == 1 && angles[2] == 6);
            for (int retry = 0; retry < 2; ++retry) {
                attempt(true);
                assertTensor(*owner->body());
                assert(position[0] == 3 && (array || angles[2] == 90));
                assert(same(owner->body()->getWorldTransform(), owner->body()->getInterpolationWorldTransform()));
            }
        }
    }
    std::puts("Native pose rollback: production single/array guards restored inertia after scope exit/exception; repeated retries passed");
}

int main() {
    testPoseInertia();
    testRollbackRetry();
    btBoxShape shape(btVector3(1, 1, 1));
    DB_RigidBody_t a(shape), b(shape);
    a.set_transform(vec3f(1, 2, 3), mvl::qidentity<float>());
    b.set_transform(vec3f(4, 5, 6), mvl::qidentity<float>());
    const btTransform frameA(btQuaternion(btVector3(0, 0, 1), .3F), btVector3(1, 2, 3));
    const btTransform frameB(btQuaternion(btVector3(0, 1, 0), -.4F), btVector3(4, 5, 6));
    ProbeNail nail(new btPoint2PointConstraint(*a.body(), *b.body(), btVector3(1,2,3), btVector3(4,5,6)));
    ProbeHinge hinge(new btHingeConstraint(*a.body(), *b.body(), frameA, frameB, false));
    ProbeSlider slider(new btSliderConstraint(*a.body(), *b.body(), frameA, frameB, false));
    ProbeSixdof sixdof(new btGeneric6DofConstraint(*a.body(), *b.body(), frameA, frameB, false));
    assert(!nail.getPivotChanged()); // Base flag is initialized, including before the first edit.
    hinge.setPivotChanged(true);
    for (DB_Constraint_t* value : std::vector<DB_Constraint_t*>{&nail, &hinge, &slider, &sixdof}) {
        a.m_constraintRef.push_back(value);
        b.m_constraintRef.push_back(value);
    }
    auto* point = static_cast<btPoint2PointConstraint*>(nail.constraint());
    auto* hingeNative = static_cast<btHingeConstraint*>(hinge.constraint());
    auto* sliderNative = static_cast<btSliderConstraint*>(slider.constraint());
    auto* sixdofNative = static_cast<btGeneric6DofConstraint*>(sixdof.constraint());
    const btVector3 pivotA = point->getPivotInA(), pivotB = point->getPivotInB();
    auto assertOriginal = [&]() {
        assert(point->getPivotInA() == pivotA && point->getPivotInB() == pivotB);
        assert(same(hingeNative->getAFrame(), frameA) && same(hingeNative->getBFrame(), frameB));
        assert(same(sliderNative->getFrameOffsetA(), frameA) && same(sliderNative->getFrameOffsetB(), frameB));
        assert(same(sixdofNative->getFrameOffsetA(), frameA) && same(sixdofNative->getFrameOffsetB(), frameB));
        assert(!nail.getPivotChanged() && hinge.getPivotChanged() && !slider.getPivotChanged() && !sixdof.getPivotChanged());
    };
    int allocationFailures = 0;
    bool reachedSuccess = false;
    for (long budget = 0; budget < 32; ++budget) {
        allocationBudget = budget;
        try { auto restore = a.capture_constraint_frames(); (void)restore; reachedSuccess = true; }
        catch (const std::bad_alloc&) { ++allocationFailures; }
        allocationBudget = -1;
        assertOriginal();
        if (reachedSuccess) break;
    }
    assert(reachedSuccess && allocationFailures > 0);
    for (DB_RigidBody_t* body : {&a, &b}) {
        const btTransform oldWorld = body->body()->getWorldTransform();
        const btTransform oldInterpolation = body->body()->getInterpolationWorldTransform();
        for (int partial = 1; partial <= 4; ++partial) {
            auto restore = body->capture_constraint_frames();
            std::vector<MDataHandle> handles;
            MFnTransform parent;
            {
                RestorePose rollback(body->body(), handles, restore, parent, MVector(), MQuaternion());
                rollback.bodyTouched = true;
                const float q = std::sqrt(.5F);
                body->set_transform(vec3f(10, 20, 30), quatf(q, 0, 0, q));
                assertTensor(*body->body());
                for (int index = 0; index < partial; ++index)
                    body->m_constraintRef[index]->update_constraint(body);
            }
            assertOriginal();
            assert(same(body->body()->getWorldTransform(), oldWorld));
            assert(same(body->body()->getInterpolationWorldTransform(), oldInterpolation));
            assertTensor(*body->body());
        }
    }
    a.m_constraintRef.push_back(nullptr);
    bool rejected = false;
    try { (void)a.capture_constraint_frames(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected); assertOriginal();
    std::printf("Native rigid pose: four constraint types, eight partial updates and %d capture allocation failures restored exact frames/flags\n", allocationFailures);
}
