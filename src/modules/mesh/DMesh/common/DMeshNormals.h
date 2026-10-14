#pragma once

#include "DMeshData.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DStatusOperation.h"
#include <maya/MFnMesh.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MVectorArray.h>

namespace dommy {
namespace mesh {

// Reserved face-corner metadata. Zero means Maya should compute that corner's
// normal (needed when merging explicit-normal and ordinary meshes).
inline const char* cornerNormalAttribute() { return "__dommy_corner_normal"; }

inline bool hasCornerNormals(const DMeshInternalData& mesh) {
    return mesh.isExistCompAttr(D_COMP_PER_VTX, cornerNormalAttribute());
}

inline MStatus appendTransformedCornerNormals(const DMeshInternalData& mesh,
                                             const MMatrix& matrix, DPointList& output) {
    const std::size_t count = mesh.getConnectList().length();
    const auto base = mesh.getCompAttr(D_COMP_PER_VTX, cornerNormalAttribute());
    const auto attribute = std::dynamic_pointer_cast<const DPVec3fAttr>(base);
    if (base && (!attribute || attribute->value().size() != count)) return MS::kInvalidParameter;
    std::size_t total = 0;
    if (!generation::checkedSum(output.size(), count, generation::kMaxWorkItemCount, total))
        return MS::kInvalidParameter;
    output.reserve(total);
    for (std::size_t i = 0; i < count; ++i) {
        const DPoint input = attribute ? attribute->value()[i] : DPoint(0, 0, 0);
        std::array<float, 3> normal;
        if (!numeric::checkedTransformNormal(input, matrix, normal)) return MS::kInvalidParameter;
        output.push_back(DPoint(normal[0], normal[1], normal[2]));
    }
    return MS::kSuccess;
}

inline MStatus appendCornerNormals(const DMeshInternalData& mesh, bool transformed,
                                   DPointList& output) {
    const MMatrix matrix = transformed ? mesh.getTransformMatrix() : MMatrix::identity;
    return appendTransformedCornerNormals(mesh, matrix, output);
}

inline MStatus captureCornerNormals(const MObject& object, DMeshInternalData& output,
                                   MSpace::Space space = MSpace::kObject) {
    MStatus status;
    MFnMesh mesh(object, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const auto& faces = output.getFaceList();
    const std::size_t corners = output.numberOfConnections();
    const int rawNormalCount = mesh.numNormals(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    // Bound the added import workspace before requesting native arrays. This
    // covers IDs, the native normal table and conservative attribute copies,
    // not the memory already owned by Maya or the complete output geometry.
    const std::size_t limit = 128U * 1024U * 1024U;
    std::size_t normalCount = 0, tableBytes = 0, cornerBytes = 0, faceBytes = 0, bytes = 0;
    if (!generation::checkedCount(rawNormalCount, generation::kMaxWorkItemCount, normalCount) ||
        !generation::checkedProduct(normalCount, 3 * sizeof(float), limit, tableBytes) ||
        !generation::checkedProduct(corners, sizeof(int) + 3 * sizeof(DPoint), limit, cornerBytes) ||
        !generation::checkedProduct(faces.length(), sizeof(int), limit, faceBytes) ||
        !generation::checkedSum(tableBytes, cornerBytes, limit, bytes) ||
        !generation::checkedSum(bytes, faceBytes, limit, bytes)) return MS::kInvalidParameter;
    MIntArray counts, ids;
    MFloatVectorArray normals;
    DOMMY_RETURN_IF_FAILURE(mesh.getNormalIds(counts, ids));
    if (counts.length() != faces.length() || ids.length() != corners)
        return MS::kFailure;
    for (unsigned int i = 0; i < counts.length(); ++i)
        if (counts[i] != faces[i]) return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(mesh.getNormals(normals, space));
    if (normals.length() != normalCount) return MS::kFailure;
    DPointList candidate;
    candidate.reserve(ids.length());
    for (unsigned int i = 0; i < ids.length(); ++i) {
        if (ids[i] < 0 || static_cast<unsigned int>(ids[i]) >= normals.length()) return MS::kFailure;
        const auto& normal = normals[ids[i]];
        if (!std::isfinite(normal.x) || !std::isfinite(normal.y) || !std::isfinite(normal.z))
            return MS::kInvalidParameter;
        candidate.push_back(DPoint(normal.x, normal.y, normal.z));
    }
    return output.addCompAttr(D_COMP_PER_VTX, cornerNormalAttribute(),
                             DPVec3fAttr(candidate, DPoint(0, 0, 0))) ? MS::kSuccess : MS::kFailure;
}

inline MStatus restoreCornerNormals(MFnMesh& output, const DMeshInternalData& mesh, bool transformed) {
    if (!hasCornerNormals(mesh)) return MS::kSuccess;
    DPointList corners;
    DOMMY_RETURN_IF_FAILURE(appendCornerNormals(mesh, transformed, corners));
    MVectorArray normals;
    MIntArray faces, vertices;
    const unsigned int count = static_cast<unsigned int>(corners.size());
    DOMMY_RETURN_IF_FAILURE(normals.setLength(count));
    DOMMY_RETURN_IF_FAILURE(faces.setLength(count));
    DOMMY_RETURN_IF_FAILURE(vertices.setLength(count));
    unsigned int corner = 0, used = 0;
    const MIntArray faceCounts = mesh.getFaceList(), connections = mesh.getConnectList();
    for (unsigned int face = 0; face < faceCounts.length(); ++face) {
        for (int local = 0; local < faceCounts[face]; ++local, ++corner) {
            if (corner >= corners.size()) return MS::kInvalidParameter;
            const DPoint& normal = corners[corner];
            if (normal[0] == 0 && normal[1] == 0 && normal[2] == 0) continue;
            normals[used] = MVector(normal[0], normal[1], normal[2]);
            faces[used] = static_cast<int>(face);
            if (corner >= connections.length()) return MS::kInvalidParameter;
            vertices[used++] = connections[corner];
        }
    }
    if (corner != corners.size()) return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(normals.setLength(used));
    DOMMY_RETURN_IF_FAILURE(faces.setLength(used));
    DOMMY_RETURN_IF_FAILURE(vertices.setLength(used));
    return used ? output.setFaceVertexNormals(normals, faces, vertices, MSpace::kObject) : MStatus(MS::kSuccess);
}

} // namespace mesh
} // namespace dommy
