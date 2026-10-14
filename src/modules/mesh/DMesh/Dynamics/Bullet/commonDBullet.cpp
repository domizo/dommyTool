//
//  commonDBullet.cpp
//  dommyTool
//
//  Created by yuzo on 2017/10/25.
//

#include <stdexcept>
#include <utility>

#include "commonDBullet.h"


void DBulletRigidBodyDeleter::operator()(btRigidBody* body) const noexcept
{
    if (!body) {
        return;
    }

    delete body->getMotionState();
    body->setMotionState(nullptr);
    delete static_cast<customDBData*>(body->getUserPointer());
    body->setUserPointer(nullptr);
    delete body;
}


DBulletRigidBodyPtr MakeBulletRigidBody(
    std::unique_ptr<btMotionState> motionState,
    const btRigidBody::btRigidBodyConstructionInfo& constructionInfo,
    std::unique_ptr<customDBData> userData)
{
    if (!motionState || constructionInfo.m_motionState != motionState.get() ||
        !constructionInfo.m_collisionShape) {
        throw std::invalid_argument(
            "MakeBulletRigidBody requires matching motion state and collision shape.");
    }

    DBulletRigidBodyPtr body(new btRigidBody(constructionInfo));
    body->setUserPointer(userData.get());
    motionState.release();
    userData.release();
    return body;
}


DBulletRigidBodyPtr MakeSimpleKinematicRigidBody(
    btCollisionShape* collisionShape,
    const btVector3& position)
{
    if (!collisionShape) {
        throw std::invalid_argument(
            "Cannot create a kinematic body without a collision shape.");
    }

    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(position);
    std::unique_ptr<btMotionState> motionState(
        new btDefaultMotionState(transform));
    btRigidBody::btRigidBodyConstructionInfo bodyInfo(
        0.0, motionState.get(), collisionShape, btVector3(0, 0, 0));
    DBulletRigidBodyPtr body = MakeBulletRigidBody(
        std::move(motionState), bodyInfo);
    set_kinematic(body.get(), true, 0.0, btVector3(0, 0, 0));
    return body;
}


void AddBulletRigidBody(
    btDynamicsWorld& world,
    std::vector<btRigidBody*>& bodies,
    DBulletRigidBodyPtr body)
{
    if (!body) {
        throw std::invalid_argument("Cannot add a null Bullet rigid body.");
    }

    bodies.reserve(bodies.size() + 1);
    btRigidBody* const rawBody = body.get();
    // Native registration may throw after partially inserting the body.
    try {
        world.addRigidBody(rawBody);
        bodies.push_back(rawBody);
    } catch (...) {
        world.removeRigidBody(rawBody);
        throw;
    }
    body.release();
}


void AddBulletRigidBodyWithShape(
    btDynamicsWorld& world,
    std::vector<btRigidBody*>& bodies,
    std::vector<db_collision_shape_base*>& shapes,
    DBulletRigidBodyPtr body,
    std::unique_ptr<db_collision_shape_base> shape)
{
    if (!body || !shape || body->getCollisionShape() != shape->shape()) {
        throw std::invalid_argument(
            "Rigid body and owned collision shape do not match.");
    }

    shapes.reserve(shapes.size() + 1);
    shapes.push_back(shape.get());
    try {
        AddBulletRigidBody(world, bodies, std::move(body));
    } catch (...) {
        shapes.pop_back();
        throw;
    }
    shape.release();
}


void AddBulletConstraint(
    const DBulletWorldReferencePtr& worldReference,
    std::vector<db_constraint_base*>& constraints,
    DBulletConstraintPtr constraint,
    bool disableCollisionsBetweenLinkedBodies)
{
    if (!worldReference || !worldReference->get() ||
        !constraint || !constraint->constraint()) {
        throw std::invalid_argument("Cannot add an empty Bullet constraint.");
    }

    constraints.reserve(constraints.size() + 1);
    db_constraint_base* const rawConstraint = constraint.get();
    btDynamicsWorld& world = *worldReference->get();
    worldReference->trackConstraint(rawConstraint);
    // Detach a partial native insertion before destroying its owning wrapper.
    try {
        world.addConstraint(
            rawConstraint->constraint(), disableCollisionsBetweenLinkedBodies);
        constraints.push_back(rawConstraint);
    } catch (...) {
        world.removeConstraint(rawConstraint->constraint());
        worldReference->forgetConstraint(rawConstraint);
        throw;
    }
    constraint.release();
}


