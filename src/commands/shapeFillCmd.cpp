#include "common/utility/DStatusOperation.h"
#include "common/utility/DCommandSyntax.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "shapeFillCmd.h"

#include "common/object/DParticleAttributeUtils.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DShapeFillPlan.h"
#include "DGlobalTex.h"
#include "DRandomGenerator.h"

#include <maya/M3dView.h>
#include <maya/MArgDatabase.h>
#include <maya/MBoundingBox.h>
#include <maya/MDGModifier.h>
#include <maya/MDagPath.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MGlobal.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MPointArray.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>
#include <maya/MVectorArray.h>

#include <cmath>
#include <cstddef>
#include <random>

namespace {

const char* kDensityFlag = "-d";
const char* kDensityLong = "-density";
const char* kThicknessFlag = "-t";
const char* kThicknessLong = "-thickness";
const char* kNoiseFlag = "-n";
const char* kNoiseLong = "-noise";
const char* kCameraFlag = "-c";
const char* kCameraLong = "-camera";
const char* kOnColorFlag = "-oc";
const char* kOnColorLong = "-onColor";
const char* kShadingGroupFlag = "-sn";
const char* kShadingGroupLong = "-sgName";
const char* kOnShadowFlag = "-os";
const char* kOnShadowLong = "-onShadow";
const char* kOnReuseMapFlag = "-orm";
const char* kOnReuseMapLong = "-onReuseMap";

} // namespace

shapeFillCmd::shapeFillCmd()
    : density(1), thickness(0.0), noise(0.0), camName(""), sgName(""),
      onColor(false), onShadow(false), onReuseMap(false),
      deleteOperationsQueued(false), queuedDeleteCount(0U), particlesDeleted(false)
{
}

shapeFillCmd::~shapeFillCmd()
{
}

void* shapeFillCmd::creator()
{
    return ::dommy::maya::createObject("shapeFillCmd", [&]() {
        return new shapeFillCmd;
    });
}

MStatus shapeFillCmd::buildSyntax(MSyntax& syntax)
{
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kDensityFlag, kDensityLong, MSyntax::kLong));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kThicknessFlag, kThicknessLong, MSyntax::kDouble));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kNoiseFlag, kNoiseLong, MSyntax::kDouble));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kCameraFlag, kCameraLong, MSyntax::kString));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kShadingGroupFlag, kShadingGroupLong, MSyntax::kString));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kOnColorFlag, kOnColorLong, MSyntax::kBoolean));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kOnShadowFlag, kOnShadowLong, MSyntax::kBoolean));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kOnReuseMapFlag, kOnReuseMapLong, MSyntax::kBoolean));
    return MS::kSuccess;
}

MSyntax shapeFillCmd::newSyntax() try
{
    return dommy::maya::createCommandSyntax<shapeFillCmd>("shapeFillCmd");
} catch (...) {
    dommy::maya::reportCommandSyntaxException<shapeFillCmd>(
        "shapeFillCmd", std::current_exception());
    return MSyntax();
}

MStatus shapeFillCmd::parseArgs(const MArgList& args)
{
    MSyntax commandSyntax;
    MStatus status = dommy::maya::buildCommandSyntax<shapeFillCmd>(
        commandSyntax, "shapeFillCmd");
    if (!status) return status;
    MArgDatabase arguments(commandSyntax, args, &status);
    if (status != MS::kSuccess) {
        return status;
    }

    struct FlagReader {
        static MStatus read(MArgDatabase& database, const char* flag, int& value) {
            return database.isFlagSet(flag) ? database.getFlagArgument(flag, 0, value)
                                            : MStatus(MS::kSuccess);
        }
        static MStatus read(MArgDatabase& database, const char* flag, double& value) {
            return database.isFlagSet(flag) ? database.getFlagArgument(flag, 0, value)
                                            : MStatus(MS::kSuccess);
        }
        static MStatus read(MArgDatabase& database, const char* flag, bool& value) {
            return database.isFlagSet(flag) ? database.getFlagArgument(flag, 0, value)
                                            : MStatus(MS::kSuccess);
        }
        static MStatus read(MArgDatabase& database, const char* flag, MString& value) {
            return database.isFlagSet(flag) ? database.getFlagArgument(flag, 0, value)
                                            : MStatus(MS::kSuccess);
        }
    };

    status = FlagReader::read(arguments, kDensityFlag, density);
    if (status == MS::kSuccess) status = FlagReader::read(arguments, kThicknessFlag, thickness);
    if (status == MS::kSuccess) status = FlagReader::read(arguments, kNoiseFlag, noise);
    if (status == MS::kSuccess) status = FlagReader::read(arguments, kCameraFlag, camName);
    if (status == MS::kSuccess) status = FlagReader::read(arguments, kShadingGroupFlag, sgName);
    if (status == MS::kSuccess) status = FlagReader::read(arguments, kOnColorFlag, onColor);
    if (status == MS::kSuccess) status = FlagReader::read(arguments, kOnShadowFlag, onShadow);
    if (status == MS::kSuccess) status = FlagReader::read(arguments, kOnReuseMapFlag, onReuseMap);
    if (status != MS::kSuccess) {
        status.perror("shapeFill flag parsing failed");
    }
    return status;
}

