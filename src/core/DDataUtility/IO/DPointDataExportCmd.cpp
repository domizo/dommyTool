//
//  DPointDataExportCmd.cpp
//  dommyTool
//
//  Created by yuzo on 3/14/15.
//

#include "common/utility/DStatusOperation.h"
#include "common/utility/DCommandSyntax.h"
#include "DPointDataExportCmd.h"
#include "common/utility/DMayaPlugDataHandle.h"

#include "common/file/DAtomicFile.h"
#include "common/file/DExportPath.h"
#include "common/file/DLegacyTextData.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMayaCheckedArray.h"

#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MDataHandle.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MVectorArray.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

#define kFilePathFlag "-f"
#define kFilePathLong "-filePath"
#define kSavedNodeFlag "-sn"
#define kSavedNodeLong "-savedNode"

namespace {

const unsigned int kPointRecordsPerLine = 1024U;

bool isWithinLegacyTextBudget(std::ostringstream& stream)
{
    if (!stream) return false;
    const std::streampos position = stream.tellp();
    if (position == std::streampos(-1)) return false;
    const std::streamoff offset = position;
    return offset >= 0 &&
           static_cast<unsigned long long>(offset) <=
               dommy::file::kMaxLegacyTextBytes;
}

bool isFinite(const MPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
}

MStatus collectPoints(const MString& targetName,
                      std::size_t remainingPointCount,
                      MPointArray& points,
                      MString& typeName,
                      MString& nodeName)
{
    const std::size_t maximumPointCount = std::min(
        remainingPointCount, dommy::generation::kMaxGeneratedPointCount);
    if (maximumPointCount == 0U) return MS::kInvalidParameter;

    MStatus status;
    MSelectionList selection;
    status = selection.add(targetName);
    if (!status || selection.length() != 1U) {
        MGlobal::displayError(MString("Unable to find saved node: ") + targetName);
        return MS::kFailure;
    }

    MObject object;
    status = selection.getDependNode(0, object);
    if (!status || object.isNull()) {
        MGlobal::displayError(MString("Unable to resolve saved node: ") + targetName);
        return MS::kFailure;
    }

    MDagPath path;
    if (selection.getDagPath(0, path) == MS::kSuccess &&
        path.hasFn(MFn::kTransform)) {
        DOMMY_RETURN_IF_FAILURE(path.extendToShape());
        object = path.node(&status);
        if (!status) return status;
    }

    // Scratch may be partially populated on failure; doIt discards it before
    // creating any output file. Avoid an extra full geometry copy here.
    std::size_t expectedPointCount = 0U;
    MString candidateTypeName;
    MString candidateNodeName;
    if (path.isValid() && path.hasFn(MFn::kMesh)) {
        MFnMesh function(path, &status);
        if (!status) return status;
        const int rawCount = function.numVertices(&status);
        if (!status) return status;
        if (!dommy::generation::checkedCount(rawCount, maximumPointCount,
                expectedPointCount) || expectedPointCount == 0U)
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(function.getPoints(points, MSpace::kObject));
        candidateTypeName = "POINT_DATA_FROM_MESH";
        candidateNodeName = function.name(&status);
    } else if (path.isValid() && path.hasFn(MFn::kNurbsCurve)) {
        MFnNurbsCurve function(path, &status);
        if (!status) return status;
        const int rawCount = function.numCVs(&status);
        if (!status) return status;
        if (!dommy::generation::checkedCount(rawCount, maximumPointCount,
                expectedPointCount) || expectedPointCount == 0U)
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(function.getCVs(points, MSpace::kObject));
        candidateTypeName = "POINT_DATA_FROM_CURVE";
        candidateNodeName = function.name(&status);
    } else if (path.isValid() && path.hasFn(MFn::kNurbsSurface)) {
        MFnNurbsSurface function(path, &status);
        if (!status) return status;
        const int rawUCount = function.numCVsInU(&status);
        if (!status) return status;
        const int rawVCount = function.numCVsInV(&status);
        if (!status) return status;
        std::size_t uCount = 0U, vCount = 0U;
        if (!dommy::generation::checkedCount(rawUCount, maximumPointCount, uCount) ||
            !dommy::generation::checkedCount(rawVCount, maximumPointCount, vCount) ||
            uCount == 0U || vCount == 0U ||
            !dommy::generation::checkedProduct(uCount, vCount, maximumPointCount,
                expectedPointCount))
            return MS::kInvalidParameter;
        DOMMY_RETURN_IF_FAILURE(function.getCVs(points, MSpace::kObject));
        candidateTypeName = "POINT_DATA_FROM_SURFACE";
        candidateNodeName = function.name(&status);
    } else if (path.isValid() && path.hasFn(MFn::kParticle)) {
        MFnParticleSystem function(path, &status);
        if (!status) return status;
        expectedPointCount = function.count();
        if (expectedPointCount == 0U || expectedPointCount > maximumPointCount)
            return MS::kInvalidParameter;

        MVectorArray positions;
        const unsigned int readCount =
            function.getPerParticleAttribute("position", positions, &status);
        if (!status) return status;
        if (readCount != expectedPointCount || positions.length() != expectedPointCount)
            return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(
            positions.length(),
            [&](unsigned int index, MPoint& point) -> MStatus {
                point = MPoint(positions[index]);
                return isFinite(point) ? MStatus(MS::kSuccess)
                                       : MStatus(MS::kInvalidParameter);
            }, points));
        candidateTypeName = "POINT_DATA_FROM_PARTICLE";
        candidateNodeName = function.name(&status);
    } else {
        MFnDependencyNode function(object, &status);
        if (!status) return status;

        MPlug output = function.findPlug("outputPointArray", true, &status);
        if (!status || output.isNull()) {
            MGlobal::displayError(
                MString("Saved node has no supported point output: ") + targetName);
            return MS::kFailure;
        }

        dommy::maya::ScopedPlugDataHandle handleOwner;
        MDataHandle& handle = handleOwner.handle();
        status = handleOwner.read(output);
        if (!status) return status;
        const MObject dataObject = handle.data();
        if (dataObject.isNull()) return MS::kFailure;

        MFnPointArrayData pointData(dataObject, &status);
        if (!status) return status;
        expectedPointCount = pointData.length(&status);
        if (!status) return status;
        if (expectedPointCount == 0U || expectedPointCount > maximumPointCount)
            return MS::kInvalidParameter;
        const MPointArray source = pointData.array(&status);
        if (!status) return status;
        if (source.length() != expectedPointCount) return MS::kFailure;
        DOMMY_RETURN_IF_FAILURE(dommy::maya::copyCheckedArray(source, points));
        candidateTypeName = "POINT_DATA_FROM_DPOINT";
        candidateNodeName = function.name(&status);
    }

