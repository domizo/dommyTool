//
//  DClusterWeightCmd.cpp
//  dommyTool
//
//  Created by yuzo on 2018/06/07.
//

#include "common/utility/DStatusOperation.h"
#include "common/utility/DCommandSyntax.h"
#include "DClusterWeightCmd.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DGenerationLimits.h"

#include <maya/MArgDatabase.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnWeightGeometryFilter.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItGeometry.h>
#include <maya/MSelectionList.h>
#include <maya/MSyntax.h>

#include <cmath>
#include <cstddef>

namespace {

const char* const kSrcMeshFlag = "-sm";
const char* const kSrcMeshLong = "-sourceMesh";
const char* const kTargetClusterFlag = "-tc";
const char* const kTargetClusterLong = "-targetCluster";
const char* const kCenterFlag = "-c";
const char* const kCenterLong = "-center";
const char* const kDistanceFlag = "-d";
const char* const kDistanceLong = "-distance";

} // namespace

DClusterWeightCmd::DClusterWeightCmd()
    : centerPt(0.0, 0.0, 0.0),
      calcDistance(1.0),
      planReady(false),
      applied(false)
{
}

DClusterWeightCmd::~DClusterWeightCmd() = default;

MStatus DClusterWeightCmd::buildSyntax(MSyntax& syntax)
{
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kSrcMeshFlag, kSrcMeshLong, MSyntax::kString));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(
        kTargetClusterFlag, kTargetClusterLong, MSyntax::kString));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(
        kCenterFlag, kCenterLong, MSyntax::kDouble, MSyntax::kDouble,
        MSyntax::kDouble));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kDistanceFlag, kDistanceLong, MSyntax::kDouble));
    return MS::kSuccess;
}

MSyntax DClusterWeightCmd::newSyntax() try
{
    return dommy::maya::createCommandSyntax<DClusterWeightCmd>("DClusterWeightCmd");
} catch (...) {
    dommy::maya::reportCommandSyntaxException<DClusterWeightCmd>(
        "DClusterWeightCmd", std::current_exception());
    return MSyntax();
}

MStatus DClusterWeightCmd::parseArgs(const MArgList& args)
{
    sourceMeshName.clear();
    targetClusterName.clear();
    centerPt = MPoint(0.0, 0.0, 0.0);
    calcDistance = 1.0;

    MSyntax commandSyntax;
    MStatus status = dommy::maya::buildCommandSyntax<DClusterWeightCmd>(
        commandSyntax, "DClusterWeightCmd");
    if (!status) return status;
    MArgDatabase argData(commandSyntax, args, &status);
    if (!status) {
        return status;
    }

    if (argData.isFlagSet(kSrcMeshFlag)) {
        status = argData.getFlagArgument(kSrcMeshFlag, 0, sourceMeshName);
        if (!status) {
            status.perror(
                "DClusterWeightCmd: sourceMesh flag parsing failed");
            return status;
        }
    }

    if (argData.isFlagSet(kTargetClusterFlag)) {
        status =
            argData.getFlagArgument(kTargetClusterFlag, 0, targetClusterName);
        if (!status) {
            status.perror(
                "DClusterWeightCmd: targetCluster flag parsing failed");
            return status;
        }
    }

    if (argData.isFlagSet(kCenterFlag)) {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        status = argData.getFlagArgument(kCenterFlag, 0, x);
        if (!status) {
            return status;
        }
        status = argData.getFlagArgument(kCenterFlag, 1, y);
        if (!status) {
            return status;
        }
        status = argData.getFlagArgument(kCenterFlag, 2, z);
        if (!status) {
            return status;
        }
        centerPt = MPoint(x, y, z);
    }

    if (argData.isFlagSet(kDistanceFlag)) {
        status = argData.getFlagArgument(kDistanceFlag, 0, calcDistance);
        if (!status) {
            return status;
        }
    }

    return MS::kSuccess;
}

MStatus DClusterWeightCmd::doIt(const MArgList& args)
{
    try {
        planReady = false;
        applied = false;
        clusterHandle = MObjectHandle();
        componentObject = MObject::kNullObj;
        MStatus status = originalWeights.clear();
        if (!status) return status;
        status = targetWeights.clear();
        if (!status) return status;

        status = parseArgs(args);
        if (!status) {
            return status;
        }
        if (sourceMeshName.length() == 0U ||
            targetClusterName.length() == 0U ||
            !std::isfinite(centerPt.x) || !std::isfinite(centerPt.y) ||
            !std::isfinite(centerPt.z) || !std::isfinite(calcDistance) ||
            calcDistance <= 0.0) {
            MGlobal::displayError(
                "DClusterWeightCmd requires source/target names and a finite "
                "positive distance.");
            return MS::kInvalidParameter;
        }

        MSelectionList selection;
        status = selection.add(sourceMeshName);
        if (!status || selection.length() != 1U) {
            MGlobal::displayError(
                "DClusterWeightCmd could not resolve exactly one source mesh.");
            return status ? MStatus(MS::kInvalidParameter) : status;
        }

        MObject sourceComponent;
        status = selection.getDagPath(0, srcMeshPath, sourceComponent);
        if (!status) {
            return status;
        }
        if (srcMeshPath.node().hasFn(MFn::kTransform)) {
            status = srcMeshPath.extendToShape();
            if (!status) {
                return status;
            }
        }
        if (!srcMeshPath.node().hasFn(MFn::kMesh)) {
            MGlobal::displayError(
                "DClusterWeightCmd sourceMesh must resolve to a mesh.");
            return MS::kInvalidParameter;
        }

        status = selection.clear();
        if (!status) return status;
        status = selection.add(targetClusterName);
        if (!status || selection.length() != 1U) {
            MGlobal::displayError(
                "DClusterWeightCmd could not resolve exactly one target "
                "cluster.");
            return status ? MStatus(MS::kInvalidParameter) : status;
        }

        MObject clusterObject;
        status = selection.getDependNode(0, clusterObject);
        if (!status) {
            return status;
        }
        MFnWeightGeometryFilter cluster(clusterObject, &status);
        if (!status) {
            MGlobal::displayError(
                "DClusterWeightCmd targetCluster is not a weight geometry "
                "filter.");
            return status;
        }
        clusterHandle = MObjectHandle(clusterObject);

        status = buildWeightPlan(sourceComponent);
        if (!status) {
            return status;
        }
        planReady = true;
        return redoIt();
    } DOMMY_COMMAND_CATCH("DClusterWeightCmd")
}

