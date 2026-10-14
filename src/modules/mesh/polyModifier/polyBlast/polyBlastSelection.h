#pragma once

#include "common/utility/DMeshSelection.h"

#include <maya/MFnComponentListData.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MIntArray.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace poly_blast {

inline MStatus copyFaceIdsToMaya(const std::vector<int>& source,
                                 MIntArray& destination)
{
    if (source.size() >
        static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        return MS::kInvalidParameter;
    }

    MStatus status = destination.setLength(
        static_cast<unsigned int>(source.size()));
    if (!status) return status;
    for (std::size_t index = 0U; index < source.size(); ++index) {
        destination[static_cast<unsigned int>(index)] = source[index];
    }
    return MS::kSuccess;
}

inline MStatus buildPolygonComponentList(const std::vector<int>& faceIds,
                                         MObject& componentList)
{
    MIntArray mayaFaceIds;
    MStatus status = copyFaceIdsToMaya(faceIds, mayaFaceIds);
    if (!status) return status;

    MFnSingleIndexedComponent componentFunction;
    MObject component = componentFunction.create(
        MFn::kMeshPolygonComponent, &status);
    if (!status) return status;
    status = componentFunction.addElements(mayaFaceIds);
    if (!status) return status;

    MFnComponentListData componentListFunction;
    const MObject result = componentListFunction.create(&status);
    if (!status) return status;
    status = componentListFunction.add(component);
    if (!status) return status;
    componentList = result;
    return MS::kSuccess;
}

inline MStatus readPolygonFaceComponent(
    const MObject& component,
    std::size_t polygonCount,
    std::size_t maximumFaceCount,
    std::vector<int>& faceIds)
{
    if (component.isNull() ||
        !component.hasFn(MFn::kMeshPolygonComponent)) {
        return MS::kInvalidParameter;
    }

    MStatus status;
    MFnSingleIndexedComponent componentFunction(component, &status);
    if (!status) return status;
    const bool isComplete = componentFunction.isComplete(&status);
    if (!status) return status;
    if (isComplete) {
        int completeCount = 0;
        status = componentFunction.getCompleteData(completeCount);
        if (!status) return status;
        if (completeCount < 0 ||
            static_cast<std::size_t>(completeCount) != polygonCount) {
            return MS::kInvalidParameter;
        }
        return dommy::mesh::buildPercentageFaceSelection(
                   polygonCount, 100.0, maximumFaceCount, faceIds)
            ? MStatus(MS::kSuccess)
            : MStatus(MS::kInvalidParameter);
    }

    MIntArray requestedFaceIds;
    status = componentFunction.getElements(requestedFaceIds);
    if (!status) return status;
    if (static_cast<std::size_t>(requestedFaceIds.length()) >
        maximumFaceCount) {
        return MS::kInvalidParameter;
    }

    std::vector<int> requestedFaces;
    requestedFaces.reserve(
        static_cast<std::size_t>(requestedFaceIds.length()));
    for (unsigned int index = 0U;
         index < requestedFaceIds.length(); ++index) {
        requestedFaces.push_back(requestedFaceIds[index]);
    }
    return dommy::mesh::normalizeFaceSelection(
               requestedFaces, polygonCount, maximumFaceCount, faceIds)
        ? MStatus(MS::kSuccess)
        : MStatus(MS::kInvalidParameter);
}

} // namespace poly_blast
} // namespace dommy
