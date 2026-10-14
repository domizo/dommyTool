

#include "common/utility/DStatusOperation.h"
#include "common/utility/DCommandSyntax.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "getCurveLengthPosCmd.h"
#include "common/utility/DCommandGuard.h"

#include <maya/MFnNurbsCurve.h>
#include <maya/MArgDatabase.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>
#include <maya/MPoint.h>
#include <maya/MDoubleArray.h>

#include <cmath>




#define klengthPosFlag			"-lp"
#define klengthPosLong			"-lengthPos"
#define kCurveFlag				"-c"
#define kCurveLong				"-curve"

getCurveLengthPosCmd::getCurveLengthPosCmd()
{
	lengthPos = 0.0;
	curve = "";
}

getCurveLengthPosCmd::~getCurveLengthPosCmd()
{
}

void* getCurveLengthPosCmd::creator()
{
    return ::dommy::maya::createObject("getCurveLengthPosCmd", [&]() {
        return new getCurveLengthPosCmd;
    });
}

MStatus getCurveLengthPosCmd::buildSyntax(MSyntax& syntax)
{
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(klengthPosFlag, klengthPosLong, MSyntax::kDouble));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kCurveFlag, kCurveLong, MSyntax::kString));
    
    return MS::kSuccess;
}

MSyntax getCurveLengthPosCmd::newSyntax() try
{
    return dommy::maya::createCommandSyntax<getCurveLengthPosCmd>("getCurveLengthPosCmd");
} catch (...) {
    dommy::maya::reportCommandSyntaxException<getCurveLengthPosCmd>(
        "getCurveLengthPosCmd", std::current_exception());
    return MSyntax();
}


MStatus getCurveLengthPosCmd::parseArgs(const MArgList& args)
{
    double parsedLengthPos = 0.0;
    MString parsedCurve;
    MSyntax commandSyntax;
    MStatus status = dommy::maya::buildCommandSyntax<getCurveLengthPosCmd>(
        commandSyntax, "getCurveLengthPosCmd");
    if (!status) {
        return status;
    }
    MArgDatabase argData(commandSyntax, args, &status);
    if (!status) {
        return status;
    }

    // Read into local values so a failed parse cannot publish partial arguments.
    if (argData.isFlagSet(klengthPosFlag)) {
        status = argData.getFlagArgument(klengthPosFlag, 0, parsedLengthPos);
        if (!status) {
            status.perror("lengthPos flag parsing failed");
            return status;
        }
    }
    if (argData.isFlagSet(kCurveFlag)) {
        status = argData.getFlagArgument(kCurveFlag, 0, parsedCurve);
        if (!status) {
            status.perror("curve flag parsing failed");
            return status;
        }
    }

    curve = parsedCurve;
    lengthPos = parsedLengthPos;
    return MS::kSuccess;
}


MStatus	getCurveLengthPosCmd::doIt( const MArgList& args )
{
	try {
	MStatus stat;
	
	stat = parseArgs(args);
	
	if (MS::kSuccess != stat)
		return stat;
	
	if (curve.length() == 0U || !std::isfinite(lengthPos) || lengthPos < 0.0) {
		displayError("curve must name a NURBS curve and lengthPos must be finite and non-negative.");
		return MS::kFailure;
	}
	
	MSelectionList list;
	stat = list.add(curve);
	if (stat != MS::kSuccess) {
		return stat;
	}
	if( list.length() == 0 )
	{
		stat.perror("Nothing is such as name!");
		return MS::kFailure;
	}
	
	
	
	MDagPath node;
	MObject component;
	MFnNurbsCurve curveFn;
	
	
	stat = list.getDagPath(0, node, component);
	if (stat != MS::kSuccess) {
		return stat;
	}
	if( !node.hasFn(MFn::kNurbsCurve) )
	{
		displayWarning( "Please Cuvre!\n" );
		return MS::kFailure;
	}
	stat = curveFn.setObject(node);
	if (stat != MS::kSuccess) {
		return stat;
	}
	
	//get uParameter from lengthPos
	double uParam = curveFn.findParamFromLength(lengthPos, &stat);
	if (stat != MS::kSuccess || !std::isfinite(uParam)) {
		return MS::kFailure;
	}
	
	MPoint worldPos;
	stat = curveFn.getPointAtParam(uParam, worldPos, MSpace::kWorld);
	if (stat != MS::kSuccess) {
		return stat;
	}
	
	MDoubleArray lengthPosArray;
	if (lengthPosArray.append(worldPos.x) != MS::kSuccess ||
		lengthPosArray.append(worldPos.y) != MS::kSuccess ||
		lengthPosArray.append(worldPos.z) != MS::kSuccess) {
		return MS::kFailure;
	}
	
	MPxCommand::setResult( lengthPosArray );
	
	return MS::kSuccess;
	} DOMMY_COMMAND_CATCH("getCurveLengthPosCmd")
}


	
