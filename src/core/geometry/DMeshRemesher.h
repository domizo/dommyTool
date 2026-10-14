#pragma once

#include "geometry/DGeometryTypes.h"
#include "geometry/DTriangleComponents.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace dommy {
namespace geometry {

struct TriangleMesh {
    std::vector<Point3> points;
    std::vector<Triangle> triangles;

    void clear()
    {
        points.clear();
        triangles.clear();
    }

    void swap(TriangleMesh& other)
    {
        points.swap(other.points);
        triangles.swap(other.triangles);
    }
};

namespace remesh_detail {

struct EdgeKey {
    EdgeKey() : first(0U), second(0U) {}
    EdgeKey(std::size_t firstIndex, std::size_t secondIndex)
        : first(std::min(firstIndex, secondIndex)),
          second(std::max(firstIndex, secondIndex)) {}

    bool operator<(const EdgeKey& rhs) const
    {
        return first < rhs.first ||
               (first == rhs.first && second < rhs.second);
    }

    std::size_t first;
    std::size_t second;
};

struct EdgeCandidate {
    EdgeCandidate() : edge(), squaredLength(0.0) {}
    EdgeCandidate(const EdgeKey& edgeValue, double lengthValue)
        : edge(edgeValue), squaredLength(lengthValue) {}

    bool operator<(const EdgeCandidate& rhs) const
    {
        return squaredLength < rhs.squaredLength ||
               (squaredLength == rhs.squaredLength &&
                edge < rhs.edge);
    }

    EdgeKey edge;
    double squaredLength;
};

struct FaceEdge {
    FaceEdge()
        : triangle(0U), start(0U), end(0U), opposite(0U) {}
    FaceEdge(std::size_t triangleIndex,
             std::size_t startIndex,
             std::size_t endIndex,
             std::size_t oppositeIndex)
        : triangle(triangleIndex),
          start(startIndex),
          end(endIndex),
          opposite(oppositeIndex) {}

