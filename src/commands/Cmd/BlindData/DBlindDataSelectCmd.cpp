//
//  DBlindDataSelectCmd.cpp
//  dommyTool
//
//  Created by yuzo on 12/21/16.
//
//

#include "DBlindDataSelectCmd.h"
#include "common/utility/DCommandGuard.h"


#include "DMayaHeaders.h"
#include "DMayaDebug.h"

MStatus DBlindDataSelectCmd::doIt( const MArgList& args )
{
	try {
	(void)args;
	MStatus status;
	MSelectionList activeList;
	status = MGlobal::getActiveSelectionList(activeList);
    if (status != MS::kSuccess) return status;

	MItSelectionList iter(activeList, MFn::kMesh, &status);
    if (status != MS::kSuccess) return status;
    if (iter.isDone()) {
        MGlobal::displayError("Select at least one polygon mesh.");
        return MS::kFailure;
    }

    const int blindDataId = 300;
    const MString attributeName("surface_polygon_group");
    bool foundBlindData = false;
    MSelectionList resultSelection;

	for (; !iter.isDone();) {
        MDagPath meshPath;
        MObject selectedComponent;
        status = iter.getDagPath(meshPath, selectedComponent);
        if (status != MS::kSuccess || !meshPath.hasFn(MFn::kMesh)) {
            return status == MS::kSuccess ? MS::kFailure : status;
        }

        MFnMesh mesh(meshPath, &status);
        if (status != MS::kSuccess) return status;
        if (!mesh.hasBlindData(
                MFn::kMeshPolygonComponent, blindDataId)) {
            status = iter.next();
            if (status != MS::kSuccess) return status;
            continue;
        }
        foundBlindData = true;

        MItMeshPolygon polygon(meshPath, MObject::kNullObj, &status);
        if (status != MS::kSuccess) return status;
        for (; !polygon.isDone();) {
            const int polygonIndex =
                static_cast<int>(polygon.index(&status));
            if (status != MS::kSuccess) return status;

            bool selected = false;
            status = mesh.getBoolBlindData(
                polygonIndex,
                MFn::kMeshPolygonComponent,
                blindDataId,
                attributeName,
                selected);
            if (status != MS::kSuccess) return status;
            if (selected) {
                const MObject component = polygon.currentItem(&status);
                if (status != MS::kSuccess) return status;
                status = resultSelection.add(meshPath, component);
                if (status != MS::kSuccess) return status;
            }

            status = polygon.next();
            if (status != MS::kSuccess) return status;
        }

        status = iter.next();
        if (status != MS::kSuccess) return status;
	}

    if (!foundBlindData) {
        MGlobal::displayWarning(
            "The selected meshes have no matching polygon blind data.");
        setResult(0);
        return MS::kSuccess;
    }

    status = MGlobal::selectCommand(resultSelection);
    if (status != MS::kSuccess) return status;
    setResult(static_cast<int>(resultSelection.length()));
	return MS::kSuccess;
	} DOMMY_COMMAND_CATCH("DBlindDataSelectCmd")
}