    if (!status) return status;
    if (points.length() != expectedPointCount) {
        MGlobal::displayError(
            MString("Saved node returned an unexpected point count: ") + targetName);
        return MS::kFailure;
    }
    for (unsigned int index = 0; index < points.length(); ++index) {
        if (!isFinite(points[index])) {
            MGlobal::displayError(
                MString("Saved node contains a non-finite point: ") + targetName);
            return MS::kFailure;
        }
    }
    typeName = candidateTypeName;
    nodeName = candidateNodeName;
    return MS::kSuccess;
}

} // namespace

MStatus DPointDataExportCmd::buildSyntax(MSyntax& syntax)
{
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kFilePathFlag, kFilePathLong, MSyntax::kString));
    DOMMY_RETURN_IF_FAILURE(syntax.addFlag(kSavedNodeFlag, kSavedNodeLong, MSyntax::kString));
    DOMMY_RETURN_IF_FAILURE(syntax.makeFlagMultiUse(kSavedNodeFlag));
    return MS::kSuccess;
}

MSyntax DPointDataExportCmd::newSyntax() try
{
    return dommy::maya::createCommandSyntax<DPointDataExportCmd>("DPointDataExportCmd");
} catch (...) {
    dommy::maya::reportCommandSyntaxException<DPointDataExportCmd>(
        "DPointDataExportCmd", std::current_exception());
    return MSyntax();
}

MStatus DPointDataExportCmd::parseArgs(const MArgList& args)
{
    MString parsedFilePath;
    std::vector<MString> parsedNodeNames;

    MSyntax commandSyntax;
    MStatus status = dommy::maya::buildCommandSyntax<DPointDataExportCmd>(
        commandSyntax, "DPointDataExportCmd");
    if (!status) return status;
    MArgDatabase arguments(commandSyntax, args, &status);
    if (!status) return status;

    if (arguments.isFlagSet(kFilePathFlag)) {
        status = arguments.getFlagArgument(kFilePathFlag, 0, parsedFilePath);
        if (!status) return status;
    }

    const unsigned int useCount = arguments.numberOfFlagUses(kSavedNodeFlag);
    parsedNodeNames.reserve(useCount);
    for (unsigned int use = 0; use < useCount; ++use) {
        MArgList values;
        status = arguments.getFlagArgumentList(kSavedNodeFlag, use, values);
        if (!status || values.length() != 1U) return MS::kFailure;

        MString nodeName;
        status = values.get(0, nodeName);
        if (!status || nodeName.length() == 0U) return MS::kFailure;
        parsedNodeNames.push_back(nodeName);
    }
    filePath = parsedFilePath;
    savedNodeNames.swap(parsedNodeNames);
    return MS::kSuccess;
}

