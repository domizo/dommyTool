#include "spaceDuplicaterCmd.h"
#include "DLegacyDuplicaterTypeIds.h"
#include "common/utility/DCommandGuard.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MDagPath.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSet.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>

MStatus spaceDuplicaterCmd::doIt(const MArgList& args)
{
    try {
        bool sceneApplied = false;
        return dommy::maya::runCommandTransaction("spaceDuplicaterCmd", [&]() -> MStatus {
            (void)args;
            MStatus status;
            MSelectionList selection;
            status = MGlobal::getActiveSelectionList(selection);
            if (status != MS::kSuccess) {
                return status;
            }
            if (selection.length() < 1U) {
                displayWarning("Select a polygon mesh to duplicate.");
                return MS::kFailure;
            }

            MDagPath duplicateMeshPath;
            MObject component;
            status = selection.getDagPath(0U, duplicateMeshPath, component);
            if (status != MS::kSuccess || !duplicateMeshPath.hasFn(MFn::kMesh)) {
                displayWarning("The first selection must be a polygon mesh.");
                return MS::kFailure;
            }
            MFnMesh duplicateMeshFn(duplicateMeshPath, &status);
            if (status != MS::kSuccess) {
                return status;
            }

            const MObject transformNode = dagMod.createNode(
                "transform", MObject::kNullObj, &status);
            if (status != MS::kSuccess) {
                return status;
            }
            status = dagMod.renameNode(transformNode, "spaceD");
            if (status != MS::kSuccess) {
                return status;
            }
            outputMeshNode = dagMod.createNode("mesh", transformNode, &status);
            if (status != MS::kSuccess) {
                return status;
            }
            status = dagMod.renameNode(outputMeshNode, "spaceDShape");
            if (status != MS::kSuccess) {
                return status;
            }
            const MObject duplicaterNode =
                dagMod.MDGModifier::createNode(dommy::legacy::kSpaceDuplicaterTypeId, &status);
            if (status != MS::kSuccess) {
                return status;
            }

            MFnDependencyNode outputMeshFn(outputMeshNode, &status);
            if (status != MS::kSuccess) {
                return status;
            }
            MFnDependencyNode duplicaterFn(duplicaterNode, &status);
            if (status != MS::kSuccess) {
                return status;
            }

            MPlug duplicateWorldMesh = duplicateMeshFn.findPlug("worldMesh", true, &status);
            if (!status) return status;
            if (status == MS::kSuccess) {
                duplicateWorldMesh = duplicateWorldMesh.elementByLogicalIndex(0U, &status);
            }
            if (!status) return status;
            MPlug duplicateInput = duplicaterFn.findPlug("duplicateObjects", true, &status);
            if (!status) return status;
            if (status == MS::kSuccess) {
                duplicateInput = duplicateInput.elementByLogicalIndex(0U, &status);
            }
            if (!status) return status;
            MPlug outputMesh = duplicaterFn.findPlug("outPutMesh", true, &status);
            if (!status) return status;
            MPlug meshInput = outputMeshFn.findPlug("inMesh", true, &status);
            if (status != MS::kSuccess ||
                dagMod.connect(duplicateWorldMesh, duplicateInput) != MS::kSuccess ||
                dagMod.connect(outputMesh, meshInput) != MS::kSuccess) {
                return MS::kFailure;
            }

            MSelectionList shadingSelection;
            status = MGlobal::getSelectionListByName("initialShadingGroup", shadingSelection);
            if (status == MS::kSuccess) {
                status = shadingSelection.getDependNode(0U, shadingGroup);
            }
            if (status != MS::kSuccess || shadingGroup.isNull()) {
                return MS::kFailure;
            }

            status = redoIt();
            if (status != MS::kSuccess) {
                return status;
            }
            sceneApplied = true;
            const MString resultName = duplicaterFn.name(&status);
            if (!status) return status;
            setResult(resultName);
            return MS::kSuccess;
        }, [&]() -> MStatus {
            return sceneApplied ? undoIt() : MStatus(MS::kSuccess);
        });
    } DOMMY_COMMAND_CATCH("spaceDuplicaterCmd")
}

MStatus spaceDuplicaterCmd::redoIt()
{
    try {
        bool dagAttempted = false;
        return dommy::maya::runCommandTransaction("spaceDuplicaterCmd::redoIt", [&]() -> MStatus {
            if (hasShadingMembership) return MS::kSuccess;
            dagAttempted = true;
            MStatus status = dagMod.doIt();
            if (!status) return status;
            MFnSet shadingGroupFn(shadingGroup, &status);
            if (!status) return status;
            status = shadingGroupFn.addMember(outputMeshNode);
            if (status) hasShadingMembership = true;
            return status;
        }, [&]() -> MStatus {
            if (!dagAttempted) return MS::kSuccess;
            const MStatus status = dagMod.undoIt();
            if (status) hasShadingMembership = false;
            return status;
        });
    } DOMMY_COMMAND_CATCH("spaceDuplicaterCmd::redoIt")
}

MStatus spaceDuplicaterCmd::undoIt()
{
    try {
        bool membershipAttempted = false;
        bool dagUndoAttempted = false;
        return dommy::maya::runCommandTransaction("spaceDuplicaterCmd::undoIt", [&]() -> MStatus {
            if (!hasShadingMembership) return MS::kFailure;
            MStatus status;
            MFnSet shadingGroupFn(shadingGroup, &status);
            if (!status) return status;
            membershipAttempted = true;
            status = shadingGroupFn.removeMember(outputMeshNode);
            if (!status) return status;
            dagUndoAttempted = true;
            status = dagMod.undoIt();
            if (status) hasShadingMembership = false;
            return status;
        }, [&]() -> MStatus {
            if (!membershipAttempted) return MS::kSuccess;
            MStatus firstFailure = MS::kSuccess;
            if (dagUndoAttempted) {
                firstFailure = dommy::maya::invokeCommandOperation(
                    "spaceDuplicaterCmd", "node restore", [&]() { return dagMod.doIt(); });
            }
            // Attempt both independent restoration steps, even if one fails.
            const MStatus membershipStatus = dommy::maya::invokeCommandOperation(
                "spaceDuplicaterCmd", "membership restore", [&]() -> MStatus {
                    MStatus status;
                    MFnSet shadingGroupFn(shadingGroup, &status);
                    return status ? shadingGroupFn.addMember(outputMeshNode) : status;
                });
            return firstFailure ? membershipStatus : firstFailure;
        });
    } DOMMY_COMMAND_CATCH("spaceDuplicaterCmd::undoIt")
}
