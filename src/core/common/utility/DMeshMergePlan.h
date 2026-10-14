#pragma once

#include "common/utility/DGenerationLimits.h"
#include <initializer_list>

namespace dommy {
namespace mesh {

// Totals also define the offsets of the next primitive. No geometry allocation
// is performed until every input has passed this checked accumulation.
struct MeshMergePlan {
    MeshMergePlan() : meshes(0U), points(0U), faces(0U), connections(0U),
                      uvs(0U), workItems(0U) {}
    std::size_t meshes, points, faces, connections, uvs, workItems;
};

inline bool appendMeshMergePlan(std::size_t points, std::size_t faces,
                                std::size_t connections, std::size_t uvs,
                                MeshMergePlan& plan)
{
    MeshMergePlan next = plan;
    if (!generation::checkedSum(next.meshes, 1U,
            generation::kMaxGeneratedMeshCount, next.meshes) ||
        !generation::checkedSum(next.points, points,
            generation::kMaxGeneratedPointCount, next.points) ||
        !generation::checkedSum(next.faces, faces,
            generation::kMaxGeneratedFaceCount, next.faces) ||
        !generation::checkedSum(next.connections, connections,
            generation::kMaxWorkItemCount, next.connections) ||
        !generation::checkedSum(next.uvs, uvs,
            generation::kMaxWorkItemCount, next.uvs)) return false;

    // Positions, face/connection arrays, both UV coordinate arrays, and the
    // full per-face/per-corner UV assignment. This is a conservative budget
    // even when mixed UV presence makes the output drop UVs.
    next.workItems = next.points;
    for (const std::size_t count : {next.faces, next.connections, next.uvs,
                                   next.uvs, next.faces, next.connections}) {
        if (!generation::checkedSum(next.workItems, count,
                generation::kMaxWorkItemCount, next.workItems)) return false;
    }
    plan = next;
    return true;
}

// Count-only inspection of owning pointers or synchronous borrowed views.
// Topology/value validation remains the caller's responsibility. Rejecting a
// null source or an excessive count leaves the accumulated plan unchanged.
template <typename Iterator>
bool appendMeshRangePlan(Iterator begin, Iterator end, MeshMergePlan& plan)
{
    MeshMergePlan next = plan;
    for (; begin != end; ++begin) {
        const auto& mesh = *begin;
        if (!mesh || !appendMeshMergePlan(mesh->numberOfPoints(),
                static_cast<std::size_t>(mesh->numFace()), mesh->numberOfConnections(),
                mesh->numberOfUVs(), next)) return false;
    }
    plan = next;
    return true;
}

// A geometry staging estimate, not a bound on Maya's allocator or DG caches.
const std::size_t kMaxMeshOperationBytes = 256U * 1024U * 1024U;

// Keep one estimate for boolean admission and retained-storage consumers.
// The destination is unchanged on overflow or policy rejection.
inline bool meshOperationStorageBytes(const MeshMergePlan& plan,
                                      std::size_t& output,
                                      std::size_t extraBytes = 0U)
{
    const std::size_t maximum = kMaxMeshOperationBytes;
    if (extraBytes > maximum) return false;
    std::size_t bytes = extraBytes, part = 0U;
    const std::size_t counts[] = {plan.points, plan.faces, plan.connections, plan.uvs, plan.meshes};
    const std::size_t widths[] = {128U, 32U, 32U, 16U, 128U};
    for (std::size_t i = 0U; i < 5U; ++i)
        if (!generation::checkedProduct(counts[i], widths[i], maximum, part) ||
            !generation::checkedSum(bytes, part, maximum, bytes)) return false;
    output = bytes;
    return true;
}

inline bool validMeshOperationStorage(const MeshMergePlan& plan,
                                      std::size_t extraBytes = 0U)
{
    std::size_t bytes = 0U;
    return meshOperationStorageBytes(plan, bytes, extraBytes);
}

// Treat every selected face corner as a separate boundary corner. This bounds
// both together/separate face extrusion without constructing a Maya mesh.
inline bool appendExtrusionPlan(std::size_t points, std::size_t faces,
                                std::size_t connections, std::size_t uvs,
                                std::size_t selectedCorners, int divisions,
                                MeshMergePlan& plan)
{
    std::size_t steps = 0U, growth = 0U, corners = 0U;
    if (!generation::checkedCount(divisions, generation::kMaxGeneratedTransformCount, steps) ||
        steps == 0U || selectedCorners > connections ||
        !generation::checkedProduct(selectedCorners, steps, generation::kMaxGeneratedFaceCount, growth) ||
        !generation::checkedProduct(growth, 4U, generation::kMaxWorkItemCount, corners) ||
        !generation::checkedSum(points, growth, generation::kMaxGeneratedPointCount, points) ||
        !generation::checkedSum(faces, growth, generation::kMaxGeneratedFaceCount, faces) ||
        !generation::checkedSum(connections, corners, generation::kMaxWorkItemCount, connections) ||
        !generation::checkedSum(uvs, corners, generation::kMaxWorkItemCount, uvs)) return false;
    MeshMergePlan candidate = plan;
    if (!appendMeshMergePlan(points, faces, connections, uvs, candidate) ||
        !validMeshOperationStorage(candidate)) return false;
    plan = candidate;
    return true;
}

inline bool repeatMeshMergePlan(const MeshMergePlan& source, std::size_t copies,
                                MeshMergePlan& output)
{
    MeshMergePlan candidate;
    if (copies == 0U ||
        !generation::checkedProduct(source.meshes, copies,
            generation::kMaxGeneratedMeshCount, candidate.meshes) ||
        !generation::checkedProduct(source.points, copies,
            generation::kMaxGeneratedPointCount, candidate.points) ||
        !generation::checkedProduct(source.faces, copies,
            generation::kMaxGeneratedFaceCount, candidate.faces) ||
        !generation::checkedProduct(source.connections, copies,
            generation::kMaxWorkItemCount, candidate.connections) ||
        !generation::checkedProduct(source.uvs, copies,
            generation::kMaxWorkItemCount, candidate.uvs) ||
        !generation::checkedProduct(source.workItems, copies,
            generation::kMaxWorkItemCount, candidate.workItems)) return false;
    output = candidate;
    return true;
}

} // namespace mesh
} // namespace dommy
