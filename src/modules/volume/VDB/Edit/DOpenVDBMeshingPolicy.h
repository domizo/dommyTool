#pragma once

#include "DOpenVDBMeshConversionPolicy.h"
#include "../Common/DOpenVDBUtil.h"
#include <openvdb/util/NullInterrupter.h>

namespace dommy {
namespace vdb {

struct MeshingBudget {
    TreeEditBudget resources;
    std::size_t points = 0, faces = 0, connects = 0;
    bool addOutput(std::size_t pointCount, std::size_t faceCount, std::size_t connectCount) {
        return generation::checkedSum(points, pointCount, generation::kMaxGeneratedPointCount, points) &&
            generation::checkedSum(faces, faceCount, generation::kMaxGeneratedFaceCount, faces) &&
            generation::checkedSum(connects, connectCount, generation::kMaxWorkItemCount, connects) &&
            resources.addBytes(pointCount, 128) && resources.addBytes(faceCount, 64) &&
            resources.addBytes(connectCount, 32) && resources.addWork(pointCount, 16) &&
            resources.addWork(connectCount, 16);
    }
};

struct MeshingPreflight {
    TreeEditBudget& budget;
    bool valid = false;
    explicit MeshingPreflight(TreeEditBudget& b) : budget(b) {}
    template<typename GridType> void operator()(const GridType& grid) {
        math::Bounds3d bounds;
        if (grid.activeVoxelCount() > generation::kMaxGeneratedPointCount ||
            !pointGenerationDomain(grid, true, budget, bounds)) return;
        if (bounds.empty()) { valid = true; return; }
        auto box = gridBounds(bounds);
        box.expand(2); // Surface cells, gradients and interpolation neighborhoods.
        openvdb::CoordBBox region;
        valid = pointGenerationBox(box, grid.transform(), region) &&
            chargeDenseTreeRegion<GridType>(region, budget, 2, 64, 64);
    }
};

// Validate every selected supported grid before the first native mesher runs.
// Unsupported types retain the historical skip behavior.
inline bool planMeshingGrids(const GridCPtrVec& grids, MeshingBudget& budget) {
    if (grids.size() > kMaxVdbGridCount) return false;
    for (const auto& grid : grids) {
        if (!grid) return false;
        MeshingPreflight check(budget.resources);
        if (processVdbScalarGrid(getVdbGridType(*grid), *grid, check) && !check.valid) return false;
    }
    return true;
}

template<typename GridType>
bool buildReferenceVolume(const std::vector<openvdb::Vec3s>& points,
                          const std::vector<openvdb::Vec4I>& primitives,
                          const openvdb::math::Transform& transform, bool sharpen,
                          TreeEditBudget& budget, typename GridType::ConstPtr& output,
                          typename GridType::template ValueConverter<openvdb::Int32>::Type::Ptr& indices) {
    output.reset(); indices.reset();
    MeshConversionSettings settings;
    if (!numeric::checkedNumericCast(transform.voxelSize()[0], settings.voxelSize)) return false;
    openvdb::CoordBBox region;
    using IndexGrid = typename GridType::template ValueConverter<openvdb::Int32>::Type;
    if (!planMeshConversion<GridType>(points, primitives, transform, settings, budget, 1, &region) ||
        !chargeDenseTreeRegion<IndexGrid>(region, budget, 1, 32, 32) ||
        (sharpen && !chargeDenseTreeRegion<openvdb::BoolGrid>(region, budget, 2, 128, 128))) return false;
    auto index = IndexGrid::create(0);
    if (!index) return false;
    openvdb::tools::QuadAndTriangleDataAdapter<openvdb::Vec3s, openvdb::Vec4I> mesh(points, primitives);
    openvdb::util::NullInterrupter interrupt;
    auto result = openvdb::tools::meshToVolume<GridType>(interrupt, mesh, transform, 3.0f, 3.0f, 0, index.get());
    if (!result || interrupt.wasInterrupted() || !validMeshVolume(*result, transform)) return false;
    for (auto it = index->cbeginValueOn(); it; ++it)
        if (!budget.addWork(1, 1) || *it < 0 ||
            static_cast<std::size_t>(*it) >= primitives.size()) return false;
    output = result; indices = index;
    return true;
}

} // namespace vdb
} // namespace dommy
