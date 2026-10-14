//
//  DPnParticl.cpp
//  dommyTool
//
//  Created by yuzo on 2/6/17.
//
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "DPnParticle.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaPluginDataOutput.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/object/DNucleusCollisionUtils.h"
#include "common/object/DNucleusParticleControls.h"
#include "common/object/DNucleusSolverControls.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DEvaluationContext.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DParticleFieldMath.h"

#include "DMayaHeaders.h"
#include "DMayaDebug.h"

#include "DPointDataHeaders.h"
#include "DCurveDataHeaders.h"
#include "DMeshDataHeaders.h"

#include "DStringOp.h"

#include "dommyMath.h"

#include <cmath>
#include <limits>

#if defined(DOMMY_ENABLE_DEBUG_LOGS) && DOMMY_ENABLE_DEBUG_LOGS
const static bool DP_NPARTICLE_DEBUG = true;
#else
const static bool DP_NPARTICLE_DEBUG = false;
#endif

namespace {

bool checkedMayaPointCount(std::size_t rawCount, unsigned int& pointCount) {
    if (rawCount > dommy::generation::kMaxGeneratedPointCount) {
        return false;
    }
    pointCount = static_cast<unsigned int>(rawCount);
    return true;
}

using dommy::maya::ParticleControls;
using dommy::maya::readParticleControls;
using dommy::maya::applyParticleControls;

struct ParticleScalarInput {
    bool apply = false;
    bool missing = false;
    float fallback = 0.0f;
    std::string name;
    DScalarDataList values;
};

MStatus readParticleScalar(const DPData& dpoint, MDataBlock& data,
                          const MObject& nameAttribute, const MObject& valueAttribute,
                          bool initializing, unsigned int count, ParticleScalarInput& result) {
    MStatus status;
    const MDataHandle nameHandle = data.inputValue(nameAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    result.name = nameHandle.asString().asChar();
    result.missing = !dpoint.isExistAttr(result.name);
    result.apply = initializing || !result.missing;
    if (!result.apply) return MS::kSuccess;

    const MDataHandle valueHandle = data.inputValue(valueAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    result.fallback = valueHandle.asFloat();
    if (!std::isfinite(result.fallback) || result.fallback < 0.0f) return MS::kInvalidParameter;
    result.values.assign(count, result.fallback);
    if (!result.missing && !dommy::attribute::convertScalarAttribute(
            dpoint.getAttr(result.name), count, result.values)) return MS::kInvalidParameter;
    if (result.values.size() != count) return MS::kFailure;
    for (float value : result.values) {
        if (!std::isfinite(value) || value < 0.0f) return MS::kInvalidParameter;
    }
    return MS::kSuccess;
}

} // namespace

const MTypeId DPnParticle::typeId(ID_DP_TO_NPARTICLE);

MObject DPnParticle::inDP;

MObject DPnParticle::aStartTime;
MObject DPnParticle::aCurrentTime;
MObject DPnParticle::prevDP;
MObject DPnParticle::nextDP;

MObject DPnParticle::aVelocityAttribute;

MObject DPnParticle::aThickness;
MObject DPnParticle::aThickAttribute;
MObject DPnParticle::aInverseMass;
MObject DPnParticle::aMassAttribute;

MObject DPnParticle::aBounce;
MObject DPnParticle::aFriction;
MObject DPnParticle::aDamping;

MObject DPnParticle::aDisableGravity;
MObject DPnParticle::aLiquidSimulation;
MObject DPnParticle::aIncompressibility;
MObject DPnParticle::aRestDensity;
MObject DPnParticle::aLiquidRadiusScale;
MObject DPnParticle::aViscosity;
MObject DPnParticle::aSurfaceTension;

MObject DPnParticle::aMaxIterations;
MObject DPnParticle::aMaxSelfCollisionIterations;

MObject DPnParticle::aSelfCollisionSoftness;
MObject DPnParticle::aDragAndLift;
MObject DPnParticle::aCollide;
MObject DPnParticle::aSelfCollide;
MObject DPnParticle::aSelfCollideWidth;

// Solver
MObject DPnParticle::aGravity;
MObject DPnParticle::aGravityDirection;
MObject DPnParticle::aAirDensity;
MObject DPnParticle::aWindSpeed;
MObject DPnParticle::aWindDirection;
MObject DPnParticle::aWindNoiseIntensity;
MObject DPnParticle::aEnable;
MObject DPnParticle::aSubsteps;
MObject DPnParticle::aSolverIterations;

// Emitter
MObject DPnParticle::aIsEmit;
MObject DPnParticle::aEmitterList;
MObject DPnParticle::aEmitDP;
MObject DPnParticle::aActiveEmit;

// Force
MObject DPnParticle::aEnableForce;
MObject DPnParticle::aForceAttribute;
MObject DPnParticle::aForceEnvelope;
MObject DPnParticle::aIsRealForce;

// collide
MObject DPnParticle::aInCollideObject;
MObject DPnParticle::aInCollideDM;

MObject DPnParticle::outDP;

DPnParticle::DPnParticle() : m_particleInitialized(false) {}

void* DPnParticle::creator() {
    return ::dommy::maya::createObject("DPnParticle", [&]() {
        return new DPnParticle();
    });
}

#pragma mark particle method

MStatus DPnParticle::ensureNParticle() {
    if (m_particleInitialized)
        return MS::kSuccess;
    const MStatus status = fNObject.createNParticle();
    if (status)
        m_particleInitialized = true;
    return status;
}

MStatus DPnParticle::initializeDPDataParticle(DPData& dpoint, MDataBlock& data) {
    return configureDPDataParticle(dpoint, data, true);
}

MStatus DPnParticle::updateDPDataParticle(DPData& dpoint, MDataBlock& data) {
    // Retry the failed operation's policy, including creation of missing
    // attributes when initialization (rather than a topology update) failed.
    if (m_pendingParticleSetup == PendingParticleSetup::Initialization) {
        return initializeDPDataParticle(dpoint, data);
    }
    unsigned int count = 0U;
    DOMMY_RETURN_IF_FAILURE(fNObject.getNumVertices(count));
    if (m_pendingParticleSetup == PendingParticleSetup::None &&
        count == dpoint.numberOfPoints()) return MS::kSuccess;
    return configureDPDataParticle(dpoint, data, false);
}

MStatus DPnParticle::configureDPDataParticle(
    DPData& dpoint, MDataBlock& data, bool initializing) {
    // Remains pending after any failed read, allocation, native setter or
    // exception. Only a complete application may make the count-only fast path
    // safe again; a native topology change cannot itself establish validity.
    m_pendingParticleSetup = initializing ? PendingParticleSetup::Initialization
                                         : PendingParticleSetup::Update;
    unsigned int count = 0U;
    if (!checkedMayaPointCount(dpoint.numberOfPoints(), count)) {
        MGlobal::displayError("DPnParticle: point count exceeds the generation budget.");
        return MS::kFailure;
    }

    MFloatPointArray positions;
    DOMMY_RETURN_IF_FAILURE(dommy::geometry::copyPointsToMayaArray(
        *dpoint.getPositionPtrConst(), positions));
    if (positions.length() != count) return MS::kFailure;

    MStatus status;
    const MDataHandle velocityHandle = data.inputValue(aVelocityAttribute, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    std::string velocityName = velocityHandle.asString().asChar();
    const bool missingVelocity = !dpoint.isExistAttr(velocityName);
    const bool applyVelocity = initializing || !missingVelocity;
    MFloatPointArray velocities;
    if (applyVelocity) {
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(count,
            [](unsigned int, MFloatPoint& value) -> MStatus {
                value = MFloatPoint(0.0f, 0.0f, 0.0f);
                return MS::kSuccess;
            }, velocities));
        if (!missingVelocity) {
            DOMMY_RETURN_IF_FAILURE(dommy::attribute::copyVector3MayaArray<Imath::V3f>(
                dpoint.getAttr(velocityName), count,
                [](const Imath::V3f& value) { return MFloatPoint(value[0], value[1], value[2], 0.0f); },
                velocities));
        }
        if (velocities.length() != count) return MS::kFailure;
        for (unsigned int index = 0U; index < count; ++index) {
            if (!dommy::maya::isFinitePoint(velocities[index])) return MS::kInvalidParameter;
        }
    } else {
        EchoWarning(name().asChar(), "velocity attribute is not exist. Please check attribute.");
    }

    // Keep the legacy default-attribute order even when attribute names alias.
    // These changes affect only the caller's staged output, not borrowed input.
    if (initializing && missingVelocity) {
        checkValidString(velocityName, "vel");
        dpoint.addAttrWithValues(velocityName, DVec3DataList(count, DVec3Data(0, 0, 0)));
    }

    ParticleScalarInput radius, mass;
    DOMMY_RETURN_IF_FAILURE(readParticleScalar(
        dpoint, data, aThickAttribute, aThickness, initializing, count, radius));
    if (initializing && radius.missing) {
        checkValidString(radius.name, "pscale");
        dpoint.addAttr(radius.name, DPFloatAttr(radius.values, radius.fallback));
    }
    DOMMY_RETURN_IF_FAILURE(readParticleScalar(
        dpoint, data, aMassAttribute, aInverseMass, initializing, count, mass));
    if (initializing && mass.missing) {
        checkValidString(mass.name, "mass");
        dpoint.addAttr(mass.name, DPFloatAttr(mass.values, mass.fallback));
    }
    if (!radius.apply) {
        EchoWarning(name().asChar(), "radius attribute is not exist. Please check attribute.");
    }
    if (!mass.apply) {
        EchoWarning(name().asChar(), "mass attribute is not exist. Please check attribute.");
    }
    ParticleControls controls;
    DOMMY_RETURN_IF_FAILURE(readParticleControls<DPnParticle>(data, controls));

    // All host reads, local allocation/conversion and validation precede the
    // first native mutation. Native setter failures cannot be rolled back here.
    DOMMY_RETURN_IF_FAILURE(fNObject.setTopology(static_cast<int>(count)));
    DOMMY_RETURN_IF_FAILURE(fNObject.setPositions(positions, true));
    if (applyVelocity) DOMMY_RETURN_IF_FAILURE(fNObject.setVelocities(velocities));
    if (radius.apply) {
        if (radius.values.empty()) DOMMY_RETURN_IF_FAILURE(fNObject.setThickness(radius.fallback));
        else DOMMY_RETURN_IF_FAILURE(fNObject.setThickness(radius.values.data()));
    }
    if (mass.apply) {
        if (mass.values.empty()) DOMMY_RETURN_IF_FAILURE(fNObject.setInverseMass(mass.fallback));
        else DOMMY_RETURN_IF_FAILURE(fNObject.setInverseMass(mass.values.data()));
    }
    DOMMY_RETURN_IF_FAILURE(applyParticleControls(fNObject, controls));
    m_pendingParticleSetup = PendingParticleSetup::None;
    return MS::kSuccess;
}


#pragma mark solver method

MStatus DPnParticle::initializeDPSolver(MDataBlock& data) {
    if (DP_NPARTICLE_DEBUG)
        DOMMY_MAYA_DEBUG("DPnParticle", "initializeDPSolver:");

    MStatus status;
    const MDataHandle startTimeHandle = data.inputValue(aStartTime, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    float startTime = 0.0f;
    if (!dommy::numeric::checkedFloatCast(
            startTimeHandle.asTime().as(MTime::kSeconds), startTime)) {
        return MS::kInvalidParameter;
    }

    solver = MnSolver();
    DOMMY_RETURN_IF_FAILURE(solver.createNSolver());
    status = solver.setStartTime(startTime);
    if (status != MS::kSuccess) {
        return status;
    }

    colObjList.clear();
    status = solver.addNObject(&fNObject);
    if (status != MS::kSuccess) {
        return status;
    }
    return syncCollisions(data);
}

MStatus DPnParticle::calculateDPSolver(MDataBlock& data, float solverTime) {
    return dommy::maya::solveNucleus<DPnParticle>(data, solver, solverTime);
}

#pragma mark collision method

MStatus DPnParticle::syncCollisions(MDataBlock& data) {
    return dommy::maya::syncNucleusRigidCollisions(data, aInCollideObject, solver, colObjList);
}

MStatus DPnParticle::compute(const MPlug& plug, MDataBlock& data) {
    if (plug != prevDP && plug != outDP) return MS::kUnknownParameter;
    try {

        MStatus stat = ensureNParticle();
        if (!stat) {
            MGlobal::displayError("DPnParticle: failed to create the Nucleus particle.");
            return stat;
        }

        const MDataHandle startTimeHandle = data.inputValue(aStartTime, &stat);
        DOMMY_RETURN_IF_FAILURE(stat);
        const MTime startTime = startTimeHandle.asTime();
        if (!std::isfinite(startTime.value()) ||
            !std::isfinite(startTime.as(MTime::kSeconds))) {
            return MS::kInvalidParameter;
        }

        if (plug == prevDP) {

            const MDataHandle currentTimeHandle = data.inputValue(aCurrentTime, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MTime currTime = currentTimeHandle.asTime();
            if (!std::isfinite(currTime.value()) ||
                !std::isfinite(currTime.as(MTime::kSeconds))) {
                return MS::kInvalidParameter;
            }

            if (currTime.value() <= startTime.value()) {
                // start frameで初期のDPointの値を取得

                const DPData* inputDPData = nullptr;
                DOMMY_RETURN_IF_FAILURE(getValidInputDP(inDP, data, inputDPData));

                if (!inputDPData) {
                    EchoWarning(name().asChar(), "Failed input data inDP");
                    return MS::kFailure;
                }

                MFnPluginData pluginData;
                DPData* outputDPData = nullptr;
                stat = dommy::maya::createPluginData(pluginData, DPData::id, outputDPData);
                if (!stat || !outputDPData) {
                    EchoWarning(name().asChar(), "Failed to create a Output DPData object");
                    return stat ? MStatus(MS::kFailure) : stat;
                }

                outputDPData->setDPData(*inputDPData);

#pragma mark init
                // Init NObjetct
                stat = initializeDPDataParticle(*outputDPData, data);
                if (stat != MS::kSuccess) {
                    return stat;
                }

                // solver set
                stat = initializeDPSolver(data);
                if (stat != MS::kSuccess) {
                    return stat;
                }

                MDataHandle output = data.outputValue(prevDP, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishPluginDataObject(
                        output, pluginData.object(), *outputDPData));

                return data.setClean(plug);

            } else {
                const DPData* inputDPData = nullptr;
                DOMMY_RETURN_IF_FAILURE(getValidInputDP(nextDP, data, inputDPData));
                if (!inputDPData) {
                    EchoWarning(name().asChar(), "Failed input data nextDP");
                    return MS::kFailure;
                }

                MFnPluginData pluginData;
                DPData* outputDPData = nullptr;
                stat = dommy::maya::createPluginData(pluginData, DPData::id, outputDPData);
                if (!stat || !outputDPData) {
                    return stat ? MStatus(MS::kFailure) : stat;
                }
                outputDPData->setDPData(*inputDPData);

                MDataHandle output = data.outputValue(prevDP, &stat);
                if (!stat)
                    return stat;
                stat = dommy::maya::publishPluginDataObject(
                    output, pluginData.object(), *outputDPData);
                if (!stat)
                    return stat;
                return data.setClean(plug);
            }
        }

        if (plug == outDP) {

#pragma mark get prev dp
            const MDataHandle currentTimeHandle = data.inputValue(aCurrentTime, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const MTime currTime = currentTimeHandle.asTime();
            float solveTime = 0.0f;
            if (!dommy::numeric::checkedFloatCast(currTime.as(MTime::kSeconds), solveTime)) {
                return MS::kInvalidParameter;
            }

            const DPData* nextDPData = nullptr;
            DOMMY_RETURN_IF_FAILURE(getValidInputDP(nextDP, data, nextDPData));

            if (!nextDPData) {
                EchoWarning(name().asChar(), "Failed input data nextDPData");
                return MS::kFailure;
            }

            // PreData
            MFnPluginData pluginData;
            DPData* outputDPData = nullptr;
            stat = dommy::maya::createPluginData(pluginData, DPData::id, outputDPData);
            if (!stat || !outputDPData) {
                EchoWarning(name().asChar(), "Failed to create a Output DPData object");
                return stat ? MStatus(MS::kFailure) : stat;
            }
            outputDPData->setDPData(*nextDPData);

            // check topology
            unsigned int currentTopoNum = 0;
            stat = fNObject.getNumVertices(currentTopoNum);
            if (stat != MS::kSuccess)
                return stat;

            bool changedTopology = m_pendingParticleSetup != PendingParticleSetup::None;

            // check data changed DPointがフローによって変更されたかを確認
            if (currentTopoNum != outputDPData->numberOfPoints()) {

                changedTopology = true;
            }

#pragma mark emit

            const MDataHandle emitEnabledHandle = data.inputValue(aIsEmit, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool emitEnabled = emitEnabledHandle.asBool();
            if (emitEnabled) {

                // check emitter array handle
                MArrayDataHandle arrayHandle = data.inputArrayValue(aEmitterList, &stat);
                DOMMY_RETURN_IF_FAILURE(stat);

                const unsigned int count = arrayHandle.elementCount(&stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);

                for (unsigned int i = 0U; i < count; ++i) {
                    MDataHandle emitter = arrayHandle.inputValue(&stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    MDataHandle emitterInput = emitter.child(aEmitDP);
                    const DPData* emitDPData = nullptr;
                    if (!emitterInput.data().isNull()) {
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::getPluginDataFromHandle(
                            emitterInput, DPData::id, emitDPData));
                        if (!emitDPData->hasValidPointTopology()) return MS::kInvalidParameter;
                    } else {
                        // A stored empty element may be skipped, but a connected
                        // producer returning null is an error. Use the logical
                        // index, not the physical iteration offset, for sparse arrays.
                        const unsigned int logicalIndex = arrayHandle.elementIndex(&stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
                        const MPlug element = MPlug(thisMObject(), aEmitterList)
                            .elementByLogicalIndex(logicalIndex, &stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
                        const MPlug input = element.child(aEmitDP, &stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
                        const bool connected = input.isConnected(&stat);
                        DOMMY_RETURN_IF_FAILURE(stat);
                        if (connected) return MS::kFailure;
                    }

                    if (emitDPData && emitDPData->numberOfPoints()) {

                        const std::string actStr = emitter.child(aActiveEmit).asString().asChar();

                        // create expression
                        DPointSeExpr expr(actStr);

                        // Activation is evaluated once per emitter, not per point.
                        if (expr.usesVar("id") || expr.usesVar("P") ||
                            expr.usesPointDataFunctions()) {
                            MGlobal::displayError("DPnParticle: emitter activation does not "
                                                  "support point variables or functions.");
                            return MS::kInvalidParameter;
                        }
                        DOMMY_RETURN_IF_FAILURE(dommy::maya::setExpressionTime(expr, currTime));

                        if (expr.isValid()) {
                            SeVec3d val;
                            if (!dommy::expression::evaluateChecked(expr, val)) {
                                MGlobal::displayError(
                                    "DPnParticle: emitter activation evaluation failed.");
                                return MS::kInvalidParameter;
                            }

                            if (val[0] > 0) {
                                std::size_t mergedPointCount = 0U;
                                if (!dommy::generation::checkedSum(
                                        outputDPData->numberOfPoints(),
                                        emitDPData->numberOfPoints(),
                                        dommy::generation::kMaxGeneratedPointCount,
                                        mergedPointCount)) {
                                    MGlobal::displayError("DPnParticle: emitted point count "
                                                          "exceeds the generation budget.");
                                    return MS::kFailure;
                                }
                                if (!outputDPData->mergeDPData(*emitDPData)) {
                                    return MS::kFailure;
                                }
                                changedTopology = true;
                            }

                        } else {

                            EchoWarning(name().asChar(),
                                        std::string("Expression is invalid: ") + expr.parseError());
                            return MS::kInvalidParameter;
                        }

                    } // check dpdata and dpoint count

                    if (i + 1U < count) {
                        DOMMY_RETURN_IF_FAILURE(arrayHandle.next());
                    }
                } // check emitter count
            } // check emit

            // The output velocity attribute must be writable as Vec3 even
            // when topology is unchanged and force application is disabled.
            const MDataHandle velocityHandle = data.inputValue(aVelocityAttribute, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            std::string velStr = velocityHandle.asString().asChar();
            checkValidString(velStr, "vel");
            if (outputDPData->isExistAttr(velStr)) {
                const auto attribute = outputDPData->getAttr(velStr);
                if (!attribute || !isVector3DAttrType(attribute->getAttrType())) {
                    return MS::kInvalidParameter;
                }
            }

            if (changedTopology) {
                stat = updateDPDataParticle(*outputDPData, data);
                if (stat != MS::kSuccess) {
                    return stat;
                }
            }

#pragma mark add force
            const MDataHandle forceEnabledHandle = data.inputValue(aEnableForce, &stat);
            DOMMY_RETURN_IF_FAILURE(stat);
            const bool forceEnabled = forceEnabledHandle.asBool();
            if (forceEnabled) {

                unsigned int currentNum;
                stat = fNObject.getNumVertices(currentNum);
                if (stat != MS::kSuccess)
                    return stat;
                if (outputDPData->numberOfPoints() == currentNum) {

                    MFloatPointArray forceArray(currentNum, MFloatPoint(0.0, 0.0, 0.0, 0.0));

                    const MDataHandle forceNameHandle = data.inputValue(aForceAttribute, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const std::string forceStr = forceNameHandle.asString().asChar();

                    MFloatPointArray velArray(currentNum, MFloatPoint(0.0, 0.0, 0.0, 0.0));

                    const MDataHandle forceVelocityHandle = data.inputValue(aVelocityAttribute, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const std::string velStr = forceVelocityHandle.asString().asChar();

                    const MDataHandle forceEnvelopeHandle = data.inputValue(aForceEnvelope, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const float forceEnv = forceEnvelopeHandle.asFloat();
                    const MDataHandle realForceHandle = data.inputValue(aIsRealForce, &stat);
                    DOMMY_RETURN_IF_FAILURE(stat);
                    const bool isRealForce = realForceHandle.asBool();
                    if (!std::isfinite(forceEnv)) {
                        return MS::kInvalidParameter;
                    }

                    if (outputDPData->isExistAttr(forceStr) && outputDPData->isExistAttr(velStr)) {
                        // force
                        DOMMY_RETURN_IF_FAILURE(dommy::attribute::copyVector3MayaArray<Imath::V3f>(
                            outputDPData->getAttr(forceStr), currentNum,
                            [](const Imath::V3f& value) { return MFloatPoint(value[0], value[1], value[2], 0.0f); },
                            forceArray));
                        // velocity
                        DOMMY_RETURN_IF_FAILURE(dommy::attribute::copyVector3MayaArray<Imath::V3f>(
                            outputDPData->getAttr(velStr), currentNum,
                            [](const Imath::V3f& value) { return MFloatPoint(value[0], value[1], value[2], 0.0f); },
                            velArray));
                        if (forceArray.length() != currentNum || velArray.length() != currentNum) {
                            return MS::kFailure;
                        }

                        double dt = 1.0;
                        if (isRealForce) {
                            DOMMY_RETURN_IF_FAILURE(
                                dommy::maya::getEvaluationStepSeconds(data, dt));
                        }
                        for (unsigned int i = 0U; i < velArray.length(); ++i) {

                            // Keep v + force * dt * envelope, without premature
                            // float overflow/underflow in Maya point operators.
                            for (unsigned int axis = 0U; axis < 3U; ++axis) {
                                double impulse = 0.0, velocity = 0.0;
                                if (!dommy::field::checkedMultiplyFieldValue(forceArray[i][axis], dt, impulse) ||
                                    !dommy::field::checkedMultiplyFieldValue(impulse, forceEnv, impulse) ||
                                    !dommy::field::checkedAccumulateFieldForce(velArray[i][axis], impulse, velocity) ||
                                    !dommy::numeric::checkedFloatCast(velocity, velArray[i][axis])) {
                                    return MS::kInvalidParameter;
                                }
                            }
                        }

                        DOMMY_RETURN_IF_FAILURE(fNObject.setVelocities(velArray));
                    }
                }
            }

            // Keep the solver's non-owning collider pointers synchronized with the
            // data handles that are valid for this evaluation.
            stat = syncCollisions(data);
            if (stat != MS::kSuccess) {
                return stat;
            }

#pragma mark calculate particle

            stat = calculateDPSolver(data, solveTime);
            if (stat != MS::kSuccess) {
                return stat;
            }

            // update position and velosity

            MFloatPointArray currentP, currentV;
            stat = fNObject.getPositions(currentP);
            if (stat != MS::kSuccess)
                return stat;
            stat = fNObject.getVelocities(currentV);
            if (stat != MS::kSuccess)
                return stat;
            stat = fNObject.getNumVertices(currentTopoNum);
            if (stat != MS::kSuccess)
                return stat;

            // Validate the complete native result before modifying the staged
            // output. Skipping only one update would mix old and new frames.
            if (currentTopoNum != outputDPData->numberOfPoints() ||
                currentP.length() != currentTopoNum ||
                currentV.length() != currentTopoNum) {
                return MS::kFailure;
            }
            for (unsigned int index = 0U; index < currentTopoNum; ++index) {
                if (!dommy::maya::isFinitePoint(currentP[index]) ||
                    !dommy::maya::isFinitePoint(currentV[index])) {
                    return MS::kInvalidParameter;
                }
            }

            outputDPData->setPosition(currentP);
            if (outputDPData->isExistAttr(velStr))
                outputDPData->setDPAttributeValues(velStr, currentV);

            outputDPData->updateBBox(); // update  bbox

            MDataHandle output = data.outputValue(outDP, &stat);
            CHECK_MSTATUS_AND_RETURN_IT(stat);
            DOMMY_RETURN_IF_FAILURE(
                dommy::maya::publishPluginDataObject(
                    output, pluginData.object(), *outputDPData));

            if (currTime.value() > startTime.value()) {
                MDataHandle preoutput = data.outputValue(prevDP, &stat);
                CHECK_MSTATUS_AND_RETURN_IT(stat);
                DOMMY_RETURN_IF_FAILURE(
                    dommy::maya::publishPluginDataObject(
                        preoutput, pluginData.object(), *outputDPData));
            }

            return data.setClean(plug);
        }

        return MS::kUnknownParameter;
    }
    DOMMY_COMPUTE_CATCH("DPnParticle")
}

MStatus DPnParticle::initialize() try {
    MStatus stat;

    MFnNumericAttribute nAttr;
    MFnTypedAttribute tAttr;

    // ----------------------- INPUTS --------------------------
    inDP = tAttr.create("inDP", "idp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(inDP));

    prevDP = tAttr.create("prevDP", "prdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(prevDP));

    nextDP = tAttr.create("nextDP", "nxdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(nextDP));

    MFnUnitAttribute uniAttr;
    aStartTime = uniAttr.create("StartTime", "stm", MFnUnitAttribute::kTime, 1.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aStartTime));

    aCurrentTime = uniAttr.create("CurrentTime", "ctm", MFnUnitAttribute::kTime, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCurrentTime));

    //

    MFnStringData fnStringData;
    const MObject aVelocityAttributeDefault = fnStringData.create("vel", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aVelocityAttribute = tAttr.create("VelocityAttribute", "vattr", MFnData::kString,
                                      aVelocityAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aVelocityAttribute));

    // attribute
    aThickness = nAttr.create("Thickness", "thick", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.05f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aThickness));

    const MObject aThickAttributeDefault = fnStringData.create("pscale", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aThickAttribute = tAttr.create("ThickAttribute", "tattr", MFnData::kString,
                                   aThickAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aThickAttribute));

    aInverseMass = nAttr.create("InverseMass", "invm", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInverseMass));

    const MObject aMassAttributeDefault = fnStringData.create("mass", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aMassAttribute = tAttr.create("MassAttribute", "msattr", MFnData::kString,
                                  aMassAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMassAttribute));

    aBounce = nAttr.create("Bounce", "bnc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aBounce));

    aFriction = nAttr.create("Friction", "frc", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aFriction));

    aDamping = nAttr.create("Damping", "dmp", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDamping));

    aDisableGravity = nAttr.create("DisableGravity", "disg", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDisableGravity));

    aLiquidSimulation = nAttr.create("LiquidSimulation", "liqsims", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLiquidSimulation));

    aIncompressibility = nAttr.create("Incompressibility", "incmpb", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIncompressibility));

    aRestDensity = nAttr.create("RestDensity", "resden", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(2.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aRestDensity));

    aLiquidRadiusScale = nAttr.create("LiquidRadiusScale", "liqradscl", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aLiquidRadiusScale));

    aViscosity = nAttr.create("Viscosity", "visco", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.1));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aViscosity));

    aSurfaceTension = nAttr.create("SurfaceTension", "srften", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSurfaceTension));

    aCollide = nAttr.create("Collide", "coll", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aCollide));

    aSelfCollide = nAttr.create("SelfCollide", "slfcoll", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelfCollide));

    aSelfCollideWidth = nAttr.create("SelfCollideWidth", "slfcollw", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelfCollideWidth));