MStatus DClusterWeightCmd::buildWeightPlan(MObject sourceComponent)
{
    MStatus status;
    MItGeometry geometry(srcMeshPath, sourceComponent, &status);
    if (!status) {
        return status;
    }

    const int rawCount = geometry.count(&status);
    std::size_t checkedCount = 0U;
    if (!status ||
        !dommy::generation::checkedCount(
            rawCount, dommy::generation::kMaxGeneratedPointCount,
            checkedCount) ||
        checkedCount == 0U) {
        MGlobal::displayError(
            "DClusterWeightCmd source selection is empty or exceeds the "
            "supported component budget.");
        return status ? MStatus(MS::kInvalidParameter) : status;
    }

    MIntArray componentIds;
    status = targetWeights.clear();
    if (!status) return status;
    for (; !geometry.isDone();) {
        if (componentIds.length() >= checkedCount) {
            MGlobal::displayError("DClusterWeightCmd iterator exceeded its checked component count.");
            return MS::kFailure;
        }
        const int componentId = geometry.index(&status);
        if (!status) {
            return status;
        }
        status = componentIds.append(componentId);
        if (!status) {
            return status;
        }

        const MPoint point = geometry.position(MSpace::kWorld, &status);
        if (!status) {
            return status;
        }
        const double distance = point.distanceTo(centerPt);
        if (!std::isfinite(distance)) {
            MGlobal::displayError(
                "DClusterWeightCmd encountered a non-finite mesh position.");
            return MS::kFailure;
        }

        const float weight = distance <= calcDistance
            ? static_cast<float>(1.0 - distance / calcDistance)
            : 0.0F;
        status = targetWeights.append(weight);
        if (!status) {
            return status;
        }
        status = geometry.next();
        if (!status) return status;
    }

    if (componentIds.length() != checkedCount ||
        targetWeights.length() != checkedCount) {
        MGlobal::displayError(
            "DClusterWeightCmd component cardinality changed during "
            "evaluation.");
        return MS::kFailure;
    }

    MFnSingleIndexedComponent componentFn;
    componentObject = componentFn.create(MFn::kMeshVertComponent, &status);
    if (!status) {
        return status;
    }
    status = componentFn.addElements(componentIds);
    if (!status) {
        return status;
    }

    if (!clusterHandle.isValid() || !clusterHandle.isAlive()) {
        return MS::kFailure;
    }
    MFnWeightGeometryFilter cluster(clusterHandle.object(), &status);
    if (!status) {
        return status;
    }
    status = cluster.getWeights(
        srcMeshPath, componentObject, originalWeights);
    if (!status) {
        return status;
    }
    if (originalWeights.length() != targetWeights.length()) {
        MGlobal::displayError(
            "DClusterWeightCmd received an unexpected weight count.");
        return MS::kFailure;
    }

    return MS::kSuccess;
}

MStatus DClusterWeightCmd::applyWeights(MFloatArray& weights)
{
    if (!planReady || !clusterHandle.isValid() || !clusterHandle.isAlive() ||
        componentObject.isNull() ||
        weights.length() != targetWeights.length()) {
        return MS::kFailure;
    }

    MStatus status;
    MFnWeightGeometryFilter cluster(clusterHandle.object(), &status);
    if (!status) {
        return status;
    }
    return cluster.setWeight(srcMeshPath, componentObject, weights);
}

MStatus DClusterWeightCmd::redoIt()
{
    try {
        bool weightsAttempted = false;
        return dommy::maya::runCommandTransaction("DClusterWeightCmd::redoIt", [&]() -> MStatus {
            if (applied) return MS::kSuccess;
            if (!planReady) return MS::kFailure;
            weightsAttempted = true;
            const MStatus status = applyWeights(targetWeights);
            if (status) applied = true;
            return status;
        }, [&]() -> MStatus {
            return weightsAttempted ? applyWeights(originalWeights) : MStatus(MS::kSuccess);
        });
    } DOMMY_COMMAND_CATCH("DClusterWeightCmd::redoIt")
}

MStatus DClusterWeightCmd::undoIt()
{
    try {
        bool weightsAttempted = false;
        return dommy::maya::runCommandTransaction("DClusterWeightCmd::undoIt", [&]() -> MStatus {
            if (!planReady || !applied) return MS::kFailure;
            weightsAttempted = true;
            const MStatus status = applyWeights(originalWeights);
            if (status) applied = false;
            return status;
        }, [&]() -> MStatus {
            return weightsAttempted ? applyWeights(targetWeights) : MStatus(MS::kSuccess);
        });
    } DOMMY_COMMAND_CATCH("DClusterWeightCmd::undoIt")
}
