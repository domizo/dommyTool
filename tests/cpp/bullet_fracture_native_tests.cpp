// Complete experimental fracture-world implementation with real Bullet 2.88.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "DBFractureDynamicsWorld.h"
#include "dbtFractureBody.h"

struct Fixture {
    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher{&configuration};
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    DBFractureDynamicsWorld world{&dispatcher, &broadphase, &solver, &configuration};
    Fixture() { world.setGravity(btVector3(0, 0, 0)); }
    void clear() {
        while (world.getNumCollisionObjects())
            world.destroyRigidBody(btRigidBody::upcast(world.getCollisionObjectArray()[0]));
    }
    ~Fixture() { clear(); }
};

static bool near(btScalar a, btScalar b) {
    return btFabs(a - b) < btScalar(5e-4) * (1 + btFabs(a) + btFabs(b));
}
static void equalVector(const btVector3& a, const btVector3& b) {
    for (int axis = 0; axis < 3; ++axis) assert(near(a[axis], b[axis]));
}
static void equalMatrix(const btMatrix3x3& a, const btMatrix3x3& b) {
    for (int row = 0; row < 3; ++row) equalVector(a[row], b[row]);
}
static btMatrix3x3 inertiaWorld(const btRigidBody& body) {
    const btMatrix3x3& rotation = body.getWorldTransform().getBasis();
    return rotation.scaled(body.getLocalInertia()) * rotation.transpose();
}
static btMatrix3x3 pointInertia(btScalar mass, const btVector3& r) {
    const btScalar squared = r.length2();
    btMatrix3x3 result;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            result[row][column] = mass * ((row == column ? squared : 0) - r[row] * r[column]);
    return result;
}
static btVector3 linearMomentum(const btRigidBody& body) {
    return body.getLinearVelocity() / body.getInvMass();
}
static btVector3 angularMomentum(const btRigidBody& body) {
    return inertiaWorld(body) * body.getAngularVelocity() +
        body.getCenterOfMassPosition().cross(linearMomentum(body));
}
static void assertPose(const btRigidBody& body) {
    const btMatrix3x3& rotation = body.getWorldTransform().getBasis();
    equalMatrix(body.getInvInertiaTensorWorld(),
        rotation.scaled(body.getInvInertiaDiagLocal()) * rotation.transpose());
    equalMatrix(body.getInterpolationWorldTransform().getBasis(), rotation);
    equalVector(body.getInterpolationWorldTransform().getOrigin(), body.getCenterOfMassPosition());
    equalVector(body.getInterpolationLinearVelocity(), body.getLinearVelocity());
    equalVector(body.getInterpolationAngularVelocity(), body.getAngularVelocity());
}

static void testStepDispatch(bool fracture) {
    Fixture fixture;
    btBoxShape shape(btVector3(1, 1, 1));
    btVector3 inertia;shape.calculateLocalInertia(1, inertia);
    btScalar mass=1;
    auto* first=new dbtFractureBody(1, nullptr, &shape, inertia, &mass, 1, &fixture.world);
    auto* second=new btRigidBody(1, nullptr, &shape, inertia);
    btTransform pose;pose.setIdentity();pose.setOrigin(btVector3(1.9, 0, 0));
    second->setCenterOfMassTransform(pose);
    fixture.world.addRigidBody(first);fixture.world.addRigidBody(second);
    fixture.world.setFractureMode(fracture);
    assert(fixture.world.stepSimulation(btScalar(1.0/24), 0)==1);
    // No caller-side glueCallback: solveConstraints must honor the mode.
    assert(fixture.world.getNumCollisionObjects()==(fracture?2:1));
    fixture.clear();
}

