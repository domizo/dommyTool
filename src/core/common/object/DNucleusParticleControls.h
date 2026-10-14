#pragma once

#include <cmath>
#include <cstddef>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MnParticle.h>
#include "common/utility/DStatusOperation.h"

namespace dommy {
namespace maya {

// Shared setup controls: read and validate everything before native mutation.
struct ParticleControls {
    float bounce;
    float friction;
    float damping;
    bool disableGravity;
    bool liquidSimulation;
    float incompressibility;
    float restDensity;
    float liquidRadiusScale;
    float viscosity;
    float surfaceTension;
    int maxIterations;
    int maxSelfCollisionIterations;
    float selfCollisionSoftness;
    bool collide;
    bool selfCollide;
    float selfCollideWidth;
};

template <typename Node>
MStatus readParticleControls(MDataBlock& data, ParticleControls& controls) {
    MStatus status;
    const MDataHandle bounceHandle = data.inputValue(Node::aBounce, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.bounce = bounceHandle.asFloat();
    const MDataHandle frictionHandle = data.inputValue(Node::aFriction, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.friction = frictionHandle.asFloat();
    const MDataHandle dampingHandle = data.inputValue(Node::aDamping, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.damping = dampingHandle.asFloat();
    const MDataHandle disableGravityHandle = data.inputValue(Node::aDisableGravity, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.disableGravity = disableGravityHandle.asBool();
    const MDataHandle liquidSimulationHandle = data.inputValue(Node::aLiquidSimulation, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.liquidSimulation = liquidSimulationHandle.asBool();
    const MDataHandle incompressibilityHandle = data.inputValue(Node::aIncompressibility, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.incompressibility = incompressibilityHandle.asFloat();
    const MDataHandle restDensityHandle = data.inputValue(Node::aRestDensity, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.restDensity = restDensityHandle.asFloat();
    const MDataHandle liquidRadiusScaleHandle = data.inputValue(Node::aLiquidRadiusScale, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.liquidRadiusScale = liquidRadiusScaleHandle.asFloat();
    const MDataHandle viscosityHandle = data.inputValue(Node::aViscosity, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.viscosity = viscosityHandle.asFloat();
    const MDataHandle surfaceTensionHandle = data.inputValue(Node::aSurfaceTension, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.surfaceTension = surfaceTensionHandle.asFloat();
    const MDataHandle maxIterationsHandle = data.inputValue(Node::aMaxIterations, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.maxIterations = maxIterationsHandle.asInt();
    const MDataHandle maxSelfCollisionIterationsHandle = data.inputValue(Node::aMaxSelfCollisionIterations, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.maxSelfCollisionIterations = maxSelfCollisionIterationsHandle.asInt();
    const MDataHandle selfCollisionSoftnessHandle = data.inputValue(Node::aSelfCollisionSoftness, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.selfCollisionSoftness = selfCollisionSoftnessHandle.asFloat();
    const MDataHandle collideHandle = data.inputValue(Node::aCollide, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.collide = collideHandle.asBool();
    const MDataHandle selfCollideHandle = data.inputValue(Node::aSelfCollide, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.selfCollide = selfCollideHandle.asBool();
    const MDataHandle selfCollideWidthHandle = data.inputValue(Node::aSelfCollideWidth, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    controls.selfCollideWidth = selfCollideWidthHandle.asFloat();

    const float finiteValues[] = {controls.bounce,
                                  controls.friction,
                                  controls.damping,
                                  controls.incompressibility,
                                  controls.restDensity,
                                  controls.liquidRadiusScale,
                                  controls.viscosity,
                                  controls.surfaceTension,
                                  controls.selfCollisionSoftness,
                                  controls.selfCollideWidth};
    for (std::size_t index = 0U; index < sizeof(finiteValues) / sizeof(finiteValues[0]); ++index) {
        if (!std::isfinite(static_cast<double>(finiteValues[index]))) {
            return MS::kInvalidParameter;
        }
    }
    if (controls.maxIterations < 0 || controls.maxSelfCollisionIterations < 0) {
        return MS::kInvalidParameter;
    }

    return MS::kSuccess;
}

inline MStatus applyParticleControls(MnParticle& particle, const ParticleControls& controls) {
    DOMMY_RETURN_IF_FAILURE(particle.setBounce(controls.bounce));
    DOMMY_RETURN_IF_FAILURE(particle.setFriction(controls.friction));
    DOMMY_RETURN_IF_FAILURE(particle.setDamping(controls.damping));
    DOMMY_RETURN_IF_FAILURE(particle.setDisableGravity(controls.disableGravity));
    DOMMY_RETURN_IF_FAILURE(particle.setLiquidSimulation(controls.liquidSimulation));
    DOMMY_RETURN_IF_FAILURE(particle.setIncompressibility(controls.incompressibility));
    DOMMY_RETURN_IF_FAILURE(particle.setRestDensity(controls.restDensity));
    DOMMY_RETURN_IF_FAILURE(particle.setLiquidRadiusScale(controls.liquidRadiusScale));
    DOMMY_RETURN_IF_FAILURE(particle.setViscosity(controls.viscosity));
    DOMMY_RETURN_IF_FAILURE(particle.setSurfaceTension(controls.surfaceTension));
    DOMMY_RETURN_IF_FAILURE(particle.setMaxIterations(controls.maxIterations));
    DOMMY_RETURN_IF_FAILURE(particle.setMaxSelfCollisionIterations(controls.maxSelfCollisionIterations));
    DOMMY_RETURN_IF_FAILURE(particle.setSelfCollisionSoftness(controls.selfCollisionSoftness));
    DOMMY_RETURN_IF_FAILURE(particle.setCollide(controls.collide));
    DOMMY_RETURN_IF_FAILURE(particle.setSelfCollide(controls.selfCollide));
    return particle.setSelfCollideWidth(controls.selfCollideWidth);
}

} // namespace maya
} // namespace dommy
