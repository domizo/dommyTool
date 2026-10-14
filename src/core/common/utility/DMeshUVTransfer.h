#pragma once

#include "DGenerationLimits.h"
#include "DMayaCheckedArray.h"

#include <maya/MStatus.h>

#include <cmath>
#include <vector>

namespace dommy {
namespace mesh {

// On failure, outputs can contain a copied prefix. Callers must discard them
// and propagate the status before accessing or publishing any output array.
template <typename FloatArray, typename IntArray>
MStatus copyMeshUVArrays(const FloatArray& sourceU, const FloatArray& sourceV,
                        const IntArray& sourceCounts, const IntArray& sourceIds,
                        FloatArray& u, FloatArray& v, IntArray& counts, IntArray& ids)
{
    MStatus status = maya::copyCheckedArray(sourceU, u);
    if (status != MS::kSuccess) return status;
    status = maya::copyCheckedArray(sourceV, v);
    if (status != MS::kSuccess) return status;
    status = maya::copyCheckedArray(sourceCounts, counts);
    if (status != MS::kSuccess) return status;
    return maya::copyCheckedArray(sourceIds, ids);
}

template <typename FloatArray, typename IntArray>
bool validFullMeshUVArrays(const IntArray& faces,
                          const FloatArray& u, const FloatArray& v,
                          const IntArray& counts, const IntArray& ids)
{
    if (u.length() == 0U && v.length() == 0U &&
        counts.length() == 0U && ids.length() == 0U) return true;
    if (u.length() == 0U || u.length() != v.length() ||
        counts.length() != faces.length()) return false;

    std::size_t work = 0U;
    if (!generation::checkedProduct(u.length(), 2U,
            generation::kMaxWorkItemCount, work) ||
        !generation::checkedSum(work, counts.length(),
            generation::kMaxWorkItemCount, work) ||
        !generation::checkedSum(work, ids.length(),
            generation::kMaxWorkItemCount, work)) return false;

    std::size_t connections = 0U;
    for (unsigned int face = 0U; face < faces.length(); ++face) {
        if (counts[face] < 3 || counts[face] != faces[face] ||
            !generation::checkedSum(connections,
                static_cast<std::size_t>(counts[face]),
                generation::kMaxWorkItemCount, connections)) return false;
    }
    if (connections != ids.length()) return false;
    for (unsigned int uv = 0U; uv < u.length(); ++uv) {
        if (!std::isfinite(u[uv]) || !std::isfinite(v[uv])) return false;
    }
    for (unsigned int connection = 0U; connection < ids.length(); ++connection) {
        if (ids[connection] < 0 ||
            static_cast<unsigned int>(ids[connection]) >= u.length()) return false;
    }
    return true;
}

struct UVTransferSlice {
    std::size_t faceOffset;
    std::size_t faceCount;
    std::size_t connectionOffset;
    std::size_t connectionCount;
};

inline bool validMeshUVTransferBudget(std::size_t faces, std::size_t connections,
                                     std::size_t uvs)
{
    if (faces > generation::kMaxGeneratedFaceCount) return false;
    const std::size_t maximum = 256U * 1024U * 1024U;
    std::size_t work = 0U, bytes = 0U, part = 0U;
    const std::size_t counts[] = {uvs, faces, connections};
    const std::size_t workWidths[] = {2U, 3U, 7U};
    const std::size_t byteWidths[] = {8U, 32U, 40U};
    for (std::size_t i = 0U; i < 3U; ++i) {
        if (!generation::checkedProduct(counts[i], workWidths[i], generation::kMaxWorkItemCount, part) ||
            !generation::checkedSum(work, part, generation::kMaxWorkItemCount, work) ||
            !generation::checkedProduct(counts[i], byteWidths[i], maximum, part) ||
            !generation::checkedSum(bytes, part, maximum, bytes)) return false;
    }
    return true;
}

// DMCopyUV preserves its historical face-order mapping and per-corner UV
// expansion. Partial UV assignments are not supported by DMesh: reject them
// explicitly instead of silently retaining old UVs on some primitives.
template <typename IntArray>
bool planMeshUVTransfer(const std::vector<std::vector<int>>& destinationFaces,
                        const IntArray& sourceCounts,
                        std::size_t sourceConnectionCount,
                        std::size_t sourceUVCount,
                        std::vector<UVTransferSlice>& result)
{
    if (destinationFaces.size() > generation::kMaxGeneratedMeshCount ||
        sourceCounts.length() > generation::kMaxGeneratedFaceCount) return false;

    std::size_t faceOffset = 0U;
    std::size_t connectionOffset = 0U;
    if (!validMeshUVTransferBudget(sourceCounts.length(), sourceConnectionCount, sourceUVCount))
        return false;

    std::vector<UVTransferSlice> candidate;
    candidate.reserve(destinationFaces.size());
    for (const std::vector<int>& faces : destinationFaces) {
        if (faces.empty() || faceOffset > sourceCounts.length() ||
            faces.size() > sourceCounts.length() - faceOffset) return false;
        const std::size_t firstConnection = connectionOffset;
        for (std::size_t face = 0U; face < faces.size(); ++face) {
            const int count = sourceCounts[static_cast<unsigned int>(faceOffset + face)];
            if (count < 3 || faces[face] != count ||
                !generation::checkedSum(connectionOffset,
                    static_cast<std::size_t>(count),
                    generation::kMaxWorkItemCount, connectionOffset)) return false;
        }
        candidate.push_back(UVTransferSlice{
            faceOffset, faces.size(), firstConnection,
            connectionOffset - firstConnection});
        faceOffset += faces.size();
    }
    if (faceOffset != sourceCounts.length() ||
        connectionOffset != sourceConnectionCount) return false;
    result.swap(candidate);
    return true;
}

} // namespace mesh
} // namespace dommy
