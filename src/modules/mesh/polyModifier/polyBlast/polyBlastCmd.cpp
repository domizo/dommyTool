//
//  polyBlastCmd.cpp
//  dommyTool
//
//  Created by yuzo on 1/30/13.
//

#include "common/utility/DMayaEntryPointGuard.h"
#include "polyBlastCmd.h"

#include "polyBlastNode.h"
#include "polyBlastSelection.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshSelection.h"

#include <maya/MFnComponentListData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItSelectionList.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>

#include <cstddef>
#include <vector>

void* polyBlast::creator()
{
    return ::dommy::maya::createObject("polyBlast", [&]() {
        return new polyBlast();
    });
}

bool polyBlast::isUndoable() const
{
    return true;
}

MStatus polyBlast::doIt(const MArgList&) try
{
    if (!canStartPolyEdit()) return MS::kInvalidParameter;
    MStatus status;
    MSelectionList selection;
    status = MGlobal::getActiveSelectionList(selection);
    if (!status) return status;

    MItSelectionList selectionIterator(selection, MFn::kInvalid, &status);
    if (!status) return status;
    status = selectionIterator.setFilter(MFn::kMesh);
    if (!status) return status;

    MFnComponentListData emptyComponentListFunction;
    fComponentList = emptyComponentListFunction.create(&status);
    if (!status) return status;
    status = pSelectedFaces.clear();
    if (!status) return status;

    bool foundTarget = false;
    bool foundMultipleTargets = false;
    const unsigned int selectionCount = selection.length(&status);
    if (!status) return status;
    if (selectionCount > dommy::generation::kMaxConnectedGeometryCount) {
        return MS::kInvalidParameter;
    }
    unsigned int visited = 0;
    for (; !selectionIterator.isDone(); ) {
        if (visited++ >= selectionCount) return MS::kFailure;
        MDagPath dagPath;
        MObject component;
        status = selectionIterator.getDagPath(dagPath, component);
        if (!status) return status;

        if (dagPath.apiType() != MFn::kMesh) {
            status = dagPath.extendToShape();
            if (!status || dagPath.apiType() != MFn::kMesh) {
                status = selectionIterator.next();
                if (!status) return status;
                continue;
            }
        }

        const bool wholeObjectSelection = component.isNull();
        if (!wholeObjectSelection &&
            !component.hasFn(MFn::kMeshPolygonComponent)) {
            status = selectionIterator.next();
            if (!status) return status;
            continue;
        }
        if (foundTarget) {
            foundMultipleTargets = true;
            break;
        }

        MFnMesh meshFunction(dagPath, &status);
        if (!status) return status;
        const int polygonCount = meshFunction.numPolygons(&status);
        if (!status || polygonCount < 0 ||
            static_cast<std::size_t>(polygonCount) >
                dommy::generation::kMaxGeneratedFaceCount) {
            return MS::kInvalidParameter;
        }

        std::vector<int> selectedFaces;
        if (wholeObjectSelection) {
            if (!dommy::mesh::buildPercentageFaceSelection(
                    static_cast<std::size_t>(polygonCount),
                    dommy::poly_blast::kDefaultDeletePercent,
                    dommy::generation::kMaxGeneratedFaceCount,
                    selectedFaces)) {
                return MS::kInvalidParameter;
            }
        } else {
            status = dommy::poly_blast::readPolygonFaceComponent(
                    component,
                    static_cast<std::size_t>(polygonCount),
                    dommy::generation::kMaxGeneratedFaceCount,
                    selectedFaces);
            if (!status) {
                MGlobal::displayError(
                    "polyBlast: polygon-face selection is invalid.");
                return status;
            }
            if (selectedFaces.empty()) {
                MGlobal::displayError(
                    "polyBlast: select at least one valid polygon face.");
                return MS::kInvalidParameter;
            }
            status = dommy::poly_blast::buildPolygonComponentList(
                selectedFaces, fComponentList);
            if (!status) return status;
        }

        status = dommy::poly_blast::copyFaceIdsToMaya(
            selectedFaces, pSelectedFaces);
        if (!status) return status;
        setMeshNode(dagPath);
        foundTarget = true;
        status = selectionIterator.next();
        if (!status) return status;
    }

    if (!foundTarget) {
        MGlobal::displayError(
            "polyBlast: select one polygon mesh or polygon-face component.");
        return MS::kInvalidParameter;
    }
    if (foundMultipleTargets) {
        MGlobal::displayWarning(
            "polyBlast: multiple meshes selected; only the first is used.");
    }

    setModifierNodeType(polyBlastNode::id);
    bool editApplied = false;
    return dommy::maya::runCommandTransaction("polyBlast", [&]() -> MStatus {
        const MStatus result = doModifyPoly();
        if (!result) return result;
        editApplied = true;
        setResult("polyBlast command succeeded.");
        return MS::kSuccess;
    }, [&]() -> MStatus {
        return editApplied ? cancelModifyPoly() : MStatus(MS::kSuccess);
    });
}
DOMMY_COMMAND_CATCH("polyBlast")

MStatus polyBlast::redoIt() try
{
    const bool wasApplied = isPolyEditApplied();
    bool editApplied = false;
    return dommy::maya::runCommandTransaction("polyBlast redo", [&]() -> MStatus {
        const MStatus status = redoModifyPoly();
        if (!status) return status;
        editApplied = !wasApplied;
        setResult("polyBlast redo succeeded.");
        return MS::kSuccess;
    }, [&]() -> MStatus {
        return editApplied ? undoModifyPoly() : MStatus(MS::kSuccess);
    });
}
DOMMY_COMMAND_CATCH("polyBlast::redoIt")

MStatus polyBlast::undoIt() try
{
    const bool wasApplied = isPolyEditApplied();
    bool editUndone = false;
    return dommy::maya::runCommandTransaction("polyBlast undo", [&]() -> MStatus {
        const MStatus status = undoModifyPoly();
        if (!status) return status;
        editUndone = wasApplied;
        setResult("polyBlast undo succeeded.");
        return MS::kSuccess;
    }, [&]() -> MStatus {
        return editUndone ? redoModifyPoly() : MStatus(MS::kSuccess);
    });
}
DOMMY_COMMAND_CATCH("polyBlast::undoIt")

MStatus polyBlast::initModifierNode(MObject modifierNode)
{
    MStatus status;
    MFnDependencyNode dependencyNode(modifierNode, &status);
    if (!status) return status;
    const MObject componentAttribute =
        dependencyNode.attribute("inputComponents", &status);
    if (!status) return status;
    if (componentAttribute.isNull()) return MS::kFailure;

    MPlug componentPlug(modifierNode, componentAttribute);
    return componentPlug.setValue(fComponentList);
}

MStatus polyBlast::directModifier(MObject mesh)
{
    pBlastFactory.setMesh(mesh);
    pBlastFactory.setFaces(pSelectedFaces);
    return pBlastFactory.doIt();
}
