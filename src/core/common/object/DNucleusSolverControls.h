#pragma once

#include <cmath>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MnSolver.h>
#include "common/utility/DStatusOperation.h"

namespace dommy {
namespace maya {

// Both Nucleus nodes share the same solver settings. Read and validate every
// input before native mutation; never consume a failed data-block handle.
// Direction components are copied while their source handle is still local.
template <typename Node>
MStatus solveNucleus(MDataBlock& data, MnSolver& solver, float solverTime) {
    MStatus status;
    const MDataHandle gravityHandle = data.inputValue(Node::aGravity, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float gravity = gravityHandle.asFloat();
    const MDataHandle gravityDirectionHandle = data.inputValue(Node::aGravityDirection, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float3& gravitySource = gravityDirectionHandle.asFloat3();
    const float gravityDirection[] = {gravitySource[0], gravitySource[1], gravitySource[2]};
    const MDataHandle airDensityHandle = data.inputValue(Node::aAirDensity, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float airDensity = airDensityHandle.asFloat();
    const MDataHandle windSpeedHandle = data.inputValue(Node::aWindSpeed, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float windSpeed = windSpeedHandle.asFloat();
    const MDataHandle windDirectionHandle = data.inputValue(Node::aWindDirection, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float3& windSource = windDirectionHandle.asFloat3();
    const float windDirection[] = {windSource[0], windSource[1], windSource[2]};
    const MDataHandle windNoiseHandle = data.inputValue(Node::aWindNoiseIntensity, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const float windNoise = windNoiseHandle.asFloat();
    const MDataHandle substepsHandle = data.inputValue(Node::aSubsteps, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int substeps = substepsHandle.asInt();
    const MDataHandle iterationsHandle = data.inputValue(Node::aSolverIterations, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int iterations = iterationsHandle.asInt();
    const MDataHandle enabledHandle = data.inputValue(Node::aEnable, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const bool enabled = enabledHandle.asBool();

    if (!std::isfinite(solverTime) || !std::isfinite(gravity) ||
        !std::isfinite(gravityDirection[0]) || !std::isfinite(gravityDirection[1]) ||
        !std::isfinite(gravityDirection[2]) || !std::isfinite(airDensity) ||
        !std::isfinite(windSpeed) || !std::isfinite(windDirection[0]) ||
        !std::isfinite(windDirection[1]) || !std::isfinite(windDirection[2]) ||
        !std::isfinite(windNoise) || substeps <= 0 || iterations <= 0) {
        return MS::kInvalidParameter;
    }

    DOMMY_RETURN_IF_FAILURE(solver.setGravity(gravity));
    DOMMY_RETURN_IF_FAILURE(solver.setGravityDir(gravityDirection[0], gravityDirection[1], gravityDirection[2]));
    DOMMY_RETURN_IF_FAILURE(solver.setAirDensity(airDensity));
    DOMMY_RETURN_IF_FAILURE(solver.setWindSpeed(windSpeed));
    DOMMY_RETURN_IF_FAILURE(solver.setWindDir(windDirection[0], windDirection[1], windDirection[2]));
    DOMMY_RETURN_IF_FAILURE(solver.setWindNoiseIntensity(windNoise));
    DOMMY_RETURN_IF_FAILURE(solver.setDisabled(!enabled));
    DOMMY_RETURN_IF_FAILURE(solver.setSubsteps(substeps));
    DOMMY_RETURN_IF_FAILURE(solver.setMaxIterations(iterations));
    return solver.solve(solverTime);
}

} // namespace maya
} // namespace dommy
