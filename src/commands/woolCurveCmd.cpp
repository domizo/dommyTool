#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MDagPath.h>
#include <maya/MPlug.h>
#include <maya/MFnNurbsCurve.h>


#include "woolCurve.h"
#include "woolCurveCmd.h"
#include "common/utility/DCommandGuard.h"



MStatus woolCurveCmd::doIt ( const MArgList &args )
{
	try {
	(void)args;
	MStatus status;

	const MObject transform = dagMod.createNode(
        "transform", MObject::kNullObj, &status);
    if (status != MS::kSuccess) return status;
	const MObject curve = dagMod.createNode(
        "nurbsCurve", transform, &status);
    if (status != MS::kSuccess) return status;
	MFnDependencyNode curveNode(curve, &status);
    if (status != MS::kSuccess) return status;

	const MObject wool = dgMod.createNode("woolCurve", &status);
    if (status != MS::kSuccess) return status;
	MFnDependencyNode woolNode(wool, &status);
    if (status != MS::kSuccess) return status;

    const MPlug outputCurve =
        woolNode.findPlug("outputCurve", true, &status);
    if (status != MS::kSuccess) return status;
    const MPlug inputCurve = curveNode.findPlug("create", true, &status);
    if (status != MS::kSuccess) return status;
	status = dgMod.connect(outputCurve, inputCurve);
    if (status != MS::kSuccess) return status;

	return redoIt();
	} DOMMY_COMMAND_CATCH("woolCurveCmd")
}

MStatus woolCurveCmd::undoIt()
{
    try {
        bool dgUndoAttempted = false;
        bool dagUndoAttempted = false;
        return dommy::maya::runCommandTransaction("woolCurveCmd::undoIt", [&]() -> MStatus {
            dgUndoAttempted = true;
            MStatus status = dgMod.undoIt();
            if (!status) return status;
            dagUndoAttempted = true;
            return dagMod.undoIt();
        }, [&]() -> MStatus {
            MStatus firstFailure = MS::kSuccess;
            if (dagUndoAttempted) {
                firstFailure = dommy::maya::invokeCommandOperation(
                    "woolCurveCmd", "DAG restore", [&]() { return dagMod.doIt(); });
            }
            if (dgUndoAttempted) {
                const MStatus status = dommy::maya::invokeCommandOperation(
                    "woolCurveCmd", "DG restore", [&]() { return dgMod.doIt(); });
                if (firstFailure && !status) firstFailure = status;
            }
            return firstFailure;
        });
    } DOMMY_COMMAND_CATCH("woolCurveCmd::undoIt")
}

MStatus woolCurveCmd::redoIt()
{
    try {
        bool dagAttempted = false;
        bool dgAttempted = false;
        return dommy::maya::runCommandTransaction("woolCurveCmd::redoIt", [&]() -> MStatus {
            dagAttempted = true;
            const MStatus status = dagMod.doIt();
            if (!status) return status;
            dgAttempted = true;
            return dgMod.doIt();
        }, [&]() -> MStatus {
            MStatus firstFailure = MS::kSuccess;
            if (dgAttempted) {
                firstFailure = dommy::maya::invokeCommandOperation(
                    "woolCurveCmd", "DG rollback", [&]() { return dgMod.undoIt(); });
            }
            if (dagAttempted) {
                const MStatus status = dommy::maya::invokeCommandOperation(
                    "woolCurveCmd", "DAG rollback", [&]() { return dagMod.undoIt(); });
                if (firstFailure && !status) firstFailure = status;
            }
            return firstFailure;
        });
    } DOMMY_COMMAND_CATCH("woolCurveCmd::redoIt")
}