static void testGlue(float secondMass, bool moving) {
    Fixture fixture;
    btBoxShape shape(btVector3(2, 1, .5));
    btVector3 inertia; shape.calculateLocalInertia(1, inertia);
    btScalar mass = 1;
    auto* first = new dbtFractureBody(1, nullptr, &shape, inertia, &mass, 1, &fixture.world);
    shape.calculateLocalInertia(secondMass, inertia);
    auto* second = new btRigidBody(secondMass, nullptr, &shape, inertia);
    const btQuaternion rotation(btVector3(0, 0, 1), SIMD_HALF_PI);
    first->setCenterOfMassTransform(btTransform(rotation, btVector3(0, 0, 0)));
    second->setCenterOfMassTransform(btTransform(rotation, btVector3(0, 3.9, 0)));
    first->setAngularVelocity(btVector3(.3, -.2, 1));
    second->setAngularVelocity(btVector3(-.1, .5, 1));
    if (moving) {
        first->setLinearVelocity(btVector3(1, -2, 3));
        second->setLinearVelocity(btVector3(-2, 1, .5));
    }
    const btScalar totalMass = 1 + secondMass;
    const btVector3 center = (first->getCenterOfMassPosition() +
        secondMass * second->getCenterOfMassPosition()) / totalMass;
    const btMatrix3x3 expectedInertia = inertiaWorld(*first) + inertiaWorld(*second) +
        pointInertia(1, first->getCenterOfMassPosition() - center) +
        pointInertia(secondMass, second->getCenterOfMassPosition() - center);
    const btVector3 p = linearMomentum(*first) + linearMomentum(*second);
    const btVector3 l = angularMomentum(*first) + angularMomentum(*second);
    fixture.world.addRigidBody(first); fixture.world.addRigidBody(second);
    fixture.world.performDiscreteCollisionDetection();
    fixture.world.glueCallback();
    assert(fixture.world.getNumCollisionObjects() == 1);
    auto* merged = btRigidBody::upcast(fixture.world.getCollisionObjectArray()[0]);
    assert(near(1 / merged->getInvMass(), totalMass));
    equalVector(merged->getCenterOfMassPosition(), center);
    equalMatrix(inertiaWorld(*merged), expectedInertia);
    equalVector(linearMomentum(*merged), p);
    equalVector(angularMomentum(*merged), l);
    assertPose(*merged);
    fixture.clear();
}

