#pragma once

#include <algorithm>
#include <vector>

#include <maya/MArrayDataHandle.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnNObjectData.h>
#include <maya/MnRigid.h>
#include <maya/MnSolver.h>

namespace dommy {
namespace maya {

inline MStatus collectNucleusRigidInputs(
    MDataBlock& data,
    const MObject& attribute,
    std::vector<MnRigid*>& rigidObjects)
{
    MStatus status;
    MArrayDataHandle inputs = data.inputArrayValue(attribute, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    const unsigned int inputCount = inputs.elementCount(&status);
    if (status != MS::kSuccess) {
        return status;
    }
    std::vector<MnRigid*> collected;
    collected.reserve(inputCount);

    for (unsigned int index = 0U; index < inputCount; ++index) {
        MDataHandle input = inputs.inputValue(&status);
        if (status != MS::kSuccess) {
            return status;
        }

        const MObject inputObject = input.data();
        if (!inputObject.isNull()) {
            MFnNObjectData inputData(inputObject, &status);
            if (status != MS::kSuccess) {
                return status;
            }

            MnRigid* rigidObject = nullptr;
            status = inputData.getObjectPtr(rigidObject);
            if (status != MS::kSuccess) {
                return status;
            }
            if (rigidObject == nullptr) {
                return MS::kInvalidParameter;
            }

            if (std::find(collected.begin(), collected.end(), rigidObject) ==
                collected.end()) {
                collected.push_back(rigidObject);
            }
        }

        if (index + 1U < inputCount) {
            status = inputs.next();
            if (status != MS::kSuccess) {
                return status;
            }
        }
    }

    rigidObjects.swap(collected);
    return MS::kSuccess;
}

inline MStatus clearNucleusRigidCollisions(
    MnSolver& solver,
    std::vector<MnRigid*>& registeredColliders)
{
    MStatus status = solver.removeAllCollisions();
    if (status != MS::kSuccess) {
        return status;
    }

    // Pop only after Maya confirms the removal. If an operation fails, the
    // vector still describes every object that may remain registered.
    while (!registeredColliders.empty()) {
        status = solver.removeNObject(registeredColliders.back());
        if (status != MS::kSuccess) {
            return status;
        }
        registeredColliders.pop_back();
    }
    return MS::kSuccess;
}

inline MStatus syncNucleusRigidCollisions(
    MDataBlock& data,
    const MObject& attribute,
    MnSolver& solver,
    std::vector<MnRigid*>& registeredColliders)
{
    std::vector<MnRigid*> inputs;
    MStatus status = collectNucleusRigidInputs(data, attribute, inputs);
    if (status != MS::kSuccess) {
        return status;
    }

    std::vector<MnRigid*> activeColliders;
    activeColliders.reserve(inputs.size());
    for (MnRigid* collider : inputs) {
        unsigned int vertexCount = 0U;
        status = collider->getNumVertices(vertexCount);
        if (status != MS::kSuccess) {
            return status;
        }
        if (vertexCount > 0U) {
            activeColliders.push_back(collider);
        }
    }

    // Pointer insertion must not allocate after Maya has accepted an object.
    // Reserve before removing existing registrations so allocation failure
    // also leaves the previous native state and tracking contents untouched.
    registeredColliders.reserve(activeColliders.size());

    status = clearNucleusRigidCollisions(solver, registeredColliders);
    if (status != MS::kSuccess) return status;

    for (MnRigid* collider : activeColliders) {
        status = solver.addNObject(collider);
        if (status != MS::kSuccess) {
            const MStatus failure = status;
            const MStatus cleanup =
                clearNucleusRigidCollisions(solver, registeredColliders);
            return cleanup == MS::kSuccess ? failure : cleanup;
        }
        registeredColliders.push_back(collider);
    }

    status = solver.makeAllCollide();
    if (status == MS::kSuccess) return status;

    const MStatus failure = status;
    const MStatus cleanup =
        clearNucleusRigidCollisions(solver, registeredColliders);
    return cleanup == MS::kSuccess ? failure : cleanup;
}

} // namespace maya
} // namespace dommy
