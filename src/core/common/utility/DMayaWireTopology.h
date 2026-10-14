#pragma once

#include "DWireTopologyPlan.h"
#include "DMayaCheckedArray.h"
#include "DStatusOperation.h"

#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MStatus.h>

namespace dommy {
namespace maya {

inline MStatus copyWireTopologyToMaya(
    const mesh::WireTopologyPlan& plan,
    MIntArray& faceCounts,
    MIntArray& faceVertexIndices,
    MFloatArray& uCoordinates,
    MFloatArray& vCoordinates,
    MIntArray& uvCounts,
    MIntArray& uvIndices)
{
    if (!mesh::hasValidWireTopology(plan)) return MS::kInvalidParameter;

    MIntArray preparedFaceCounts, preparedFaceVertexIndices;
    MFloatArray preparedUCoordinates, preparedVCoordinates;
    MIntArray preparedUvCounts, preparedUvIndices;
    DOMMY_RETURN_IF_FAILURE(buildCheckedArray(plan.faceCounts.size(),
        [&](unsigned int index, int& value) -> MStatus {
            value = plan.faceCounts[index];
            return MS::kSuccess;
        }, preparedFaceCounts));
    DOMMY_RETURN_IF_FAILURE(buildCheckedArray(plan.faceVertexIndices.size(),
        [&](unsigned int index, int& value) -> MStatus {
            value = plan.faceVertexIndices[index];
            return MS::kSuccess;
        }, preparedFaceVertexIndices));
    DOMMY_RETURN_IF_FAILURE(buildCheckedArray(plan.uCoordinates.size(),
        [&](unsigned int index, float& value) -> MStatus {
            value = plan.uCoordinates[index];
            return MS::kSuccess;
        }, preparedUCoordinates));
    DOMMY_RETURN_IF_FAILURE(buildCheckedArray(plan.vCoordinates.size(),
        [&](unsigned int index, float& value) -> MStatus {
            value = plan.vCoordinates[index];
            return MS::kSuccess;
        }, preparedVCoordinates));
    DOMMY_RETURN_IF_FAILURE(buildCheckedArray(plan.uvCounts.size(),
        [&](unsigned int index, int& value) -> MStatus {
            value = plan.uvCounts[index];
            return MS::kSuccess;
        }, preparedUvCounts));
    DOMMY_RETURN_IF_FAILURE(buildCheckedArray(plan.uvIndices.size(),
        [&](unsigned int index, int& value) -> MStatus {
            value = plan.uvIndices[index];
            return MS::kSuccess;
        }, preparedUvIndices));

    // Construction failures leave every destination untouched. Once copying
    // starts, a failed Maya copy can leave partial destinations: discard them.
    DOMMY_RETURN_IF_FAILURE(copyCheckedArray(preparedFaceCounts, faceCounts));
    DOMMY_RETURN_IF_FAILURE(copyCheckedArray(preparedFaceVertexIndices, faceVertexIndices));
    DOMMY_RETURN_IF_FAILURE(copyCheckedArray(preparedUCoordinates, uCoordinates));
    DOMMY_RETURN_IF_FAILURE(copyCheckedArray(preparedVCoordinates, vCoordinates));
    DOMMY_RETURN_IF_FAILURE(copyCheckedArray(preparedUvCounts, uvCounts));
    return copyCheckedArray(preparedUvIndices, uvIndices);
}

} // namespace maya
} // namespace dommy
