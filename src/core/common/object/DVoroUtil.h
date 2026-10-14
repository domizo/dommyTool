#pragma once

#include "DMayaHeaders.h"
#include "voro++.hh"

#include <cstddef>
#include <vector>

namespace dommy {
namespace voronoi {

// Adds a seed when it is inside the container. A seed outside the container is
// intentionally ignored when checkInside is true.
bool addSeed(voro::container& container,
             const MPoint& point,
             int identifier,
             bool checkInside);

// Extracts complete cell meshes transactionally. The output is replaced only
// after every Voro++ cell and Maya mesh has passed the supplied budgets.
bool extractCellMeshes(
    voro::container& container,
    double wallThickness,
    std::vector<MObject>& meshes,
    std::size_t maximumMeshCount,
    std::size_t maximumPointCount,
    std::size_t maximumFaceCount,
    std::size_t maximumConnectionCount);

// Extracts one polygonal line per cell face. Connections index the returned
// point array and are validated before the output vectors are committed.
bool extractCellLines(
    voro::container& container,
    double wallThickness,
    std::vector<MPoint>& points,
    std::vector<int>& faceCounts,
    std::vector<int>& connections,
    std::size_t maximumPointCount,
    std::size_t maximumFaceCount,
    std::size_t maximumConnectionCount);

} // namespace voronoi
} // namespace dommy