    std::size_t triangle;
    std::size_t start;
    std::size_t end;
    std::size_t opposite;
};

inline bool validTriangle(const Triangle& triangle, std::size_t pointCount)
{
    return triangle.first < pointCount &&
           triangle.second < pointCount &&
           triangle.third < pointCount &&
           triangle.first != triangle.second &&
           triangle.second != triangle.third &&
           triangle.third != triangle.first;
}

inline Point3 triangleNormal(const Triangle& triangle,
                             const std::vector<Point3>& points)
{
    return cross(
        points[triangle.second] - points[triangle.first],
        points[triangle.third] - points[triangle.first]);
}

inline Point3 triangleNormalWithReplacement(
    const Triangle& triangle,
    const std::vector<Point3>& points,
    std::size_t replacedVertex,
    const Point3& replacement)
{
    const Point3 first =
        triangle.first == replacedVertex
            ? replacement : points[triangle.first];
    const Point3 second =
        triangle.second == replacedVertex
            ? replacement : points[triangle.second];
    const Point3 third =
        triangle.third == replacedVertex
            ? replacement : points[triangle.third];
    return cross(second - first, third - first);
}

inline bool preservesTriangleOrientation(const Point3& oldNormal,
                                         const Point3& newNormal,
                                         double areaTolerance)
{
    const double area = squaredLength(newNormal);
    const double alignment = dot(oldNormal, newNormal);
    return isFinite(oldNormal) && isFinite(newNormal) &&
           std::isfinite(area) && std::isfinite(alignment) &&
           area > areaTolerance * 4.0 && alignment > 0.0;
}

inline double triangleQuality(const Triangle& triangle,
                              const std::vector<Point3>& points)
{
    const Point3& first = points[triangle.first];
    const Point3& second = points[triangle.second];
    const Point3& third = points[triangle.third];
    const double denominator =
        squaredDistance(first, second) +
        squaredDistance(second, third) +
        squaredDistance(third, first);
    if (!std::isfinite(denominator) || denominator <= 0.0) return 0.0;
    const double twiceArea = length(cross(second - first, third - first));
    if (!std::isfinite(twiceArea)) return 0.0;
    return twiceArea / denominator;
}

inline bool buildEdgeCounts(
    const std::vector<Triangle>& triangles,
    std::map<EdgeKey, unsigned int>& edgeCounts,
    generation::WorkBudget& budget)
{
    edgeCounts.clear();
    for (std::vector<Triangle>::const_iterator triangle =
             triangles.begin(); triangle != triangles.end(); ++triangle) {
        for (std::size_t corner = 0U; corner < 3U; ++corner) {
            if (!budget.consume(1U + 2U * generation::orderedContainerWork(edgeCounts.size())))
                return false;
            unsigned int& count = edgeCounts[EdgeKey((*triangle)[corner], (*triangle)[(corner + 1U) % 3U])];
            if (count == std::numeric_limits<unsigned int>::max()) return false;
            ++count;
        }
    }
    return true;
}

inline bool compactReferencedPoints(TriangleMesh& mesh, generation::WorkBudget& budget)
{
    // Internal callers validate indices before this operation. Mask/remap
    // initialization, scanning/copying points and face indices are all charged.
    if (!budget.consumeProduct(mesh.points.size(), 5U) ||
        !budget.consumeProduct(mesh.triangles.size(), 6U)) return false;
    std::vector<bool> used(mesh.points.size(), false);
    for (const Triangle& triangle : mesh.triangles) {
        used[triangle.first] = true;
        used[triangle.second] = true;
        used[triangle.third] = true;
    }
    std::vector<std::size_t> remap(
        mesh.points.size(), std::numeric_limits<std::size_t>::max());
    std::size_t usedCount = 0U;
    for (bool referenced : used) if (referenced) ++usedCount;
    std::vector<Point3> compactPoints;
    compactPoints.reserve(usedCount);
    for (std::size_t index = 0U; index < mesh.points.size(); ++index) {
        if (!used[index]) continue;
        remap[index] = compactPoints.size();
        compactPoints.push_back(mesh.points[index]);
    }
    for (Triangle& triangle : mesh.triangles) {
        triangle.first = remap[triangle.first];
        triangle.second = remap[triangle.second];
        triangle.third = remap[triangle.third];
    }
    mesh.points.swap(compactPoints);
    return true;
}

inline bool normalizeMesh(TriangleMesh& mesh,
                          std::size_t maximumPointCount,
                          std::size_t maximumTriangleCount,
                          double areaTolerance,
                          generation::WorkBudget& budget)
{
    if (mesh.points.empty() ||
        mesh.points.size() > maximumPointCount ||
        mesh.triangles.empty() ||
        mesh.triangles.size() > maximumTriangleCount ||
        !std::isfinite(areaTolerance) || areaTolerance <= 0.0 || budget.exceeded()) {
        return false;
    }
    if (!budget.consume(mesh.points.size()) || !budget.consume(mesh.triangles.size())) return false;
    for (std::vector<Point3>::const_iterator point = mesh.points.begin();
         point != mesh.points.end(); ++point) {
        if (!isFinite(*point)) return false;
    }

    std::vector<Triangle> triangles;
    triangles.reserve(mesh.triangles.size());
    std::set<std::vector<std::size_t> > uniqueTriangles;
    for (std::vector<Triangle>::const_iterator triangle =
             mesh.triangles.begin(); triangle != mesh.triangles.end();
         ++triangle) {
        if (!budget.consume(15U + 2U * generation::orderedContainerWork(uniqueTriangles.size())))
            return false;
        if (!validTriangle(*triangle, mesh.points.size())) return false;
        const double areaSquared = triangleAreaSquared(
            mesh.points[triangle->first],
            mesh.points[triangle->second],
            mesh.points[triangle->third]);
        if (!std::isfinite(areaSquared)) return false;
        if (areaSquared <= areaTolerance) {
            continue;
        }
        std::vector<std::size_t> key;
        key.push_back(triangle->first);
        key.push_back(triangle->second);
        key.push_back(triangle->third);
        std::sort(key.begin(), key.end());
        if (uniqueTriangles.insert(key).second) {
            triangles.push_back(*triangle);
        }
    }
    if (triangles.empty()) return false;
    mesh.triangles.swap(triangles);

    return compactReferencedPoints(mesh, budget);
}

inline bool midpointForEdge(
    const EdgeKey& edge,
    std::vector<Point3>& points,
    std::map<EdgeKey, std::size_t>& midpoints,
    std::size_t maximumPointCount,
    std::size_t& index,
    generation::WorkBudget& budget)
{
    if (!budget.consume(2U * generation::orderedContainerWork(midpoints.size()))) return false;
    const std::map<EdgeKey, std::size_t>::const_iterator existing =
        midpoints.find(edge);
    if (existing != midpoints.end()) {
        index = existing->second;
        return true;
    }
    if (points.size() >= maximumPointCount) return false;
    const Point3 midpoint =
        (points[edge.first] + points[edge.second]) * 0.5;
    if (!isFinite(midpoint)) return false;
    if (!budget.consume(2U + 2U * generation::orderedContainerWork(midpoints.size()))) return false;
    index = points.size();
    points.push_back(midpoint);
    midpoints.insert(std::make_pair(edge, index));
    return true;
}

inline bool splitLongEdges(TriangleMesh& mesh,
                           double splitLength,
                           std::size_t maximumPointCount,
                           std::size_t maximumTriangleCount,
                           generation::WorkBudget& budget,
                           std::size_t extraStorageBytes = 0U)
{
    const double splitSquared = splitLength * splitLength;
    if (!std::isfinite(splitSquared) || splitSquared <= 0.0) return false;

    std::set<EdgeKey> splitEdges;
    for (std::vector<Triangle>::const_iterator triangle =
             mesh.triangles.begin(); triangle != mesh.triangles.end();
         ++triangle) {
        const EdgeKey edges[3] = {
            EdgeKey(triangle->first, triangle->second),
            EdgeKey(triangle->second, triangle->third),
            EdgeKey(triangle->third, triangle->first)
        };
        for (std::size_t edge = 0U; edge < 3U; ++edge) {
            if (!budget.consume(4U)) return false;
            if (squaredDistance(
                    mesh.points[edges[edge].first],
                    mesh.points[edges[edge].second]) > splitSquared) {
                if (!budget.consume(2U * generation::orderedContainerWork(splitEdges.size()))) return false;
                splitEdges.insert(edges[edge]);
            }
        }
    }
    if (splitEdges.empty()) return true;

    // Reserve admission for the worst case before growing any geometry array.
    std::size_t plannedPoints = 0U, plannedTriangles = 0U;
    if (!generation::checkedSum(mesh.points.size(), splitEdges.size(),
                                generation::kMaxGeneratedPointCount, plannedPoints) ||
        !generation::checkedProduct(mesh.triangles.size(), 4U,
                                    generation::kMaxGeneratedFaceCount, plannedTriangles) ||
        !validTriangleWorkspaceStorage(plannedPoints, plannedTriangles, extraStorageBytes)) return false;

    std::map<EdgeKey, std::size_t> midpoints;
    std::vector<Triangle> triangles;
    if (!budget.consume(mesh.triangles.size())) return false;
    triangles.reserve(mesh.triangles.size());
    for (std::vector<Triangle>::const_iterator triangle =
             mesh.triangles.begin(); triangle != mesh.triangles.end();
         ++triangle) {
        if (!budget.consume(3U) ||
            !budget.consumeProduct(3U, 2U * generation::orderedContainerWork(splitEdges.size())))
            return false;
        const std::size_t a = triangle->first;
        const std::size_t b = triangle->second;
        const std::size_t c = triangle->third;
        const EdgeKey ab(a, b);
        const EdgeKey bc(b, c);
        const EdgeKey ca(c, a);
        const bool splitAB = splitEdges.find(ab) != splitEdges.end();
        const bool splitBC = splitEdges.find(bc) != splitEdges.end();
        const bool splitCA = splitEdges.find(ca) != splitEdges.end();
        const unsigned int splitCount =
            static_cast<unsigned int>(splitAB) +
            static_cast<unsigned int>(splitBC) +
            static_cast<unsigned int>(splitCA);
        std::size_t mab = 0U;
        std::size_t mbc = 0U;
        std::size_t mca = 0U;
        if ((splitAB && !midpointForEdge(
                ab, mesh.points, midpoints, maximumPointCount, mab, budget)) ||
            (splitBC && !midpointForEdge(
                bc, mesh.points, midpoints, maximumPointCount, mbc, budget)) ||
            (splitCA && !midpointForEdge(
                ca, mesh.points, midpoints, maximumPointCount, mca, budget))) {
            return false;
        }

        const std::size_t addedCount =
            splitCount == 0U ? 1U :
            (splitCount == 1U ? 2U :
             (splitCount == 2U ? 3U : 4U));
        if (!budget.consume(addedCount) || addedCount > maximumTriangleCount ||
            triangles.size() >
                maximumTriangleCount - addedCount) {
            return false;
        }

        if (splitCount == 0U) {
            triangles.push_back(*triangle);
        } else if (splitCount == 1U) {
            if (splitAB) {
                triangles.push_back(Triangle(a, mab, c));
                triangles.push_back(Triangle(mab, b, c));
            } else if (splitBC) {
                triangles.push_back(Triangle(b, mbc, a));
                triangles.push_back(Triangle(mbc, c, a));
            } else {
                triangles.push_back(Triangle(c, mca, b));
                triangles.push_back(Triangle(mca, a, b));
            }
        } else if (splitCount == 2U) {
            if (splitAB && splitBC) {
                triangles.push_back(Triangle(b, mbc, mab));
                triangles.push_back(Triangle(mab, mbc, c));
                triangles.push_back(Triangle(mab, c, a));
            } else if (splitBC && splitCA) {
                triangles.push_back(Triangle(c, mca, mbc));
                triangles.push_back(Triangle(mbc, mca, a));
                triangles.push_back(Triangle(mbc, a, b));
            } else {
                triangles.push_back(Triangle(a, mab, mca));
                triangles.push_back(Triangle(mca, mab, b));
                triangles.push_back(Triangle(mca, b, c));
            }
        } else {
            triangles.push_back(Triangle(a, mab, mca));
            triangles.push_back(Triangle(mab, b, mbc));
            triangles.push_back(Triangle(mca, mbc, c));
            triangles.push_back(Triangle(mab, mbc, mca));
        }
    }
    mesh.triangles.swap(triangles);
    return true;
}

inline bool collapseShortEdges(TriangleMesh& mesh,
                               double collapseLength,
                               std::size_t maximumPointCount,
                               std::size_t maximumTriangleCount,
                               double areaTolerance,
                               generation::WorkBudget& budget)
{
    const double collapseSquared = collapseLength * collapseLength;
    if (!std::isfinite(collapseSquared) || collapseSquared <= 0.0 ||
        !std::isfinite(areaTolerance) || areaTolerance <= 0.0 ||
        mesh.points.size() > maximumPointCount ||
        mesh.triangles.size() > maximumTriangleCount) {
        return false;
    }
    // Only faces made degenerate by this operation may be removed below.
    // Keep malformed input rejection separate from intentional edge removal.
    if (!budget.consume(mesh.points.size()) || !budget.consume(mesh.triangles.size())) return false;
    for (const Point3& point : mesh.points)
        if (!isFinite(point)) return false;
    for (const Triangle& triangle : mesh.triangles)
        if (!validTriangle(triangle, mesh.points.size())) return false;

    std::map<EdgeKey, unsigned int> edgeCounts;
    if (!buildEdgeCounts(mesh.triangles, edgeCounts, budget) ||
        !budget.consumeProduct(mesh.points.size(), 3U)) return false;
    std::vector<bool> boundary(mesh.points.size(), false);
    std::vector<std::set<std::size_t> > neighbors(mesh.points.size());
    std::vector<std::vector<std::size_t> > incident(mesh.points.size());
    for (std::vector<Triangle>::const_iterator triangle =
             mesh.triangles.begin(); triangle != mesh.triangles.end();
         ++triangle) {
        const std::size_t triangleIndex =
            static_cast<std::size_t>(triangle - mesh.triangles.begin());
        for (std::size_t corner = 0U; corner < 3U; ++corner) {
            const std::size_t vertex = (*triangle)[corner];
            const std::size_t next = (*triangle)[(corner + 1U) % 3U];
            if (!budget.consume(1U + 2U * generation::orderedContainerWork(neighbors[vertex].size()) +
                                   2U * generation::orderedContainerWork(neighbors[next].size()))) return false;
            incident[vertex].push_back(triangleIndex);
            neighbors[vertex].insert(next);
            neighbors[next].insert(vertex);
        }
    }
    std::vector<EdgeCandidate> candidates;
    for (std::map<EdgeKey, unsigned int>::const_iterator edge =
             edgeCounts.begin(); edge != edgeCounts.end(); ++edge) {
        if (!budget.consume(4U)) return false;
        if (edge->second != 2U) {
            boundary[edge->first.first] = true;
            boundary[edge->first.second] = true;
        } else if (edge->second == 2U) {
            const double edgeLength = squaredDistance(
                mesh.points[edge->first.first],
                mesh.points[edge->first.second]);
            if (edgeLength < collapseSquared) {
                candidates.push_back(
                    EdgeCandidate(edge->first, edgeLength));
            }
        }
    }
    if (candidates.empty()) return true;
    if (!budget.consumeProduct(candidates.size(),
            2U * generation::orderedContainerWork(candidates.size())) ||
        !budget.consumeProduct(mesh.points.size(), 3U)) return false;
    std::sort(candidates.begin(), candidates.end());

    std::vector<std::size_t> representative(mesh.points.size());
    for (std::size_t index = 0U; index < representative.size(); ++index) {
        representative[index] = index;
    }
    std::vector<Point3> updatedPoints = mesh.points;
    std::vector<bool> locked(mesh.points.size(), false);

    for (std::vector<EdgeCandidate>::const_iterator candidate =
             candidates.begin(); candidate != candidates.end(); ++candidate) {
        if (!budget.consume(1U)) return false;
        const std::size_t first = candidate->edge.first;
        const std::size_t second = candidate->edge.second;
        if (boundary[first] || boundary[second] ||
            locked[first] || locked[second]) {
            continue;
        }
        // An interior manifold edge must have exactly its two opposite
        // vertices in common. Otherwise contraction can join unrelated fans.
        std::size_t commonNeighbors = 0U;
        for (const std::size_t neighbor : neighbors[first]) {
            if (!budget.consume(1U + 2U * generation::orderedContainerWork(neighbors[second].size())))
                return false;
            if (neighbors[second].count(neighbor) != 0U) ++commonNeighbors;
        }
        if (commonNeighbors != 2U) continue;
        const Point3 midpoint =
            (mesh.points[first] + mesh.points[second]) * 0.5;
        if (!isFinite(midpoint)) continue;

        std::set<std::size_t> localTriangles;
        for (std::size_t vertex : {first, second}) {
            for (std::size_t triangle : incident[vertex]) {
                if (!budget.consume(1U + 2U * generation::orderedContainerWork(localTriangles.size())))
                    return false;
                localTriangles.insert(triangle);
            }
        }
        bool valid = true;
        std::set<std::vector<std::size_t> > remainingFaces;
        for (std::set<std::size_t>::const_iterator triangleIndex =
                 localTriangles.begin();
             triangleIndex != localTriangles.end(); ++triangleIndex) {
            if (!budget.consume(15U + 2U * generation::orderedContainerWork(remainingFaces.size())))
                return false;
            const Triangle oldTriangle = mesh.triangles[*triangleIndex];
            Triangle newTriangle = oldTriangle;
            for (std::size_t corner = 0U; corner < 3U; ++corner) {
                if (newTriangle[corner] == second) {
                    newTriangle[corner] = first;
                }
            }
            if (newTriangle.first == newTriangle.second ||
                newTriangle.second == newTriangle.third ||
                newTriangle.third == newTriangle.first) {
                continue;
            }
            std::vector<std::size_t> key = {
                newTriangle.first, newTriangle.second, newTriangle.third};
            std::sort(key.begin(), key.end());
            // In particular, do not collapse a tetrahedron into two copies of
            // one face and let normalizeMesh silently discard a duplicate.
            if (!remainingFaces.insert(key).second) {
                valid = false;
                break;
            }
            const Point3 oldNormal =
                triangleNormal(oldTriangle, mesh.points);
            const Point3 newNormal = triangleNormalWithReplacement(
                newTriangle, mesh.points, first, midpoint);
            if (!preservesTriangleOrientation(oldNormal, newNormal, areaTolerance)) {
                valid = false;
                break;
            }
        }
        if (!valid) continue;

        representative[second] = first;
        updatedPoints[first] = midpoint;
        locked[first] = true;
        locked[second] = true;
        for (std::set<std::size_t>::const_iterator neighbor =
                 neighbors[first].begin();
             neighbor != neighbors[first].end(); ++neighbor) {
            if (!budget.consume(1U)) return false;
            locked[*neighbor] = true;
        }
        for (std::set<std::size_t>::const_iterator neighbor =
                 neighbors[second].begin();
             neighbor != neighbors[second].end(); ++neighbor) {
            if (!budget.consume(1U)) return false;
            locked[*neighbor] = true;
        }
    }

    if (!budget.consume(representative.size())) return false;
    bool changed = false;
    for (std::size_t index = 0U; index < representative.size(); ++index) {
        if (representative[index] != index) {
            changed = true;
            break;
        }
    }
    if (!changed) return true;
    TriangleMesh collapsed;
    if (!budget.consumeProduct(mesh.triangles.size(), 4U)) return false;
    collapsed.points.swap(updatedPoints);
    collapsed.triangles.reserve(mesh.triangles.size());
    for (const Triangle& triangle : mesh.triangles) {
        const Triangle mapped(representative[triangle.first],
                              representative[triangle.second],
                              representative[triangle.third]);
        // These are precisely the incident faces removed by the accepted
        // contractions. normalizeMesh must still reject malformed input.
        if (mapped.first == mapped.second || mapped.second == mapped.third ||
            mapped.third == mapped.first) continue;
        collapsed.triangles.push_back(mapped);
    }
    if (!normalizeMesh(collapsed, maximumPointCount, maximumTriangleCount, areaTolerance, budget))
        return false;
    mesh.swap(collapsed);
    return true;
}

inline bool buildFaceEdges(
    const std::vector<Triangle>& triangles,
    std::map<EdgeKey, std::vector<FaceEdge> >& edges,
    generation::WorkBudget& budget)
{
    edges.clear();
    for (std::size_t triangleIndex = 0U;
         triangleIndex < triangles.size(); ++triangleIndex) {
        const Triangle& triangle = triangles[triangleIndex];
        for (std::size_t corner = 0U; corner < 3U; ++corner) {
            if (!budget.consume(1U + 2U * generation::orderedContainerWork(edges.size()))) return false;
            edges[EdgeKey(triangle[corner], triangle[(corner + 1U) % 3U])].push_back(
                FaceEdge(triangleIndex, triangle[corner], triangle[(corner + 1U) % 3U],
                         triangle[(corner + 2U) % 3U]));
        }
    }
    return true;
}

inline bool flipEdges(TriangleMesh& mesh, double areaTolerance, generation::WorkBudget& budget)
{
    std::map<EdgeKey, std::vector<FaceEdge> > edges;
    if (!buildFaceEdges(mesh.triangles, edges, budget) ||
        !budget.consume(mesh.triangles.size()) || !budget.consume(mesh.points.size())) return false;
    std::vector<bool> lockedTriangles(mesh.triangles.size(), false);
    std::vector<bool> lockedVertices(mesh.points.size(), false);

    for (std::map<EdgeKey, std::vector<FaceEdge> >::const_iterator edge =
             edges.begin(); edge != edges.end(); ++edge) {
        if (!budget.consume(1U)) return false;
        if (edge->second.size() != 2U) continue;
        FaceEdge first = edge->second[0U];
        FaceEdge second = edge->second[1U];
        if (first.start != second.end || first.end != second.start) {
            continue;
        }

        const std::size_t u = first.start;
        const std::size_t v = first.end;
        const std::size_t a = first.opposite;
        const std::size_t b = second.opposite;
        if (a == b || a == u || a == v || b == u || b == v ||
            lockedTriangles[first.triangle] ||
            lockedTriangles[second.triangle] ||
            lockedVertices[u] || lockedVertices[v] ||
            lockedVertices[a] || lockedVertices[b]) {
            continue;
        }

        // A replacement diagonal already used elsewhere would create duplicate
        // faces or a nonmanifold edge. Vertex locking keeps accepted flips
        // disjoint, so the initial edge map is sufficient for this pass.
        if (!budget.consume(2U * generation::orderedContainerWork(edges.size()))) return false;
        if (edges.find(EdgeKey(a, b)) != edges.end()) continue;

        if (!budget.consume(32U)) return false;

        const Triangle newFirst(a, b, v);
        const Triangle newSecond(b, a, u);
        const double oldQuality = std::min(
            triangleQuality(mesh.triangles[first.triangle], mesh.points),
            triangleQuality(mesh.triangles[second.triangle], mesh.points));
        const double newQuality = std::min(
            triangleQuality(newFirst, mesh.points),
            triangleQuality(newSecond, mesh.points));
        if (!std::isfinite(newQuality) ||
            newQuality <= oldQuality + 1.0e-12) {
            continue;
        }

        const Point3 oldFirstNormal = triangleNormal(
            mesh.triangles[first.triangle], mesh.points);
        const Point3 oldSecondNormal = triangleNormal(
            mesh.triangles[second.triangle], mesh.points);
        const Point3 newFirstNormal =
            triangleNormal(newFirst, mesh.points);
        const Point3 newSecondNormal =
            triangleNormal(newSecond, mesh.points);
        const Point3 oldNormal = oldFirstNormal + oldSecondNormal;
        if (!preservesTriangleOrientation(oldNormal, newFirstNormal, areaTolerance) ||
            !preservesTriangleOrientation(oldNormal, newSecondNormal, areaTolerance)) {
            continue;
        }

        mesh.triangles[first.triangle] = newFirst;
        mesh.triangles[second.triangle] = newSecond;
        lockedTriangles[first.triangle] = true;
        lockedTriangles[second.triangle] = true;
        lockedVertices[u] = true;
        lockedVertices[v] = true;
        lockedVertices[a] = true;
        lockedVertices[b] = true;
    }
    return true;
}

inline bool smoothTangentially(TriangleMesh& mesh,
                               double relaxation,
                               double areaTolerance,
                               generation::WorkBudget& budget)
{
    if (!std::isfinite(areaTolerance) || areaTolerance <= 0.0 ||
        !std::isfinite(relaxation) ||
        relaxation <= 0.0 || relaxation > 1.0) {
        return false;
    }
    std::map<EdgeKey, unsigned int> edgeCounts;
    if (!buildEdgeCounts(mesh.triangles, edgeCounts, budget) ||
        !budget.consumeProduct(mesh.points.size(), 4U)) return false;
    std::vector<bool> boundary(mesh.points.size(), false);
    std::vector<std::set<std::size_t> > neighbors(mesh.points.size());
    std::vector<std::vector<std::size_t> > incident(mesh.points.size());
    std::vector<Point3> normals(mesh.points.size());
    for (std::size_t triangleIndex = 0U;
         triangleIndex < mesh.triangles.size(); ++triangleIndex) {
        const Triangle& triangle = mesh.triangles[triangleIndex];
        if (!budget.consume(4U)) return false;
        const Point3 normal = triangleNormal(triangle, mesh.points);
        for (std::size_t corner = 0U; corner < 3U; ++corner) {
            const std::size_t vertex = triangle[corner];
            const std::size_t next = triangle[(corner + 1U) % 3U];
            if (!budget.consume(2U + 2U * generation::orderedContainerWork(neighbors[vertex].size()) +
                                   2U * generation::orderedContainerWork(neighbors[next].size()))) return false;
            neighbors[vertex].insert(next);
            neighbors[next].insert(vertex);
            incident[vertex].push_back(triangleIndex);
            normals[vertex] += normal;
        }
    }
    for (std::map<EdgeKey, unsigned int>::const_iterator edge =
             edgeCounts.begin(); edge != edgeCounts.end(); ++edge) {
        if (!budget.consume(1U)) return false;
        if (edge->second == 1U) {
            boundary[edge->first.first] = true;
            boundary[edge->first.second] = true;
        }
    }

    if (!budget.consume(mesh.points.size())) return false;
    std::vector<Point3> next = mesh.points;
    for (std::size_t vertex = 0U; vertex < mesh.points.size(); ++vertex) {
        if (!budget.consume(1U)) return false;
        if (boundary[vertex] || neighbors[vertex].empty()) continue;
        Point3 normal;
        if (!normalized(normals[vertex], normal)) continue;
        Point3 average;
        if (!budget.consume(neighbors[vertex].size())) return false;
        for (std::set<std::size_t>::const_iterator neighbor =
                 neighbors[vertex].begin();
             neighbor != neighbors[vertex].end(); ++neighbor) {
            average += mesh.points[*neighbor];
        }
        average = average /
            static_cast<double>(neighbors[vertex].size());
        Point3 delta = average - mesh.points[vertex];
        delta -= normal * dot(delta, normal);
        const Point3 candidate =
            mesh.points[vertex] + delta * relaxation;
        if (!isFinite(candidate)) continue;

        bool valid = true;
        for (std::vector<std::size_t>::const_iterator triangleIndex =
                 incident[vertex].begin();
             triangleIndex != incident[vertex].end(); ++triangleIndex) {
            if (!budget.consume(8U)) return false;
            const Triangle& triangle = mesh.triangles[*triangleIndex];
            const Point3 oldNormal = triangleNormal(triangle, mesh.points);
            const Point3 newNormal = triangleNormalWithReplacement(
                triangle, mesh.points, vertex, candidate);
            if (!preservesTriangleOrientation(oldNormal, newNormal, areaTolerance)) {
                valid = false;
                break;
            }
        }
        if (valid) next[vertex] = candidate;
    }
    // Individually safe moves need not be safe when neighboring vertices move
    // together. Validate the complete batch, preserving the full step when it
    // is safe. Bounded backtracking never commits a flipped/degenerate face.
    const unsigned int maximumBacktrackingSteps = 32U;
    for (unsigned int step = 0U; step <= maximumBacktrackingSteps; ++step) {
        bool valid = true;
        for (const Triangle& triangle : mesh.triangles) {
            if (!budget.consume(8U)) return false;
            if (!preservesTriangleOrientation(
                    triangleNormal(triangle, mesh.points),
                    triangleNormal(triangle, next), areaTolerance)) {
                valid = false;
                break;
            }
        }
        if (valid) {
            mesh.points.swap(next);
            return true;
        }
        if (step == maximumBacktrackingSteps) break;
        if (!budget.consume(next.size())) return false;
        for (std::size_t vertex = 0U; vertex < next.size(); ++vertex)
            next[vertex] = mesh.points[vertex] +
                (next[vertex] - mesh.points[vertex]) * 0.5;
    }
    // A rejected smoothing proposal is a quality no-op, not invalid geometry.
    return true;
}

// Standalone compatibility entry points create one bounded budget. The
// production remeshing pipeline always passes its evaluation-wide instance.
inline bool buildEdgeCounts(const std::vector<Triangle>& triangles,
                            std::map<EdgeKey, unsigned int>& edges)
{
    generation::WorkBudget budget;
    return buildEdgeCounts(triangles, edges, budget);
}

inline bool normalizeMesh(TriangleMesh& mesh, std::size_t maximumPointCount,
                          std::size_t maximumTriangleCount, double areaTolerance)
{
    generation::WorkBudget budget;
    return normalizeMesh(mesh, maximumPointCount, maximumTriangleCount, areaTolerance, budget);
}

inline bool splitLongEdges(TriangleMesh& mesh, double splitLength,
                          std::size_t maximumPointCount, std::size_t maximumTriangleCount)
{
    generation::WorkBudget budget;
    return splitLongEdges(mesh, splitLength, maximumPointCount, maximumTriangleCount, budget);
}

inline bool collapseShortEdges(TriangleMesh& mesh, double collapseLength,
                              std::size_t maximumPointCount, std::size_t maximumTriangleCount,
                              double areaTolerance)
{
    generation::WorkBudget budget;
    return collapseShortEdges(mesh, collapseLength, maximumPointCount, maximumTriangleCount,
                              areaTolerance, budget);
}

inline bool flipEdges(TriangleMesh& mesh, double areaTolerance)
{
    generation::WorkBudget budget;
    return flipEdges(mesh, areaTolerance, budget);
}

inline bool smoothTangentially(TriangleMesh& mesh, double relaxation, double areaTolerance)
{
    generation::WorkBudget budget;
    return smoothTangentially(mesh, relaxation, areaTolerance, budget);
}

} // namespace remesh_detail

inline bool isotropicRemesh(
    const TriangleMesh& input,
    double targetEdgeLength,
    unsigned int iterations,
    TriangleMesh& output,
    std::size_t maximumPointCount,
    std::size_t maximumTriangleCount,
    generation::WorkBudget& budget)
{
    if (!std::isfinite(targetEdgeLength) ||
        targetEdgeLength <= 0.0 || iterations == 0U ||
        maximumPointCount < 3U || maximumTriangleCount == 0U ||
        budget.exceeded() || input.points.size() < 3U ||
        input.points.size() > maximumPointCount ||
        input.triangles.empty() ||
        input.triangles.size() > maximumTriangleCount) {
        return false;
    }
    std::vector<TriangleComponent> components;
    std::size_t sourceStorageBytes = 0U, triangleBytes = 0U;
    if (!generation::checkedProduct(input.points.size(), 64U,
                                    mesh::kMaxMeshOperationBytes, sourceStorageBytes) ||
        !generation::checkedProduct(input.triangles.size(), 64U,
                                    mesh::kMaxMeshOperationBytes, triangleBytes) ||
        !generation::checkedSum(sourceStorageBytes, triangleBytes,
                                mesh::kMaxMeshOperationBytes, sourceStorageBytes) ||
        !validTriangleWorkspaceStorage(input.points.size(), input.triangles.size(), sourceStorageBytes))
        return false;
    if (!budget.consumeProduct(input.triangles.size(), 3U)) return false;
    for (const Triangle& face : input.triangles)
        if (!remesh_detail::validTriangle(face, input.points.size())) return false;
    if (!triangleComponents(input.points, input.triangles, components, budget) ||
        components.empty() || !budget.consume(input.points.size())) return false;
    std::vector<std::size_t> remap(input.points.size());
    TriangleMesh aggregate;
    for (const TriangleComponent& component : components) {
        std::size_t aggregateStorageBytes = 0U, extraStorageBytes = sourceStorageBytes;
        if (!triangleWorkspaceBytes(aggregate.points.size(), aggregate.triangles.size(), aggregateStorageBytes) ||
            !generation::checkedSum(extraStorageBytes, aggregateStorageBytes,
                                    mesh::kMaxMeshOperationBytes, extraStorageBytes) ||
            !validTriangleWorkspaceStorage(component.points.size(), component.triangles.size(), extraStorageBytes))
            return false;
        TriangleMesh candidate;
        if (!component_detail::extractTriangleComponent(input.points, input.triangles, component, remap,
                                      candidate.points, candidate.triangles, budget)) return false;
        Point3 origin;
        double scale = 0.0;
        if (!budget.consume(1U) ||
            !referencedTriangleFrame(candidate.points, candidate.triangles, origin, scale, budget))
            return false;
        if (!remesh_detail::compactReferencedPoints(candidate, budget)) return false;
        const double tolerance = scaledTolerance(scale, 1.0e-12);
        if (!std::isfinite(tolerance)) return false;
        // All edge/normal/smoothing arithmetic uses a local frame. Restore only
        // after the complete operation succeeds; output remains transactional.
        if (!budget.consume(candidate.points.size())) return false;
        for (Point3& point : candidate.points) {
            point -= origin;
            if (!isFinite(point)) return false;
        }
        const double areaTolerance =
            tolerance * tolerance * tolerance * tolerance;
        if (!std::isfinite(areaTolerance) ||
            !remesh_detail::normalizeMesh(
                candidate, maximumPointCount, maximumTriangleCount,
                areaTolerance, budget)) {
            return false;
        }

        for (unsigned int iteration = 0U; iteration < iterations; ++iteration) {
            if (!budget.consume(1U)) return false;
            if (!remesh_detail::splitLongEdges(
                    candidate, targetEdgeLength * (4.0 / 3.0),
                    maximumPointCount - aggregate.points.size(),
                    maximumTriangleCount - aggregate.triangles.size(), budget, extraStorageBytes) ||
                !remesh_detail::collapseShortEdges(
                    candidate, targetEdgeLength * 0.8,
                    maximumPointCount - aggregate.points.size(),
                    maximumTriangleCount - aggregate.triangles.size(),
                    areaTolerance, budget) ||
                !remesh_detail::flipEdges(candidate, areaTolerance, budget) ||
                !remesh_detail::smoothTangentially(
                    candidate, 0.35, areaTolerance, budget) ||
                !remesh_detail::normalizeMesh(
                    candidate, maximumPointCount, maximumTriangleCount,
                    areaTolerance, budget)) {
                return false;
            }
        }
        if (!budget.consume(candidate.points.size())) return false;
        for (Point3& point : candidate.points) {
            point += origin;
            if (!isFinite(point)) return false;
        }
        std::size_t nextPoints = 0U, nextTriangles = 0U;
        if (!generation::checkedSum(aggregate.points.size(), candidate.points.size(),
                                    maximumPointCount, nextPoints) ||
            !generation::checkedSum(aggregate.triangles.size(), candidate.triangles.size(),
                                    maximumTriangleCount, nextTriangles) ||
            !validTriangleWorkspaceStorage(nextPoints, nextTriangles, sourceStorageBytes) ||
            // Include amortized vector-growth copies as well as new entries.
            !budget.consumeProduct(candidate.points.size(), 3U) ||
            !budget.consumeProduct(candidate.triangles.size(), 6U)) return false;
        const std::size_t offset = aggregate.points.size();
        aggregate.points.insert(aggregate.points.end(), candidate.points.begin(), candidate.points.end());
        for (const Triangle& face : candidate.triangles)
            aggregate.triangles.push_back(Triangle(offset + face.first, offset + face.second, offset + face.third));
    }
    output.swap(aggregate);
    return true;
}

inline bool isotropicRemesh(const TriangleMesh& input, double targetEdgeLength,
                            unsigned int iterations, TriangleMesh& output,
                            std::size_t maximumPointCount, std::size_t maximumTriangleCount,
                            std::size_t maximumWork = generation::kMaxWorkItemCount)
{
    generation::WorkBudget budget(maximumWork);
    return isotropicRemesh(input, targetEdgeLength, iterations, output,
                           maximumPointCount, maximumTriangleCount, budget);
}

} // namespace geometry
} // namespace dommy
