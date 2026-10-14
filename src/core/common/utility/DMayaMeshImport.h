#pragma once

#include "DMeshMergePlan.h"
#include "DStatusOperation.h"

#include <maya/MFnMesh.h>

namespace dommy {
namespace maya {

// Query only scalar counts before requesting native geometry arrays. Reuse the
// mesh operation staging estimate; this does not bound Maya's own DG caches,
// Boolean/meshing allocations, or the separate explicit-normal workspace.
inline MStatus preflightMeshImport(MFnMesh& meshFunction, mesh::MeshMergePlan& output)
{
    MStatus status;
    const int rawPoints = meshFunction.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawFaces = meshFunction.numPolygons(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawConnections = meshFunction.numFaceVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int rawUVs = meshFunction.numUVs(&status);
    DOMMY_RETURN_IF_FAILURE(status);

    std::size_t points = 0U, faces = 0U, connections = 0U, uvs = 0U;
    mesh::MeshMergePlan candidate;
    if (!generation::checkedCount(rawPoints, generation::kMaxGeneratedPointCount, points) ||
        !generation::checkedCount(rawFaces, generation::kMaxGeneratedFaceCount, faces) ||
        !generation::checkedCount(rawConnections, generation::kMaxWorkItemCount, connections) ||
        !generation::checkedCount(rawUVs, generation::kMaxWorkItemCount, uvs) ||
        points == 0U || faces == 0U || connections == 0U ||
        !mesh::appendMeshMergePlan(points, faces, connections, uvs, candidate) ||
        !mesh::validMeshOperationStorage(candidate)) return MS::kInvalidParameter;

    output = candidate;
    return MS::kSuccess;
}

} // namespace maya
} // namespace dommy