bool PrepareBulletCollisionShape(db_collision_shape_base& owner,
    const btVector3& scale, float margin, float mass, btVector3& inertia)
{
    btCollisionShape* const shape = owner.shape();
    if (!shape || !dommy::bullet::isSafeInverseOperand(mass) ||
        !dommy::bullet::isSafeCollisionCoordinate(margin) || margin < 0 ||
        (mass > 0 && shape->isNonMoving())) return false;
    for (int axis = 0; axis < 3; ++axis) {
        if (!dommy::bullet::isSafeCollisionCoordinate(scale[axis]) || scale[axis] == 0 ||
            !dommy::bullet::isSafeInverseOperand(std::abs(scale[axis])) ||
            shape->getLocalScaling()[axis] == 0 ||
            !dommy::bullet::isSafeInverseOperand(std::abs(shape->getLocalScaling()[axis]))) return false;
    }
    const btVector3 previousScale = shape->getLocalScaling();
    const btScalar previousMargin = shape->getMargin();
    std::vector<std::pair<btCollisionShape*, btScalar>> children;
    if (shape->isCompound()) {
        auto* compound = static_cast<btCompoundShape*>(shape);
        for (int i = 0; i < compound->getNumChildShapes(); ++i) {
            auto* child = compound->getChildShape(i);
            children.emplace_back(child, child->getMargin());
        }
    }
    const auto restore = [&]() {
        for (const auto& child : children) child.first->setMargin(child.second);
        shape->setMargin(previousMargin);
        shape->setLocalScaling(previousScale);
    };
    try {
        for (const auto& child : children) child.first->setMargin(margin);
        shape->setMargin(margin);
        shape->setLocalScaling(scale);
        btVector3 minimum, maximum;
        shape->getAabb(btTransform::getIdentity(), minimum, maximum);
        for (int axis = 0; !shape->isInfinite() && axis < 3; ++axis) {
            if (!dommy::bullet::isSafeCollisionCoordinate(minimum[axis]) ||
                !dommy::bullet::isSafeCollisionCoordinate(maximum[axis]) ||
                minimum[axis] > maximum[axis]) {
                restore();
                return false;
            }
        }
        btVector3 candidate(0,0,0);
        if (mass > 0) shape->calculateLocalInertia(mass, candidate);
        if (!dommy::bullet::safeBodyMassProperties(mass,
                vec3f(candidate.x(), candidate.y(), candidate.z()))) {
            restore();
            return false;
        }
        inertia = candidate;
        return true;
    } catch (...) {
        restore();
        throw;
    }
}

