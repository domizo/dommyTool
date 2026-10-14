#pragma once

#include "geometry/DGeometryTypes.h"
#include "common/utility/DMeshMergePlan.h"

#include <cstddef>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace dommy {
namespace geometry {

// Conservative staging admission, separate from the logical work budget.
// Includes component/polygon indices, remaps, geometry copies and ordered containers;
// it is not a bound on Maya's allocator or the implementation's heap overhead.
inline bool triangleWorkspaceBytes(std::size_t points, std::size_t triangles,
                                   std::size_t& output)
{
    mesh::MeshMergePlan plan;
    std::size_t corners = 0U, pointBytes = 0U, triangleBytes = 0U, bytes = 128U;
    const std::size_t limit = mesh::kMaxMeshOperationBytes;
    // MeshMergePlan's base estimate plus 128 bytes/point and 384 bytes/face
    // for component/remesh/polygon staging: 256*points + 512*triangles + 128.
    if (!generation::checkedProduct(triangles, 3U, generation::kMaxWorkItemCount, corners) ||
        !mesh::appendMeshMergePlan(points, triangles, corners, 0U, plan) ||
        !generation::checkedProduct(points, 256U, limit, pointBytes) ||
        !generation::checkedProduct(triangles, 512U, limit, triangleBytes) ||
        !generation::checkedSum(bytes, pointBytes, limit, bytes) ||
        !generation::checkedSum(bytes, triangleBytes, limit, bytes)) return false;
    output = bytes;
    return true;
}

inline bool validTriangleWorkspaceStorage(std::size_t points, std::size_t triangles,
                                          std::size_t extraBytes = 0U)
{
    std::size_t bytes = 0U;
    return triangleWorkspaceBytes(points, triangles, bytes) &&
           generation::checkedSum(bytes, extraBytes, mesh::kMaxMeshOperationBytes, bytes);
}

struct TriangleComponent {
    std::vector<std::size_t> points;
    std::vector<std::size_t> triangles;
};

namespace component_detail {

struct PointLess {
    bool operator()(const Point3& a, const Point3& b) const
    {
        return a.x < b.x || (a.x == b.x &&
               (a.y < b.y || (a.y == b.y && a.z < b.z)));
    }
};

inline bool root(std::vector<std::size_t>& parents, std::size_t vertex,
                 std::size_t& result, generation::WorkBudget& budget)
{
    result = vertex;
    while (parents[result] != result) {
        if (!budget.consume(1U)) return false;
        result = parents[result];
    }
    while (parents[vertex] != vertex) {
        if (!budget.consume(1U)) return false;
        const std::size_t next = parents[vertex];
        parents[vertex] = result;
        vertex = next;
    }
    return budget.consume(1U);
}

inline bool join(std::vector<std::size_t>& parents, std::vector<std::size_t>& sizes,
                 std::size_t a, std::size_t b, generation::WorkBudget& budget)
{
    if (!root(parents, a, a, budget) || !root(parents, b, b, budget)) return false;
    if (a == b) return true;
    if (!budget.consume(2U)) return false;
    if (sizes[a] < sizes[b]) std::swap(a, b);
    parents[b] = a;
    sizes[a] += sizes[b]; // Each vertex belongs to exactly one admitted tree.
    return true;
}

} // namespace component_detail

// Components retain first-face order and source-relative point/face order.
// Exactly degenerate faces cannot join otherwise independent components.
// Optional exact-coordinate connectivity supports indexed triangle soups;
// proximity alone never joins unrelated components or inflates their frames.
inline bool triangleComponents(const std::vector<Point3>& points,
                               const std::vector<Triangle>& triangles,
                               std::vector<TriangleComponent>& output,
                               generation::WorkBudget& budget,
                               bool connectCoincidentPoints = false)
{
    if (points.empty() || triangles.empty() ||
        !validTriangleWorkspaceStorage(points.size(), triangles.size()) ||
        !budget.consumeProduct(points.size(), 5U) ||
        !budget.consumeProduct(triangles.size(), 32U)) return false;
    for (const Point3& point : points)
        if (!isFinite(point)) return false;
    const std::size_t absent = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> parents(points.size()), sizes(points.size(), 1U);
    std::vector<std::size_t> componentIds(points.size(), absent);
    std::vector<bool> used(points.size(), false), active(triangles.size(), false);
    for (std::size_t i = 0U; i < points.size(); ++i) parents[i] = i;
    std::map<Point3, std::size_t, component_detail::PointLess> coincident;
    for (std::size_t i = 0U; i < triangles.size(); ++i) {
        const Triangle& face = triangles[i];
        for (std::size_t corner = 0U; corner < 3U; ++corner)
            if (face[corner] >= points.size()) return false;
        if (triangleAreaSquared(points[face.first], points[face.second],
                                points[face.third]) == 0.0) continue;
        active[i] = true;
        if (!component_detail::join(parents, sizes, face.first, face.second, budget) ||
            !component_detail::join(parents, sizes, face.first, face.third, budget)) return false;
        for (std::size_t corner = 0U; corner < 3U; ++corner) {
            const std::size_t vertex = face[corner];
            if (used[vertex]) continue;
            used[vertex] = true;
            if (!connectCoincidentPoints) continue;
            if (!budget.consume(2U * generation::orderedContainerWork(coincident.size()) + 1U))
                return false;
            const auto inserted = coincident.insert(std::make_pair(points[vertex], vertex));
            if (!inserted.second &&
                !component_detail::join(parents, sizes, vertex, inserted.first->second, budget))
                return false;
        }
    }
    std::vector<TriangleComponent> candidate;
    for (std::size_t i = 0U; i < triangles.size(); ++i) {
        if (!active[i]) continue;
        std::size_t root = 0U;
        if (!component_detail::root(parents, triangles[i].first, root, budget) ||
            !budget.consume(1U)) return false;
        if (componentIds[root] == absent) {
            componentIds[root] = candidate.size();
            candidate.push_back(TriangleComponent());
        }
        candidate[componentIds[root]].triangles.push_back(i);
    }
    for (std::size_t i = 0U; i < points.size(); ++i) {
        if (!used[i]) continue;
        std::size_t root = 0U;
        if (!component_detail::root(parents, i, root, budget) ||
            !budget.consume(1U)) return false;
        candidate[componentIds[root]].points.push_back(i);
    }
    output.swap(candidate);
    return true;
}

namespace component_detail {

// Planner-only extraction: components were validated above and remap has one
// entry per source point. Every face references points in this component.
inline bool extractTriangleComponent(const std::vector<Point3>& points,
                                     const std::vector<Triangle>& triangles,
                                     const TriangleComponent& component,
                                     std::vector<std::size_t>& remap,
                                     std::vector<Point3>& localPoints,
                                     std::vector<Triangle>& localTriangles,
                                     generation::WorkBudget& budget)
{
    if (!budget.consumeProduct(component.points.size(), 2U) ||
        !budget.consumeProduct(component.triangles.size(), 4U)) return false;
    localPoints.clear();
    localTriangles.clear();
    localPoints.reserve(component.points.size());
    localTriangles.reserve(component.triangles.size());
    for (std::size_t vertex : component.points) {
        remap[vertex] = localPoints.size();
        localPoints.push_back(points[vertex]);
    }
    for (std::size_t index : component.triangles) {
        const Triangle& face = triangles[index];
        localTriangles.push_back(Triangle(remap[face.first], remap[face.second], remap[face.third]));
    }
    return true;
}

} // namespace component_detail

} // namespace geometry
} // namespace dommy