MStatus shapeFillCmd::doIt(const MArgList& args)
{
    try {
        bool transactionStarted = false;
        return dommy::maya::runCommandTransaction("shapeFillCmd", [&]() -> MStatus {
            // A command instance owns its existing edits until undo completes.
            if (createdParticleShapes.length() != 0U || deleteOperationsQueued)
                return MS::kFailure;
            MStatus status = parseArgs(args);
            if (status != MS::kSuccess) {
                return status;
            }
            if (density <= 0 || !std::isfinite(thickness) || thickness < 0.0 ||
                !std::isfinite(noise) || noise < 0.0 ||
                (onColor && sgName.length() == 0U)) {
                displayError("density must be positive; thickness/noise must be finite and non-negative; color sampling requires sgName.");
                return MS::kFailure;
            }

            MSelectionList selection;
            status = MGlobal::getActiveSelectionList(selection);
            if (status != MS::kSuccess) {
                return status;
            }
            MItSelectionList iterator(selection, MFn::kMesh, &status);
            if (status != MS::kSuccess || iterator.isDone()) {
                displayError("Select at least one polygon mesh.");
                return MS::kFailure;
            }

            status = createdParticleShapes.clear();
            if (!status) return status;
            deleteOperationsQueued = false;
            queuedDeleteCount = 0U;
            particlesDeleted = false;
            transactionStarted = true;
            MStringArray resultNames;
            while (!iterator.isDone()) {
                MDagPath meshPath;
                MObject component;
                status = iterator.getDagPath(meshPath, component);
                if (status != MS::kSuccess || !meshPath.hasFn(MFn::kMesh)) {
                    return MS::kFailure;
                }

                MFnMesh meshFn(meshPath, &status);
                if (status != MS::kSuccess) {
                    return status;
                }
                MBoundingBox emitRegion = meshFn.boundingBox(&status);
                if (status != MS::kSuccess) {
                    return status;
                }
                const MMatrix worldMatrix = meshPath.inclusiveMatrix(&status);
                if (status != MS::kSuccess) {
                    return status;
                }
                emitRegion.transformUsing(worldMatrix);

                const MPoint minPoint = emitRegion.min();
                const MPoint maxPoint = emitRegion.max();
                dommy::generation::ShapeFillPlan plan;
                if (!dommy::generation::buildShapeFillPlan(
                        maxPoint.x - minPoint.x,
                        maxPoint.y - minPoint.y,
                        maxPoint.z - minPoint.z,
                        density, plan)) {
                    displayError("shapeFill candidate grid is invalid or exceeds the safety budget.");
                    return MS::kFailure;
                }

                const unsigned int randomSeed = std::random_device()();
                dommy::math::RandomGenerator randomGenerator(randomSeed);
                MPointArray particlePoints;
                MFloatArray uCoordinates;
                MFloatArray vCoordinates;
                MFloatPointArray shadingPoints;
                MFloatVectorArray shadingNormals;

                for (std::size_t xIndex = 0; xIndex < plan.sampleCounts[0]; ++xIndex) {
                    for (std::size_t yIndex = 0; yIndex < plan.sampleCounts[1]; ++yIndex) {
                        for (std::size_t zIndex = 0; zIndex < plan.sampleCounts[2]; ++zIndex) {
                            double randomOffset[3] = {0.0, 0.0, 0.0};
                            if (noise > 0.0) {
                                for (int axis = 0; axis < 3; ++axis) {
                                    randomOffset[axis] = randomGenerator.uniform(-noise, noise);
                                }
                            }
                            const MPoint particlePoint(
                                minPoint.x + xIndex * plan.sampleSteps[0] + randomOffset[0],
                                minPoint.y + yIndex * plan.sampleSteps[1] + randomOffset[1],
                                minPoint.z + zIndex * plan.sampleSteps[2] + randomOffset[2]);
                            MPoint closestPoint;
                            MVector normal;
                            status = meshFn.getClosestPointAndNormal(
                                particlePoint, closestPoint, normal, MSpace::kWorld);
                            if (status != MS::kSuccess) {
                                return status;
                            }

                            const double distance = closestPoint.distanceTo(particlePoint);
                            if ((thickness != 0.0 && distance >= thickness) ||
                                (distance >= 0.0001 &&
                                 normal * (closestPoint - particlePoint) <= 0.0)) {
                                continue;
                            }
                            status = particlePoints.append(particlePoint);
                            if (status != MS::kSuccess) {
                                return status;
                            }
                            if (onColor) {
                                float uv[2] = {0.0f, 0.0f};
                                MPoint uvPoint = closestPoint;
                                status = meshFn.getUVAtPoint(uvPoint, uv, MSpace::kWorld);
                                if (status != MS::kSuccess ||
                                    shadingPoints.append(MFloatPoint(
                                        closestPoint.x, closestPoint.y, closestPoint.z)) != MS::kSuccess ||
                                    shadingNormals.append(normal) != MS::kSuccess ||
                                    uCoordinates.append(uv[0]) != MS::kSuccess ||
                                    vCoordinates.append(uv[1]) != MS::kSuccess) {
                                    return MS::kFailure;
                                }
                            }
                        }
                    }
                }

                if (particlePoints.length() == 0U) {
                    displayWarning("shapeFill found no points inside the selected mesh.");
                    status = iterator.next();
                    if (status != MS::kSuccess) {
                        return status;
                    }
                    continue;
                }

                MVectorArray colors;
                if (onColor) {
                    MDagPath cameraPath;
                    if (camName.length() == 0U) {
                        status = M3dView::active3dView().getCamera(cameraPath);
                    } else {
                        MSelectionList cameraSelection;
                        status = cameraSelection.add(camName);
                        if (status == MS::kSuccess) {
                            status = cameraSelection.getDagPath(0U, cameraPath);
                        }
                        if (status == MS::kSuccess && cameraPath.hasFn(MFn::kTransform)) {
                            status = cameraPath.extendToShape();
                        }
                    }
                    if (status != MS::kSuccess || !cameraPath.hasFn(MFn::kCamera)) {
                        displayError("Unable to resolve the shading camera.");
                        return MS::kFailure;
                    }
                    const MMatrix cameraMatrix = cameraPath.inclusiveMatrix(&status);
                    if (status != MS::kSuccess) {
                        return status;
                    }
                    MFloatMatrix cameraFloatMatrix = cameraMatrix.matrix;
                    MFloatVectorArray sampledColors;
                    MFloatVectorArray transparencies;
                    status = sampleShadingNetworkInBatches(
                        sgName, onShadow, onReuseMap, cameraFloatMatrix,
                        shadingPoints, &uCoordinates, &vCoordinates,
                        &shadingNormals, &shadingPoints, nullptr, nullptr, nullptr,
                        sampledColors, transparencies);
                    if (status != MS::kSuccess || sampledColors.length() != particlePoints.length()) {
                        displayError("Unable to sample the requested shading network.");
                        return MS::kFailure;
                    }
                    for (unsigned int color = 0; color < sampledColors.length(); ++color) {
                        status = colors.append(MVector(sampledColors[color]));
                        if (status != MS::kSuccess) {
                            return status;
                        }
                    }
                }

                MSelectionList timeSelection;
                MObject timeObject;
                status = timeSelection.add("time1");
                if (status == MS::kSuccess) {
                    status = timeSelection.getDependNode(0U, timeObject);
                }
                MFnDependencyNode timeFn(timeObject, &status);
                if (status != MS::kSuccess) {
                    return status;
                }

                // Allocate tracking storage before the host can create a scene node.
                const unsigned int shapeIndex = createdParticleShapes.length();
                status = createdParticleShapes.append(MObject::kNullObj);
                if (!status) return status;
                MFnParticleSystem particleFn;
                createdParticleShapes[shapeIndex] = particleFn.create(MObject::kNullObj, &status);
                if (!status || createdParticleShapes[shapeIndex].isNull()) {
                    return status ? MStatus(MS::kFailure) : status;
                }
                status = particleFn.emit(particlePoints);
                if (status != MS::kSuccess) {
                    return status;
                }
                if (onColor) {
                    if (!particleFn.hasRgb()) {
                        status = dommy::ensureParticleRgbAttributes(particleFn);
                    }
                    if (status == MS::kSuccess) {
                        particleFn.setPerParticleAttribute("rgbPP", colors, &status);
                    }
                    if (status != MS::kSuccess) {
                        return status;
                    }
                }
                status = particleFn.saveInitialState();
                if (status != MS::kSuccess) {
                    return status;
                }

                MPlug outTime = timeFn.findPlug("outTime", true, &status);
                if (status != MS::kSuccess) {
                    return status;
                }
                MPlug currentTime = particleFn.findPlug("currentTime", true, &status);
                if (status != MS::kSuccess) {
                    return status;
                }
                MDGModifier connectionModifier;
                status = connectionModifier.connect(outTime, currentTime);
                if (status == MS::kSuccess) {
                    status = connectionModifier.doIt();
                }
                if (status != MS::kSuccess) {
                    return status;
                }
                const MString particleName = particleFn.name(&status);
                if (!status) return status;
                status = resultNames.append(particleName);
                if (!status) return status;

                status = iterator.next();
                if (status != MS::kSuccess) {
                    return status;
                }
            }

            if (createdParticleShapes.length() == 0U) {
                return MS::kFailure;
            }
            setResult(resultNames);
            return MS::kSuccess;
        }, [&]() -> MStatus {
            return transactionStarted ? undoIt() : MStatus(MS::kSuccess);
        });
    } DOMMY_COMMAND_CATCH("shapeFillCmd")
}

