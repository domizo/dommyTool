#include "common/utility/DParticleFieldMath.h"

#include <maya/MDoubleArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnField.h>
#include <maya/MItDag.h>
#include <maya/MPointArray.h>
#include <maya/MTime.h>
#include <maya/MVectorArray.h>
#include <maya/MnParticle.h>

MStatus compileParticleFieldApi(MnParticle& particle)
{
    unsigned int count = 0;
    MStatus status = particle.getNumVertices(count);
    if (status != MS::kSuccess || count == 0U) return status;

    MFloatPointArray sourcePositions;
    MFloatPointArray sourceVelocities;
    MFloatArray inverseMasses;
    status = particle.getPositions(sourcePositions);
    if (status != MS::kSuccess) return status;
    status = particle.getVelocities(sourceVelocities);
    if (status != MS::kSuccess) return status;
    status = particle.getInverseMass(inverseMasses);
    if (status != MS::kSuccess) return status;

    MPointArray positions(count);
    MVectorArray velocities(count);
    MDoubleArray masses(count);
    double mass = 0.0;
    if (!dommy::field::fieldMassFromInverseMass(inverseMasses[0], mass)) {
        return MS::kFailure;
    }
    status = positions.set(
        0U, sourcePositions[0].x, sourcePositions[0].y, sourcePositions[0].z);
    if (status != MS::kSuccess) return status;
    status = velocities.set(
        MVector(sourceVelocities[0].x, sourceVelocities[0].y,
                sourceVelocities[0].z),
        0U);
    if (status != MS::kSuccess) return status;
    status = masses.set(mass, 0U);
    if (status != MS::kSuccess) return status;

    MStatus iteratorStatus;
    MItDag iterator(MItDag::kDepthFirst, MFn::kField, &iteratorStatus);
    if (iteratorStatus != MS::kSuccess || iterator.isDone(&iteratorStatus)) {
        return iteratorStatus;
    }
    MFnField field(iterator.currentItem(&iteratorStatus), &iteratorStatus);
    if (iteratorStatus != MS::kSuccess) return iteratorStatus;

    MVectorArray force;
    const double deltaTime =
        MTime(1.0, MTime::uiUnit()).as(MTime::kSeconds);
    status = field.getForceAtPoint(
        positions, velocities, masses, force, deltaTime);
    if (status != MS::kSuccess) return status;

    MFloatPointArray updatedVelocities(count);
    status = updatedVelocities.set(
        0U, sourceVelocities[0].x, sourceVelocities[0].y,
        sourceVelocities[0].z, sourceVelocities[0].w);
    if (status != MS::kSuccess) return status;
    return particle.setVelocities(updatedVelocities);
}
