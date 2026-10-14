#pragma once

#include "DOpenVDBPointGenerationPolicy.h"
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace vdb {

struct MeshConversionSettings {
    float voxelSize = 1, exterior = 3, interior = 3;
    bool unsignedDistance = false, fillInterior = false;
};

inline bool validMeshConversionSettings(const MeshConversionSettings& settings) {
    return std::isfinite(settings.voxelSize) && settings.voxelSize >= 1e-6f &&
        settings.voxelSize <= 1e6f && std::isfinite(settings.exterior) &&
        settings.exterior > 0 && settings.exterior <= 64 &&
        (settings.unsignedDistance || settings.fillInterior ||
         (std::isfinite(settings.interior) && settings.interior > 0 && settings.interior <= 64));
}

template<typename GridType = openvdb::FloatGrid>
inline bool planMeshConversion(const std::vector<openvdb::Vec3s>& points,
                               const std::vector<openvdb::Vec4I>& primitives,
                               const openvdb::math::Transform& transform,
                               const MeshConversionSettings& settings,
                               TreeEditBudget& budget, std::size_t outputCopies,
                               openvdb::CoordBBox* admittedRegion = nullptr) {
    if (!validMeshConversionSettings(settings) || !transform.isLinear() ||
        !validDerivativeGridMap(transform) || points.empty() || primitives.empty() ||
        points.size() > generation::kMaxGeneratedPointCount ||
        primitives.size() > generation::kMaxGeneratedFaceCount || outputCopies < 1 || outputCopies > 2)
        return false;
    math::Bounds3d bounds;
    for (const auto& p : points)
        if (!bounds.add(p[0], p[1], p[2])) return false;
    for (const auto& primitive : primitives) {
        for (int axis = 0; axis < 4; ++axis) {
            if (axis == 3 && primitive[axis] == openvdb::util::INVALID_IDX) continue;
            if (primitive[axis] >= points.size()) return false;
        }
    }
    auto box = gridBounds(bounds);
    const float interior = settings.unsignedDistance || settings.fillInterior ?
        settings.exterior : settings.interior;
    // FillInterior fills the admitted domain, not an FLT_MAX-sized padding.
    box.expand(std::ceil(std::max(settings.exterior, interior)) + 8);
    openvdb::CoordBBox checked;
    const bool accepted = pointGenerationBox(box, transform, checked) &&
        budget.addWork(points.size(), 8) && budget.addWork(primitives.size(), 32) &&
        chargeDenseTreeRegion<GridType>(checked, budget, outputCopies + 2, 64, 128);
    if (accepted && admittedRegion) *admittedRegion = checked;
    return accepted;
}

template<typename GridType>
inline bool validMeshVolume(const GridType& grid,
                            const openvdb::math::Transform& transform) {
    if (grid.transform() != transform) return false;
    TreeEditBudget budget;
    math::Bounds3d bounds;
    return pointGenerationDomain(grid, true, budget, bounds);
}

// The caller preflights all mesh input arrays before this allocating boundary.
inline bool convertMeshVolume(const std::vector<openvdb::Vec3s>& points,
                              const std::vector<openvdb::Vec4I>& primitives,
                              const openvdb::math::Transform& transform,
                              const MeshConversionSettings& settings,
                              TreeEditBudget& budget, std::size_t outputCopies,
                              openvdb::FloatGrid::Ptr& output) {
    output.reset();
    if (!planMeshConversion(points, primitives, transform, settings, budget, outputCopies)) return false;
    const float interior = settings.unsignedDistance ? settings.exterior :
        settings.fillInterior ? std::numeric_limits<float>::max() : settings.interior;
    const int flags = settings.unsignedDistance ? openvdb::tools::UNSIGNED_DISTANCE_FIELD : 0;
    openvdb::tools::QuadAndTriangleDataAdapter<openvdb::Vec3s, openvdb::Vec4I> mesh(points, primitives);
    auto result = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
        mesh, transform, settings.exterior, interior, flags);
    if (!result || !validMeshVolume(*result, transform)) return false;
    output = result;
    return true;
}

inline bool convertMeshVolumeToFog(openvdb::FloatGrid::Ptr& grid) {
    if (!grid) return false;
    const auto transform = grid->transform().copy();
    if (!transform) return false;
    if (grid->empty()) {
        // OpenVDB 5.2 fog conversion indexes empty node vectors.
        const auto name = grid->getName();
        grid = openvdb::FloatGrid::create(0.0f);
        if (!grid) return false;
        grid->setName(name);
        grid->setTransform(transform);
        grid->setGridClass(openvdb::GRID_FOG_VOLUME);
    } else {
        openvdb::tools::sdfToFogVolume(*grid);
    }
    return validMeshVolume(*grid, *transform);
}

} // namespace vdb
} // namespace dommy
