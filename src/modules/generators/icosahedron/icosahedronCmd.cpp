//
//  icosahedronCmd.cpp
//  dommyTool
//
//  Created by yuzo on 3/29/13.
//

#include "icosahedronCmd.h"
#include "icosahedronNode.h"
#include "common/utility/DCommandGuard.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnSet.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>

MStatus icosahedronCmd::doIt(const MArgList& args)
{
    try {
        bool sceneApplied = false;
        return dommy::maya::runCommandTransaction("icosahedronCmd", [&]() -> MStatus {
            (void)args;
            MStatus status;

            const MObject transformNode = dagMod.createNode(
                "transform", MObject::kNullObj, &status);
            if (!status) {
                return status;
            }
            status = dagMod.renameNode(transformNode, "icosahedron1");
            if (!status) {
                return status;
            }

            outputMeshNode = dagMod.createNode("mesh", transformNode, &status);
            if (!status) {
                return status;
            }
            status = dagMod.renameNode(outputMeshNode, "icosahedronShape");
            if (!status) {
                return status;
            }

            const MObject generatorNode =
                dagMod.MDGModifier::createNode(icosahedronNode::typeId, &status);
            if (!status) {
                return status;
            }

            MFnDependencyNode outputMeshFn(outputMeshNode, &status);
            if (!status) {
                return status;
            }
            MFnDependencyNode generatorFn(generatorNode, &status);
            if (!status) {
                return status;
            }

            const MPlug outputMesh =
                generatorFn.findPlug("outPutMesh", true, &status);
            if (!status) {
                return status;
            }
            const MPlug meshInput =
                outputMeshFn.findPlug("inMesh", true, &status);
            if (!status) {
                return status;
            }
            status = dagMod.connect(outputMesh, meshInput);
            if (!status) {
                return status;
            }

            MSelectionList shadingSelection;
            status = MGlobal::getSelectionListByName(
                "initialShadingGroup", shadingSelection);
            if (status) {
                status = shadingSelection.getDependNode(0U, shadingGroup);
            }
            if (!status || shadingGroup.isNull()) {
                MGlobal::displayError(
                    "icosahedronCmd could not resolve initialShadingGroup.");
                return status ? MStatus(MS::kFailure) : status;
            }

            status = redoIt();
            if (!status) {
                return status;
            }
            sceneApplied = true;

            MFnDependencyNode resultFn(generatorNode, &status);
            if (!status) {
                return status;
            }
            const MString resultName = resultFn.name(&status);
            if (!status) return status;
            setResult(resultName);
            return MS::kSuccess;
        }, [&]() -> MStatus {
            return sceneApplied ? undoIt() : MStatus(MS::kSuccess);
        });
    } DOMMY_COMMAND_CATCH("icosahedronCmd")
}

MStatus icosahedronCmd::redoIt()
{
    try {
        bool dagAttempted = false;
        return dommy::maya::runCommandTransaction("icosahedronCmd::redoIt", [&]() -> MStatus {
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
    } DOMMY_COMMAND_CATCH("icosahedronCmd::redoIt")
}

MStatus icosahedronCmd::undoIt()
{
    try {
        bool membershipAttempted = false;
        bool dagUndoAttempted = false;
        return dommy::maya::runCommandTransaction("icosahedronCmd::undoIt", [&]() -> MStatus {
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
                    "icosahedronCmd", "node restore", [&]() { return dagMod.doIt(); });
            }
            // Attempt both independent restoration steps, even if one fails.
            const MStatus membershipStatus = dommy::maya::invokeCommandOperation(
                "icosahedronCmd", "membership restore", [&]() -> MStatus {
                    MStatus status;
                    MFnSet shadingGroupFn(shadingGroup, &status);
                    return status ? shadingGroupFn.addMember(outputMeshNode) : status;
                });
            return firstFailure ? membershipStatus : firstFailure;
        });
    } DOMMY_COMMAND_CATCH("icosahedronCmd::undoIt")
}
