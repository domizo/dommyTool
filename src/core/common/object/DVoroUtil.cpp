#include "DVoroUtil.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace voronoi {
namespace {

bool checkedAccumulate(std::size_t current,
                       std::size_t added,
                       std::size_t maximum,
                       std::size_t& result)
{
    if (current > maximum || added > maximum - current) return false;
    result = current + added;
    return true;
}

bool finitePoint(const MPoint& point)
{
    return std::isfinite(point.x) &&
           std::isfinite(point.y) &&
           std::isfinite(point.z) &&
           std::isfinite(point.w);
}

bool extractVertices(voro::voronoicell& cell,
                     const MPoint& seed,
                     double wallThickness,
                     std::vector<MPoint>& points)
{
    if (cell.p <= 0 ||
        !std::isfinite(wallThickness) ||
        wallThickness <= -1.0) {
        return false;
    }
    const double scale = 1.0 + wallThickness;
    const MPoint scaledSeed = seed * scale;
    if (!finitePoint(scaledSeed)) return false;

    std::vector<MPoint> candidate;
    candidate.reserve(static_cast<std::size_t>(cell.p));
    const double* coordinates = cell.pts;
    if (!coordinates) return false;
    for (int index = 0; index < cell.p;
         ++index, coordinates += 3) {
        const MPoint point(
            scaledSeed.x + coordinates[0] * 0.5,
            scaledSeed.y + coordinates[1] * 0.5,
            scaledSeed.z + coordinates[2] * 0.5);
        if (!finitePoint(point)) return false;
        candidate.push_back(point);
    }
    points.swap(candidate);
    return true;
}

bool extractFaces(voro::voronoicell& cell,
                  std::vector<int>& faceCounts,
                  std::vector<int>& connections)
{
    std::vector<int> encodedFaces;
    cell.face_vertices(encodedFaces);
    if (encodedFaces.empty()) return false;

    std::vector<int> candidateCounts;
    std::vector<int> candidateConnections;
    std::size_t cursor = 0U;
    while (cursor < encodedFaces.size()) {
        const int rawCount = encodedFaces[cursor++];
        if (rawCount < 3) return false;
        const std::size_t count =
            static_cast<std::size_t>(rawCount);
        if (count > encodedFaces.size() - cursor) return false;

        candidateCounts.push_back(rawCount);
        // The historical adapter reversed each face to match Maya winding.
        for (std::size_t corner = count; corner > 0U; --corner) {
            const int vertex = encodedFaces[cursor + corner - 1U];
            if (vertex < 0 || vertex >= cell.p) return false;
            candidateConnections.push_back(vertex);
        }
        cursor += count;
    }
    if (candidateCounts.empty()) return false;
    faceCounts.swap(candidateCounts);
    connections.swap(candidateConnections);
    return true;
}

bool createCellMesh(voro::voronoicell& cell,
                    const MPoint& seed,
                    double wallThickness,
                    MObject& mesh,
                    std::size_t& pointCount,
                    std::size_t& faceCount,
                    std::size_t& connectionCount)
{
    std::vector<MPoint> points;
    std::vector<int> faceCounts;
    std::vector<int> connections;
    if (!extractVertices(cell, seed, wallThickness, points) ||
        !extractFaces(cell, faceCounts, connections) ||
        points.size() >
            static_cast<std::size_t>(
                std::numeric_limits<int>::max()) ||
        faceCounts.size() >
            static_cast<std::size_t>(
                std::numeric_limits<int>::max())) {
        return false;
    }

    MStatus status;
    MPointArray mayaPoints;
    for (std::vector<MPoint>::const_iterator point = points.begin();
         point != points.end(); ++point) {
        status = mayaPoints.append(*point);
        if (!status) return false;
    }
    MIntArray mayaFaceCounts;
    for (std::vector<int>::const_iterator count = faceCounts.begin();
         count != faceCounts.end(); ++count) {
        status = mayaFaceCounts.append(*count);
        if (!status) return false;
    }
    MIntArray mayaConnections;
    for (std::vector<int>::const_iterator index =
             connections.begin();
         index != connections.end(); ++index) {
        status = mayaConnections.append(*index);
        if (!status) return false;
    }

    MFnMeshData meshData;
    const MObject candidate = meshData.create(&status);
    if (!status || candidate.isNull()) return false;
    MFnMesh meshFunction;
    meshFunction.create(
        static_cast<int>(points.size()),
        static_cast<int>(faceCounts.size()),
        mayaPoints, mayaFaceCounts, mayaConnections,
        candidate, &status);
    if (!status) return false;

    mesh = candidate;
    pointCount = points.size();
    faceCount = faceCounts.size();
    connectionCount = connections.size();
    return true;
}

} // namespace

bool addSeed(voro::container& container,
             const MPoint& point,
             int identifier,
             bool checkInside)
{
    if (!finitePoint(point) || identifier < 0) return false;
    if (checkInside &&
        !container.point_inside(point.x, point.y, point.z)) {
        return true;
    }
    container.put(identifier, point.x, point.y, point.z);
    return true;
}