    aMaxIterations = nAttr.create("MaxIterations", "maxiter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxIterations));

    aMaxSelfCollisionIterations =
        nAttr.create("MaxSelfCollisionIterations", "maxslfiter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aMaxSelfCollisionIterations));

    aSelfCollisionSoftness =
        nAttr.create("SelfCollisionSoftness", "slfcollsft", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSelfCollisionSoftness));

    aDragAndLift = nAttr.create("DragAndLift", "drgalft", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.010));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aDragAndLift));

#pragma mark solver attribute

    aGravity = nAttr.create("Gravity", "grvy", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(9.8f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGravity));

    aGravityDirection = nAttr.create("GravityDirection", "grvydir", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, -1.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aGravityDirection));

    aAirDensity = nAttr.create("AirDensity", "adsy", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aAirDensity));

    aWindSpeed = nAttr.create("WindSpeed", "wspd", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWindSpeed));

    aWindDirection = nAttr.create("WindDirection", "wdspd", MFnNumericData::k3Float, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f, 1.0f, 0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWindDirection));

    aWindNoiseIntensity = nAttr.create("WindNoiseIntensity", "wit", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(0.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aWindNoiseIntensity));

    aEnable = nAttr.create("Enable", "ebl", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnable));

    aSubsteps = nAttr.create("Substeps", "sbstp", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(3));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSubsteps));

    aSolverIterations = nAttr.create("SolverIterations", "slvmxiter", MFnNumericData::kInt, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(4));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aSolverIterations));

