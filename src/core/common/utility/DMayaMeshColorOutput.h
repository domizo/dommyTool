#pragma once

#include "DMeshMergePlan.h"
#include "DMayaValueValidation.h"
#include "DStatusOperation.h"
#include <maya/MColorArray.h>
#include <maya/MFnMesh.h>
#include <maya/MIntArray.h>
#include <maya/MStringArray.h>

namespace dommy {
namespace maya {

// Operate only on an unpublished detached mesh copy. Native writes are not
// atomic: callers must discard that copy on failure. Never select a current
// color set or change a downstream shape's viewport state from compute().
inline MStatus setMeshDataVertexColors(MFnMesh& mesh, const MColorArray& colors,
                                       const MString& requestedName)
{
    if (requestedName.length() == 0U) return MS::kInvalidParameter;
    MStatus status;
    const int points = mesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int faces = mesh.numPolygons(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int corners = mesh.numFaceVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    mesh::MeshMergePlan plan;
    if (points < 0 || faces < 0 || corners < 0 ||
        !mesh::appendMeshMergePlan(static_cast<std::size_t>(points),
            static_cast<std::size_t>(faces), static_cast<std::size_t>(corners), 0U, plan) ||
        !mesh::validMeshOperationStorage(plan) ||
        colors.length() != static_cast<unsigned int>(points)) return MS::kInvalidParameter;
    for (unsigned int i = 0U; i < colors.length(); ++i)
        if (!isFiniteColor(colors[i])) return MS::kInvalidParameter;

    // A color-table entry per vertex, referenced by every incident corner.
    // Query/admit scalar counts before asking Maya to allocate topology arrays.
    MIntArray faceCounts, colorIds;
    DOMMY_RETURN_IF_FAILURE(mesh.getVertices(faceCounts, colorIds));
    if (faceCounts.length() != static_cast<unsigned int>(faces) ||
        colorIds.length() != static_cast<unsigned int>(corners)) return MS::kFailure;
    std::size_t visited = 0U;
    for (unsigned int i = 0U; i < faceCounts.length(); ++i) {
        if (faceCounts[i] < 0 || !generation::checkedSum(visited,
                static_cast<std::size_t>(faceCounts[i]), plan.connections, visited))
            return MS::kFailure;
    }
    if (visited != plan.connections) return MS::kFailure;
    for (unsigned int i = 0U; i < colorIds.length(); ++i)
        if (colorIds[i] < 0 || colorIds[i] >= points) return MS::kFailure;

    MStringArray names;
    DOMMY_RETURN_IF_FAILURE(mesh.getColorSetNames(names));
    bool exists = false;
    for (unsigned int i = 0U; i < names.length(); ++i)
        if (names[i] == requestedName) { exists = true; break; }
    MString name(requestedName);
    if (!exists) {
        DOMMY_RETURN_IF_FAILURE(mesh.createColorSetDataMesh(name));
        if (!(name == requestedName)) return MS::kFailure;
    }
    DOMMY_RETURN_IF_FAILURE(mesh.clearColors(&name));
    DOMMY_RETURN_IF_FAILURE(mesh.setColors(colors, &name));
    return mesh.assignColors(colorIds, &name);
}

} // namespace maya
} // namespace dommy