static void testRepeatedGlue(bool reverse) {
    Fixture fixture;
    btBoxShape shape(btVector3(1, 1, 1));
    btCompoundShape compounds[2];
    btScalar masses[2][2] = {{1, 9}, {2, 3}};
    std::vector<std::unique_ptr<btCompoundShape>> shifted;
    btRigidBody* bodies[2];
    for (int i = 0; i < 2; ++i) {
        for (int c = 0; c < 2; ++c) {
            btTransform child; child.setIdentity(); child.setOrigin(btVector3(btScalar(c * 1.9), 0, 0));
            compounds[i].addChildShape(child, &shape);
        }
        btTransform shift; btVector3 inertia;
        shifted.emplace_back(dbtFractureBody::shiftTransform(&compounds[i], masses[i], shift, inertia));
        auto* body = new dbtFractureBody(masses[i][0] + masses[i][1], nullptr,
            shifted.back().get(), inertia, masses[i], 2, &fixture.world);
        btTransform parent; parent.setIdentity(); parent.setOrigin(btVector3(0, btScalar(i * 1.9), 0));
        body->setCenterOfMassTransform(parent * shift);
        body->setAngularVelocity(btVector3(.2, .3, 1));
        body->setLinearVelocity(btVector3(btScalar(i), 2, 3));
        bodies[i] = body;
    }
    const btVector3 center = (bodies[0]->getCenterOfMassPosition() * 10 +
        bodies[1]->getCenterOfMassPosition() * 5) / 15;
    const btVector3 p = linearMomentum(*bodies[0]) + linearMomentum(*bodies[1]);
    const btVector3 l = angularMomentum(*bodies[0]) + angularMomentum(*bodies[1]);
    fixture.world.addRigidBody(bodies[reverse ? 1 : 0]);
    fixture.world.addRigidBody(bodies[reverse ? 0 : 1]);
    fixture.world.performDiscreteCollisionDetection(); fixture.world.glueCallback();
    assert(fixture.world.getNumCollisionObjects() == 1);
    auto* merged = static_cast<dbtFractureBody*>(fixture.world.getCollisionObjectArray()[0]);
    std::vector<float> actual;
    for (int i = 0; i < merged->m_masses.size(); ++i) actual.push_back(merged->m_masses[i]);
    std::sort(actual.begin(), actual.end());
    assert((actual == std::vector<float>{1, 2, 3, 9}));
    equalVector(merged->getCenterOfMassPosition(), center);
    equalVector(linearMomentum(*merged), p); equalVector(angularMomentum(*merged), l);
    assertPose(*merged);

    // Merge the generated body again, exercising the world-owned compound's
    // lifetime and retained child masses, not just two original compounds.
    btVector3 thirdInertia; shape.calculateLocalInertia(4, thirdInertia);
    auto* third = new btRigidBody(4, nullptr, &shape, thirdInertia);
    third->setCenterOfMassTransform(btTransform(btQuaternion::getIdentity(), btVector3(0, 3.8, 0)));
    third->setLinearVelocity(btVector3(3, 2, 1)); third->setAngularVelocity(btVector3(1, .5, .2));
    const btVector3 finalCenter = (merged->getCenterOfMassPosition() * 15 +
        third->getCenterOfMassPosition() * 4) / 19;
    const btVector3 finalP = linearMomentum(*merged) + linearMomentum(*third);
    const btVector3 finalL = angularMomentum(*merged) + angularMomentum(*third);
    fixture.world.addRigidBody(third);
    fixture.world.performDiscreteCollisionDetection(); fixture.world.glueCallback();
    assert(fixture.world.getNumCollisionObjects() == 1);
    merged = static_cast<dbtFractureBody*>(fixture.world.getCollisionObjectArray()[0]);
    actual.clear();
    for (int i = 0; i < merged->m_masses.size(); ++i) actual.push_back(merged->m_masses[i]);
    std::sort(actual.begin(), actual.end());
    assert((actual == std::vector<float>{1, 2, 3, 4, 9}));
    equalVector(merged->getCenterOfMassPosition(), finalCenter);
    equalVector(linearMomentum(*merged), finalP); equalVector(angularMomentum(*merged), finalL);
    assertPose(*merged); fixture.clear();
}

static void testSplit() {
    Fixture fixture;
    btBoxShape shape(btVector3(.5, .8, 1));
    btCompoundShape original;
    for (float x : {-2.F, 2.F}) {
        btTransform child; child.setIdentity(); child.setOrigin(btVector3(x, 0, 0));
        original.addChildShape(child, &shape);
    }
    btScalar masses[] = {1, 3}; btTransform shift; btVector3 inertia;
    std::unique_ptr<btCompoundShape> shifted(dbtFractureBody::shiftTransform(&original, masses, shift, inertia));
    auto* parent = new dbtFractureBody(4, nullptr, shifted.get(), inertia, masses, 2, &fixture.world);
    const btTransform pose(btQuaternion(btVector3(0, 0, 1), .6F), btVector3(2, 3, 4));
    parent->setCenterOfMassTransform(pose * shift);
    parent->setLinearVelocity(btVector3(1, 2, 3)); parent->setAngularVelocity(btVector3(.2, .3, 1));
    btConnection connection;
    connection.m_childIndex0 = 0; connection.m_childIndex1 = 1;
    connection.m_childShape0 = connection.m_childShape1 = &shape; connection.m_strength = 1;
    parent->m_connections.push_back(connection);
    auto* obstacle = new btRigidBody(0, nullptr, &shape);
    obstacle->setCenterOfMassTransform(pose * original.getChildTransform(0));
    fixture.world.addRigidBody(parent); fixture.world.addRigidBody(obstacle);
    fixture.world.performDiscreteCollisionDetection();
    int contacts = 0;
    for (int i = 0; i < fixture.dispatcher.getNumManifolds(); ++i) {
        auto* manifold = fixture.dispatcher.getManifoldByIndexInternal(i);
        for (int p = 0; p < manifold->getNumContacts(); ++p) {
            manifold->getContactPoint(p).m_appliedImpulse = 100; ++contacts;
        }
    }
    assert(contacts > 0);
    const btVector3 center = parent->getCenterOfMassPosition();
    const btVector3 v = parent->getLinearVelocity(), w = parent->getAngularVelocity();
    const btVector3 p = linearMomentum(*parent), l = angularMomentum(*parent);
    fixture.world.fractureCallback();
    assert(fixture.world.getNumCollisionObjects() == 3);
    btVector3 afterP(0, 0, 0), afterL(0, 0, 0);
    for (int i = 0; i < fixture.world.getNumCollisionObjects(); ++i) {
        auto* body = btRigidBody::upcast(fixture.world.getCollisionObjectArray()[i]);
        if (body->getInvMass() == 0) continue;
        equalVector(body->getLinearVelocity(), v + w.cross(body->getCenterOfMassPosition() - center));
        equalVector(body->getAngularVelocity(), w); assertPose(*body);
        afterP += linearMomentum(*body); afterL += angularMomentum(*body);
    }
    equalVector(afterP, p); equalVector(afterL, l); fixture.clear();
}