#pragma mark emitter attribute

    aIsEmit = nAttr.create("IsEmit", "isemt", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsEmit));

    aEmitDP = tAttr.create("EmitDP", "emtdp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEmitDP));

    const MObject aActiveEmitDefault = fnStringData.create("1", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aActiveEmit = tAttr.create("ActiveEmit", "actemt", MFnData::kString, aActiveEmitDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aActiveEmit));

    MFnCompoundAttribute cmpAttr;
    aEmitterList = cmpAttr.create("EmitterList", "emtlst", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aActiveEmit));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.addChild(aEmitDP));
    DOMMY_RETURN_IF_FAILURE(cmpAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEmitterList));

#pragma mark force attribute

    aEnableForce = nAttr.create("EnableForce", "eblfrc", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aEnableForce));

    const MObject aForceAttributeDefault = fnStringData.create("force", &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    aForceAttribute = tAttr.create("ForceAttribute", "frcattr", MFnData::kString,
                                   aForceAttributeDefault, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setConnectable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForceAttribute));

    aForceEnvelope = nAttr.create("ForceEnvelope", "frcenv", MFnNumericData::kFloat, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(1.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMin(0.0f));
    DOMMY_RETURN_IF_FAILURE(nAttr.setSoftMax(1.0f));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aForceEnvelope));

    aIsRealForce = nAttr.create("IsRealForce", "isrlfrc", MFnNumericData::kBoolean, 0.0, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(nAttr.setDefault(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aIsRealForce));

#pragma mark collide attribute

    aInCollideObject =
        tAttr.create("InCollideObject", "incolobj", MFnData::kNObject, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(tAttr.setArray(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInCollideObject));

    aInCollideDM = tAttr.create("InCollideDM", "incoldm", DMeshData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(true));
    DOMMY_RETURN_IF_FAILURE(addAttribute(aInCollideDM));

    // create custom data
    outDP = tAttr.create("outDP", "odp", DPData::id, MObject::kNullObj, &stat);
    DOMMY_RETURN_IF_FAILURE(stat);
    DOMMY_RETURN_IF_FAILURE(tAttr.setWritable(false));
    DOMMY_RETURN_IF_FAILURE(tAttr.setStorable(false));
    DOMMY_RETURN_IF_FAILURE(addAttribute(outDP));

    const MObject simulationInputs[] = {
        inDP,
        aStartTime,
        aCurrentTime,
        aVelocityAttribute,
        aThickness,
        aThickAttribute,
        aInverseMass,
        aMassAttribute,
        aBounce,
        aFriction,
        aDamping,
        aDisableGravity,
        aLiquidSimulation,
        aIncompressibility,
        aRestDensity,
        aLiquidRadiusScale,
        aViscosity,
        aSurfaceTension,
        aMaxIterations,
        aMaxSelfCollisionIterations,
        aSelfCollisionSoftness,
        aDragAndLift,
        aCollide,
        aSelfCollide,
        aSelfCollideWidth,
        aGravity,
        aGravityDirection,
        aAirDensity,
        aWindSpeed,
        aWindDirection,
        aWindNoiseIntensity,
        aEnable,
        aSubsteps,
        aSolverIterations,
        aIsEmit,
        aEmitterList,
        aEmitDP,
        aActiveEmit,
        aEnableForce,
        aForceAttribute,
        aForceEnvelope,
        aIsRealForce,
        aInCollideObject,
    };
    for (const MObject& input : simulationInputs) {
        DOMMY_RETURN_IF_FAILURE(attributeAffects(input, prevDP));
        DOMMY_RETURN_IF_FAILURE(attributeAffects(input, outDP));
    }
    DOMMY_RETURN_IF_FAILURE(attributeAffects(nextDP, outDP));

    return MS::kSuccess;
} catch (...) {
    return dommy::maya::reportEntryPointException(
        "DPnParticle", "initialize", std::current_exception());
}