bool ReplaceBulletCollisionShape(btDynamicsWorld& world, btRigidBody& body,
    db_collision_shape_base*& owner, std::unique_ptr<db_collision_shape_base> replacement,
    const btVector3& scale, float margin, float mass)
{
    if (!dommy::bullet::isSafeInverseOperand(mass) ||
        !owner || !replacement || body.getCollisionShape() != owner->shape() ||
        !body.isInWorld() || !body.getBroadphaseHandle()) return false;
    const bool kinematic = body.isKinematicObject();
    const float effectiveMass = kinematic ? 0.0f : mass;
    const bool becomesDynamic = body.getInvMass() == 0 && effectiveMass > 0;
    const btVector3 gravity = body.getGravity();
    const btVector3 candidateGravity = becomesDynamic &&
        !(body.getFlags() & BT_DISABLE_WORLD_GRAVITY) ? world.getGravity() : gravity;
    btVector3 inertia;
    if (!PrepareBulletCollisionShape(*replacement, scale, margin, effectiveMass, inertia) ||
        !dommy::bullet::safeMassGravity(effectiveMass,
            vec3f(candidateGravity.x(), candidateGravity.y(), candidateGravity.z()))) return false;

    const float previousMass = body.getInvMass() == 0 ? 0.0f : 1.0f / body.getInvMass();
    btVector3 previousInertia(0,0,0);
    for (int axis = 0; axis < 3; ++axis) {
        const btScalar inverse = body.getInvInertiaDiagLocal()[axis];
        if (!dommy::bullet::isSafeInverseOperand(inverse)) return false;
        previousInertia[axis] = inverse == 0 ? 0.0f : 1.0f / inverse;
    }
    if (!dommy::bullet::safeBodyMassProperties(previousMass,
            vec3f(previousInertia.x(), previousInertia.y(), previousInertia.z()))) return false;
    const int previousFlags = body.getCollisionFlags();
    const int activation = body.getActivationState();
    const int group = body.getBroadphaseHandle()->m_collisionFilterGroup;
    const int mask = body.getBroadphaseHandle()->m_collisionFilterMask;
    const bool classificationChanged = (previousMass == 0) != (effectiveMass == 0);
    world.removeRigidBody(&body);
    try {
        body.setCollisionShape(replacement->shape());
        body.setMassProps(effectiveMass, inertia);
        body.updateInertiaTensor();
        if (classificationChanged) world.addRigidBody(&body);
        else world.addRigidBody(&body, group, mask);
        body.setGravity(candidateGravity);
        body.forceActivationState(becomesDynamic ? ACTIVE_TAG : activation);
        world.updateSingleAabb(&body);
    } catch (...) {
        world.removeRigidBody(&body);
        body.setCollisionShape(owner->shape());
        body.setMassProps(previousMass, previousInertia);
        body.setCollisionFlags(previousFlags);
        body.updateInertiaTensor();
        try { world.addRigidBody(&body, group, mask); }
        catch (...) { world.removeRigidBody(&body); }
        body.setGravity(gravity);
        body.forceActivationState(activation);
        // Even if re-registration fails, the body still refers to its old owner.
        // The solver's failed-evaluation guard requires StartTime recovery.
        return false;
    }
    replacement->set_itemIndex(owner->itemIndex());
    delete owner;
    owner = replacement.release();
    return true;
}

void ReleaseBulletCollisionShapes(
    std::vector<db_collision_shape_base*>& collisionShapes)
{
    for (db_collision_shape_base* shape : collisionShapes) {
        delete shape;
    }
    collisionShapes.clear();
}


void ReleaseBulletRigidBodies(std::vector<btRigidBody*>& bodies)
{
    DBulletRigidBodyDeleter deleter;
    for (btRigidBody* body : bodies) {
        deleter(body);
    }
    bodies.clear();
}


void ReleaseBulletConstrains(
    std::vector<db_constraint_base*>& constraints)
{
    for (db_constraint_base* constraint : constraints) {
        delete constraint;
    }
    constraints.clear();
}


void DetachAndReleaseBulletRigidBodies(
    const DBulletWorldReferencePtr& worldReference,
    std::vector<btRigidBody*>& bodies) noexcept
{
    btDynamicsWorld* const world = worldReference ? worldReference->get() : nullptr;
    if (world) {
        for (btRigidBody* body : bodies) {
            worldReference->detachConstraintsForBody(body);
            if (body && body->isInWorld()) {
                world->removeRigidBody(body);
            }
        }
    }
    ReleaseBulletRigidBodies(bodies);
}


void DetachAndReleaseBulletConstraints(
    const DBulletWorldReferencePtr& worldReference,
    std::vector<db_constraint_base*>& constraints) noexcept
{
    if (worldReference) {
        for (db_constraint_base* constraint : constraints) {
            worldReference->detachConstraint(constraint);
        }
    }
    ReleaseBulletConstrains(constraints);
}