MStatus DPointDataExportCmd::doIt(const MArgList& args)
{
    try {
    MStatus status = parseArgs(args);
    if (!status) return status;

    if (filePath.length() == 0U) {
        MGlobal::displayError("DPoint export requires -filePath.");
        return MS::kFailure;
    }
    const std::string outputPath = filePath.asChar();
    if (!dommy::file::isSafePathSyntax(outputPath)) {
        MGlobal::displayError(
            "DPoint export path is too long or contains unsafe components.");
        return MS::kInvalidParameter;
    }
    if (savedNodeNames.empty()) {
        MGlobal::displayError("DPoint export requires at least one -savedNode.");
        return MS::kFailure;
    }

    // Resolve and serialize every requested node before touching the target
    // file, so an invalid node cannot leave a partially generated export.
    std::ostringstream serialized;
    serialized.imbue(std::locale::classic());
    serialized << std::setprecision(std::numeric_limits<double>::max_digits10);
    serialized << "#D POINT DATA 1.0\n\n";
    if (!isWithinLegacyTextBudget(serialized)) {
        MGlobal::displayError("DPoint export header exceeds the text-size limit.");
        return MS::kFailure;
    }

    std::size_t totalPointCount = 0;
    for (std::size_t index = 0; index < savedNodeNames.size(); ++index) {
        MPointArray points;
        MString typeName;
        MString nodeName;
        status = collectPoints(
            savedNodeNames[index],
            dommy::file::kMaxLegacyTextRecords - totalPointCount,
            points, typeName, nodeName);
        if (!status) return status;

        const std::size_t pointCount = static_cast<std::size_t>(points.length());
        if (pointCount >
            dommy::file::kMaxLegacyTextRecords - totalPointCount) {
            MGlobal::displayError(
                "DPoint export exceeds the total point-count limit.");
            return MS::kFailure;
        }
        totalPointCount += pointCount;

        serialized << typeName.asChar() << ' ' << nodeName.asChar() << ' '
                   << points.length() << '\n';
        for (unsigned int first = 0; first < points.length();) {
            const unsigned int remaining = points.length() - first;
            const unsigned int lineCount =
                std::min(kPointRecordsPerLine, remaining);
            const unsigned int end = first + lineCount;
            serialized << 'P';
            for (unsigned int pointIndex = first; pointIndex < end; ++pointIndex) {
                const MPoint& point = points[pointIndex];
                serialized << ' ' << point.x << ' ' << point.y << ' ' << point.z;
            }
            serialized << '\n';
            if (!isWithinLegacyTextBudget(serialized)) {
                MGlobal::displayError(
                    "DPoint export exceeds the legacy text-size limit.");
                return MS::kFailure;
            }
            first = end;
        }
        if (!isWithinLegacyTextBudget(serialized)) {
            MGlobal::displayError("Failed to serialize DPoint export data.");
            return MS::kFailure;
        }
    }

    const std::string payload = serialized.str();
    if (payload.size() > dommy::file::kMaxLegacyTextBytes) {
        MGlobal::displayError("DPoint export exceeds the legacy text-size limit.");
        return MS::kFailure;
    }
	dommy::file::PendingOutputFile pending(outputPath,
        dommy::file::PendingOutputFile::Access::Handle,
        dommy::file::PendingOutputFile::Format::Text);
    std::ostream& output = pending.stream();
    output.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    output.flush();
    if (!output) {
        pending.close();
        MGlobal::displayError("Failed while writing the DPoint export file.");
        return MS::kFailure;
    }
    const bool closed = pending.close();
	if (output.fail() || !closed) {
		MGlobal::displayError("Failed to close the DPoint export file cleanly.");
		return MS::kFailure;
	}
	if (!pending.commit()) {
		MGlobal::displayError("Unable to commit the completed DPoint export file.");
		return MS::kFailure;
	}

	return MS::kSuccess;
    } DOMMY_COMMAND_CATCH("DPointDataExportCmd")
}