bool extractCellMeshes(
    voro::container& container,
    double wallThickness,
    std::vector<MObject>& meshes,
    std::size_t maximumMeshCount,
    std::size_t maximumPointCount,
    std::size_t maximumFaceCount,
    std::size_t maximumConnectionCount)
{
    if (!std::isfinite(wallThickness) ||
        wallThickness <= -1.0 ||
        maximumMeshCount == 0U ||
        maximumPointCount == 0U ||
        maximumFaceCount == 0U ||
        maximumConnectionCount == 0U) {
        return false;
    }

    std::vector<MObject> candidate;
    std::size_t totalPoints = 0U;
    std::size_t totalFaces = 0U;
    std::size_t totalConnections = 0U;
    voro::c_loop_all loop(container);
    if (loop.start()) {
        do {
            if (candidate.size() >= maximumMeshCount) return false;
            voro::voronoicell cell;
            if (!container.compute_cell(cell, loop)) return false;
            const double* seedCoordinates =
                container.p[loop.ijk] + container.ps * loop.q;
            if (!seedCoordinates) return false;

            MObject mesh;
            std::size_t points = 0U;
            std::size_t faces = 0U;
            std::size_t connections = 0U;
            if (!createCellMesh(
                    cell,
                    MPoint(
                        seedCoordinates[0],
                        seedCoordinates[1],
                        seedCoordinates[2]),
                    wallThickness, mesh,
                    points, faces, connections) ||
                !checkedAccumulate(
                    totalPoints, points,
                    maximumPointCount, totalPoints) ||
                !checkedAccumulate(
                    totalFaces, faces,
                    maximumFaceCount, totalFaces) ||
                !checkedAccumulate(
                    totalConnections, connections,
                    maximumConnectionCount, totalConnections)) {
                return false;
            }
            candidate.push_back(mesh);
        } while (loop.inc());
    }
    meshes.swap(candidate);
    return true;
}

bool extractCellLines(
    voro::container& container,
    double wallThickness,
    std::vector<MPoint>& points,
    std::vector<int>& faceCounts,
    std::vector<int>& connections,
    std::size_t maximumPointCount,
    std::size_t maximumFaceCount,
    std::size_t maximumConnectionCount)
{
    if (!std::isfinite(wallThickness) ||
        wallThickness <= -1.0 ||
        maximumPointCount == 0U ||
        maximumFaceCount == 0U ||
        maximumConnectionCount == 0U) {
        return false;
    }

    std::vector<MPoint> candidatePoints;
    std::vector<int> candidateCounts;
    std::vector<int> candidateConnections;
    voro::c_loop_all loop(container);
    if (loop.start()) {
        do {
            voro::voronoicell cell;
            if (!container.compute_cell(cell, loop)) return false;
            const double* seedCoordinates =
                container.p[loop.ijk] + container.ps * loop.q;
            if (!seedCoordinates) return false;

            std::vector<MPoint> cellPoints;
            std::vector<int> cellCounts;
            std::vector<int> cellConnections;
            if (!extractVertices(
                    cell,
                    MPoint(
                        seedCoordinates[0],
                        seedCoordinates[1],
                        seedCoordinates[2]),
                    wallThickness, cellPoints) ||
                !extractFaces(
                    cell, cellCounts, cellConnections)) {
                return false;
            }

            std::size_t nextPointCount = 0U;
            std::size_t nextFaceCount = 0U;
            std::size_t nextConnectionCount = 0U;
            if (!checkedAccumulate(
                    candidatePoints.size(), cellPoints.size(),
                    maximumPointCount, nextPointCount) ||
                !checkedAccumulate(
                    candidateCounts.size(), cellCounts.size(),
                    maximumFaceCount, nextFaceCount) ||
                !checkedAccumulate(
                    candidateConnections.size(),
                    cellConnections.size(),
                    maximumConnectionCount,
                    nextConnectionCount) ||
                nextPointCount >
                    static_cast<std::size_t>(
                        std::numeric_limits<int>::max())) {
                return false;
            }

            const std::size_t pointOffset =
                candidatePoints.size();
            candidatePoints.insert(
                candidatePoints.end(),
                cellPoints.begin(), cellPoints.end());
            candidateCounts.insert(
                candidateCounts.end(),
                cellCounts.begin(), cellCounts.end());
            for (std::vector<int>::const_iterator index =
                     cellConnections.begin();
                 index != cellConnections.end(); ++index) {
                if (*index < 0 ||
                    pointOffset >
                        static_cast<std::size_t>(
                            std::numeric_limits<int>::max()) -
                            static_cast<std::size_t>(*index)) {
                    return false;
                }
                candidateConnections.push_back(
                    static_cast<int>(
                        pointOffset +
                        static_cast<std::size_t>(*index)));
            }
            if (candidateConnections.size() != nextConnectionCount) {
                return false;
            }
        } while (loop.inc());
    }

    points.swap(candidatePoints);
    faceCounts.swap(candidateCounts);
    connections.swap(candidateConnections);
    return true;
}

} // namespace voronoi
} // namespace dommy
