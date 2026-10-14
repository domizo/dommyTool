#pragma once

#include "DBulletStep.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace dommy {
namespace bullet {

struct SoftBodyForce {
    btSoftBody* body;
    btVector3 force;
};
typedef std::vector<SoftBodyForce> SoftBodyForces;

template <class World>
class BoundedDynamicsWorld : public World {
public:
    using World::World;

    bool canAdvance(float seconds, float fixedSeconds, int substeps = 1) const {
        return simulationStepBudget(this->m_localTime, btScalar(seconds),
                                    btScalar(fixedSeconds), substeps);
    }

    bool stepChecked(float seconds, float fixedSeconds,
                     const SoftBodyForces& forces = SoftBodyForces()) {
        if (forces_ || !canAdvance(seconds, fixedSeconds)) return false;
        std::unordered_set<const btCollisionObject*> members;
        if (!forces.empty()) {
            const btCollisionObjectArray& objects = this->getCollisionObjectArray();
            for (int i = 0; i < objects.size(); ++i) members.insert(objects[i]);
        }
        std::unordered_set<const btSoftBody*> seen;
        for (const SoftBodyForce& item : forces) {
            if (!item.body || !members.count(item.body) ||
                !seen.insert(item.body).second) return false;
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(item.force[axis])) return false;
        }
        // Borrow only for this synchronous call; exceptions must not leave
        // body pointers or Maya-owned data attached to the world.
        struct Reset {
            const SoftBodyForces*& slot;
            ~Reset() { slot = nullptr; }
        } reset{forces_};
        forces_ = &forces;
        World::stepSimulation(seconds, kMaxSimulationSteps, fixedSeconds);
        return true;
    }

protected:
    void internalSingleStepSimulation(btScalar seconds) override {
        if (forces_) {
            // Bullet consumes soft-body force at EVERY native tick. Reapply
            // the sampled force at that cadence; zero-tick calls add nothing.
            // Validate all additions before mutating any node force.
            for (const SoftBodyForce& item : *forces_)
                for (int i = 0; i < item.body->m_nodes.size(); ++i)
                    for (int axis = 0; axis < 3; ++axis)
                        if (!std::isfinite(item.body->m_nodes[i].m_f[axis] + item.force[axis]))
                            throw std::overflow_error("Bullet soft-body force overflow");
            for (const SoftBodyForce& item : *forces_) item.body->addForce(item.force);
        }
        World::internalSingleStepSimulation(seconds);
    }

private:
    const SoftBodyForces* forces_ = nullptr;
};

typedef BoundedDynamicsWorld<btDiscreteDynamicsWorld> DBoundedRigidDynamicsWorld;
typedef BoundedDynamicsWorld<btSoftRigidDynamicsWorld> DBoundedSoftDynamicsWorld;

} // namespace bullet
} // namespace dommy