static void testInvalidMasses() {
    btBoxShape shape(btVector3(1, 1, 1)); btCompoundShape compound;
    compound.addChildShape(btTransform::getIdentity(), &shape);
    for (float value : {0.F, -1.F, 1e-40F, std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}) {
        btTransform shift; btVector3 inertia; bool rejected = false;
        try {
            std::unique_ptr<btCompoundShape> result(dbtFractureBody::shiftTransform(&compound, &value, shift, inertia));
        } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && compound.getNumChildShapes() == 1);
    }
}

static void testRejectedGlueRetry() {
    for (int failure = 0; failure < 3; ++failure) {
        Fixture fixture;
        btBoxShape shape(btVector3(1, 1, 1)); btVector3 inertia;
        shape.calculateLocalInertia(1, inertia);
        btScalar mass = failure == 0 ? 2 : 1;
        auto* first = new dbtFractureBody(1, nullptr, &shape, inertia, &mass, 1, &fixture.world);
        auto* second = new btRigidBody(1, nullptr, &shape, inertia);
        second->setCenterOfMassTransform(btTransform(btQuaternion::getIdentity(), btVector3(1.9, 0, 0)));
        if (failure == 1) second->setAngularVelocity(btVector3(std::numeric_limits<float>::quiet_NaN(), 0, 0));
        if (failure == 2) second->setLinearVelocity(btVector3(std::numeric_limits<float>::infinity(), 0, 0));
        fixture.world.addRigidBody(first); fixture.world.addRigidBody(second);
        fixture.world.performDiscreteCollisionDetection();
        for (int retry = 0; retry < 2; ++retry) {
            bool rejected = false;
            try { fixture.world.glueCallback(); } catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected && fixture.world.getNumCollisionObjects() == 2);
            assert(first->isInWorld() && second->isInWorld());
        }
        first->m_masses[0] = 1;
        second->setLinearVelocity(btVector3(0, 0, 0)); second->setAngularVelocity(btVector3(0, 0, 0));
        fixture.world.glueCallback();
        assert(fixture.world.getNumCollisionObjects() == 1);
        fixture.clear();
    }
}

int main() {
    testStepDispatch(false);testStepDispatch(true);
    for (float mass : {1.F, 9.F}) for (bool moving : {false, true}) testGlue(mass, moving);
    testRepeatedGlue(false); testRepeatedGlue(true); testSplit(); testInvalidMasses(); testRejectedGlueRetry();
    std::puts("Native fracture: weighted COM/principal inertia, spin/orbital momentum, child masses in both orders, split motion, native frames, invalid masses and rejected-glue retries passed");
}
