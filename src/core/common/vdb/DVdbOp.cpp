// Shared Maya mesh intake and the historical world-space conversion adapter.
#include "DVdbOp.h"
#include "DOpenVDBGeoUtil.h"
#include "common/utility/DMayaMeshTraversal.h"
#include "common/utility/DStatusOperation.h"
#include "../../../modules/volume/VDB/Edit/DOpenVDBMeshConversionPolicy.h"

MStatus preflightVdbMeshInput(const MObject& meshObject,
                              dommy::vdb::TreeEditBudget& budget,
                              std::size_t& vertexCount, std::size_t& triangleCount) {
    if (meshObject.isNull()) return MS::kInvalidParameter;
    MStatus status;
    MFnMesh mesh(meshObject, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    const int vertices = mesh.numVertices(&status);
    DOMMY_RETURN_IF_FAILURE(status);
    if (!dommy::generation::checkedCount(vertices, dommy::generation::kMaxGeneratedPointCount,
                                         vertexCount) || vertexCount == 0) return MS::kInvalidParameter;
    std::size_t faces = 0;
    DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshTriangles(mesh, meshObject, faces, triangleCount));
    if (triangleCount == 0 || !budget.addBytes(vertexCount, 96) ||
        !budget.addBytes(triangleCount, 96) || !budget.addWork(vertexCount, 8) ||
        !budget.addWork(triangleCount, 16)) return MS::kInvalidParameter;
    return MS::kSuccess;
}

bool GenerateVdbMeshOp(short voxelType, float voxelSize, const MObject& meshObject,
                       float exteriorBandWidth, float interiorBandWidth,
                       bool unsignedDistanceField, bool fillInterior,
                       openvdb::FloatGrid::Ptr& grid, dommy::vdb::TreeEditBudget* sharedBudget) {
    grid.reset();
    dommy::vdb::MeshConversionSettings settings;
    settings.voxelSize = voxelSize; settings.exterior = exteriorBandWidth;
    settings.interior = interiorBandWidth; settings.unsignedDistance = unsignedDistanceField;
    settings.fillInterior = fillInterior;
    if (voxelType < 0 || voxelType > 1 || !dommy::vdb::validMeshConversionSettings(settings))
        return false;
    dommy::vdb::TreeEditBudget localBudget;
    auto& budget = sharedBudget ? *sharedBudget : localBudget;
    std::size_t vertexCount = 0, triangleCount = 0;
    if (!preflightVdbMeshInput(meshObject, budget, vertexCount, triangleCount)) return false;
    auto transform = openvdb::math::Transform::createLinearTransform(voxelSize);
    if (!transform) return false;
    MStatus status;
    MFnMesh mesh(meshObject, &status);
    if (!status) return false;
    MFloatPointArray vertices;
    if (!mesh.getPoints(vertices, MSpace::kWorld) || vertices.length() != vertexCount) return false;
    std::vector<openvdb::Vec3s> points;
    points.reserve(vertexCount);
    for (unsigned int i = 0; i < vertices.length(); ++i) {
        const auto& p = vertices[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
        const auto index = transform->worldToIndex(openvdb::Vec3d(p.x, p.y, p.z));
        for (int axis = 0; axis < 3; ++axis)
            if (!dommy::numeric::canRepresentAsFloat(index[axis])) return false;
        points.push_back(openvdb::Vec3s(index[0], index[1], index[2]));
    }
    std::vector<openvdb::Vec4I> primitives;
    if (!DOpenVDBMakePrimList(meshObject, primitives) || primitives.size() > triangleCount) return false;
    openvdb::FloatGrid::Ptr candidate;
    if (!dommy::vdb::convertMeshVolume(points, primitives, *transform, settings, budget, 1, candidate) ||
        (voxelType == 1 && !dommy::vdb::convertMeshVolumeToFog(candidate))) return false;
    grid = candidate;
    return true;
}
