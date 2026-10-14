#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace mesh {

// Validates a Maya-style face-id list and returns a sorted, duplicate-free
// selection. The destination is changed only after the complete input has
// passed validation.
inline bool normalizeFaceSelection(
    const std::vector<int>& faceIds,
    std::size_t polygonCount,
    std::size_t maximumFaceCount,
    std::vector<int>& normalizedFaceIds)
{
    if (polygonCount > maximumFaceCount ||
        polygonCount > static_cast<std::size_t>(
            std::numeric_limits<int>::max()) ||
        faceIds.size() > maximumFaceCount) {
        return false;
    }

    std::vector<int> result;
    result.reserve(faceIds.size());
    for (std::size_t index = 0U; index < faceIds.size(); ++index) {
        const int faceId = faceIds[index];
        if (faceId < 0 ||
            static_cast<std::size_t>(faceId) >= polygonCount) {
            return false;
        }
        result.push_back(faceId);
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    normalizedFaceIds.swap(result);
    return true;
}

// Matches the archive node's historical behaviour: a percentage selects a
// deterministic prefix of the mesh faces, truncating fractional counts.
inline bool buildPercentageFaceSelection(
    std::size_t polygonCount,
    double deletePercent,
    std::size_t maximumFaceCount,
    std::vector<int>& faceIds)
{
    if (polygonCount > maximumFaceCount ||
        polygonCount > static_cast<std::size_t>(
            std::numeric_limits<int>::max()) ||
        !std::isfinite(deletePercent) || deletePercent < 0.0 ||
        deletePercent > 100.0) {
        return false;
    }

    const double rawCount =
        static_cast<double>(polygonCount) * (deletePercent / 100.0);
    if (!std::isfinite(rawCount) || rawCount < 0.0 ||
        rawCount > static_cast<double>(polygonCount)) {
        return false;
    }

    const std::size_t faceCount = static_cast<std::size_t>(rawCount);
    std::vector<int> result;
    result.reserve(faceCount);
    for (std::size_t index = 0U; index < faceCount; ++index) {
        result.push_back(static_cast<int>(index));
    }
    faceIds.swap(result);
    return true;
}

inline bool buildPointSelectionFromFaces(
    std::size_t pointCount, const std::vector<std::size_t>& faceVertexCounts,
    const std::vector<std::size_t>& faceVertexIndices, const std::vector<bool>& faceSelection,
    std::vector<bool>& pointSelection) {
    if (faceVertexCounts.size() != faceSelection.size())
        return false;

    std::vector<bool> result(pointCount, false);
    std::size_t connectionOffset = 0;
    for (std::size_t faceIndex = 0; faceIndex < faceVertexCounts.size(); ++faceIndex) {
        const std::size_t vertexCount = faceVertexCounts[faceIndex];
        if (vertexCount > faceVertexIndices.size() - connectionOffset)
            return false;

        for (std::size_t localIndex = 0; localIndex < vertexCount; ++localIndex) {
            const std::size_t pointIndex = faceVertexIndices[connectionOffset + localIndex];
            if (pointIndex >= pointCount)
                return false;
            if (faceSelection[faceIndex])
                result[pointIndex] = true;
        }
        connectionOffset += vertexCount;
    }

    if (connectionOffset != faceVertexIndices.size())
        return false;

    pointSelection.swap(result);
    return true;
}

} // namespace mesh
} // namespace dommy