MStatus shapeFillCmd::undoIt()
{
    try {
        bool deletionAttempted = false;
        return dommy::maya::runCommandTransaction("shapeFillCmd::undoIt", [&]() -> MStatus {
            if (particlesDeleted) return MS::kFailure;
            if (!deleteOperationsQueued) {
                for (; queuedDeleteCount < createdParticleShapes.length(); ++queuedDeleteCount) {
                    if (createdParticleShapes[queuedDeleteCount].isNull()) continue;
                    MStatus status;
                    MFnDagNode particleFn(createdParticleShapes[queuedDeleteCount], &status);
                    if (!status) return status;
                    const MObject transform = particleFn.parent(0U, &status);
                    if (!status) return status;
                    status = deleteModifier.deleteNode(transform);
                    if (!status) return status;
                }
                deleteOperationsQueued = true;
            }
            deletionAttempted = true;
            const MStatus status = deleteModifier.doIt();
            if (status) particlesDeleted = true;
            return status;
        }, [&]() -> MStatus {
            return deletionAttempted ? deleteModifier.undoIt() : MStatus(MS::kSuccess);
        });
    } DOMMY_COMMAND_CATCH("shapeFillCmd::undoIt")
}

MStatus shapeFillCmd::redoIt()
{
    try {
        bool restorationAttempted = false;
        return dommy::maya::runCommandTransaction("shapeFillCmd::redoIt", [&]() -> MStatus {
            if (!particlesDeleted) return MS::kSuccess;
            restorationAttempted = true;
            const MStatus status = deleteModifier.undoIt();
            if (status) particlesDeleted = false;
            return status;
        }, [&]() -> MStatus {
            return restorationAttempted ? deleteModifier.doIt() : MStatus(MS::kSuccess);
        });
    } DOMMY_COMMAND_CATCH("shapeFillCmd::redoIt")
}
