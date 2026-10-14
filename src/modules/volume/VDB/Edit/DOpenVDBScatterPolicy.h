#pragma once

#include "DOpenVDBPointGenerationPolicy.h"
#include "../Common/DOpenVDBUtil.h"
#include "../Common/VDBPointUtil.h"
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/util/NullInterrupter.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <type_traits>
#include <vector>

namespace dommy {
namespace vdb {

struct ScatterSettings {
    int seed = 1, mode = 0;
    double spread = 1;
    openvdb::Index64 count = 0;
    float density = 0, perVoxel = 0;
    bool local = false, interior = false;
};
struct ScatterPlan {
    GridCPtr grid;
    bool interior = false;
    std::size_t maximumActive = 0, count = 0;
};
inline bool addScatterCount(double value, bool signedNative, std::size_t& count) {
    // Native local/dense scatter converts each voxel/tile count to int before
    // calling PointAccessor. Check that conversion even for negative densities.
    int nativeCount = 0;
    if (!std::isfinite(value) ||
        (signedNative && !dommy::numeric::checkedNumericCast(value, nativeCount))) return false;
    const double upper = std::ceil(std::max(0.0, value));
    std::size_t added = 0;
    return dommy::numeric::checkedNumericCast(upper, added) &&
        dommy::generation::checkedSum(count, added, dommy::generation::kMaxGeneratedPointCount, count);
}
struct ScatterDomain {
    dommy::vdb::TreeEditBudget& budget;
    bool interior, valid = false;
    dommy::math::Bounds3d bounds;
    ScatterDomain(dommy::vdb::TreeEditBudget& b, bool i) : budget(b), interior(i) {}
    template<typename GridT> void operator()(const GridT& grid) {
        valid = dommy::vdb::pointGenerationDomain(grid, interior, budget, bounds);
    }
};
struct WeightedScatterCount {
    double perVoxel;
    std::size_t count = 0;
    bool valid = true;
    explicit WeightedScatterCount(double value) : perVoxel(value) {}
    template<typename GridT> void operator()(const GridT& grid) {
        for (auto it = grid.cbeginValueOn(); valid && it; ++it) {
            const double value = (*it) * perVoxel * it.getVoxelCount();
            valid = addScatterCount(value, true, count);
        }
    }
};
struct DenseScatterCount {
    float perVoxel;
    std::size_t count = 0;
    bool valid = true;
    explicit DenseScatterCount(float value) : perVoxel(value) {}
    template<typename GridT> void operator()(const GridT& grid) {
        if (perVoxel < 1e-6f) return; // Native early-out.
        int checked = 0;
        valid = dommy::numeric::checkedNumericCast(std::floor(perVoxel), checked);
        for (auto it = grid.cbeginValueOn(); valid && it; ++it) {
            // Keep native float multiplication before promotion to double.
            const double value = it.isVoxelValue() ? double(perVoxel) :
                double(perVoxel * it.getVoxelCount());
            valid = addScatterCount(value, true, count);
        }
    }
};
inline MStatus planScatter(const GridCPtr& grid, const ScatterSettings& settings,
                    dommy::vdb::TreeEditBudget& budget, ScatterPlan& plan) {
    plan.grid = grid;
    plan.interior = settings.interior && grid->getGridClass() == openvdb::GRID_LEVEL_SET;
    const auto type = getVdbGridType(*grid);
    if (plan.interior && type != DVdbGridType::kFloat && type != DVdbGridType::kDouble)
        return MS::kInvalidParameter;
    if ((settings.mode == 0 || (settings.mode == 1 && !settings.local)) && !plan.interior &&
        type != DVdbGridType::kFloat && type != DVdbGridType::kDouble &&
        type != DVdbGridType::kInt32 && type != DVdbGridType::kInt64) return MS::kInvalidParameter;
    ScatterDomain domain(budget, plan.interior);
    if (!processVdbTopologyGrid(type, *grid, domain) || !domain.valid ||
        !dommy::numeric::checkedNumericCast(grid->activeVoxelCount(), plan.maximumActive) ||
        plan.maximumActive > dommy::generation::kMaxWorkItemCount) return MS::kInvalidParameter;
    if (plan.interior) {
        dommy::vdb::PositiveMaskBackground exterior;
        if (!processVdbRealGrid(type, *grid, exterior) || !exterior.valid) return MS::kInvalidParameter;
        if (domain.bounds.empty()) plan.maximumActive = 0;
        else {
            auto box = dommy::vdb::gridBounds(domain.bounds);
            box.expand(1);
            openvdb::CoordBBox region;
            if (!dommy::vdb::pointGenerationBox(box, grid->transform(), region) ||
                !dommy::vdb::treeBlockCount(region, 1, plan.maximumActive) ||
                !dommy::vdb::chargeDenseTreeRegion<openvdb::BoolGrid>(region, budget, 1, 32, 32))
                return MS::kInvalidParameter;
        }
    }
    const auto dx = grid->voxelSize();
    const double localVolume = dx[0] * dx[1] * dx[2];
    const double volume = grid->transform().voxelVolume();
    if (!std::isfinite(volume) || volume <= 0 || !std::isfinite(localVolume) || localVolume <= 0)
        return MS::kInvalidParameter;
    if (plan.maximumActive != 0) {
        if (settings.mode == 0) plan.count = static_cast<std::size_t>(settings.count);
        else if (settings.mode == 1 && settings.local) {
            if (!addScatterCount(settings.density * volume * double(plan.maximumActive), false, plan.count))
                return MS::kInvalidParameter;
        } else if (plan.interior) {
            const double perVoxel = settings.mode == 1 ? settings.density * localVolume : settings.perVoxel;
            // A mask can contain either voxels or active tiles. Reserve rounding
            // per voxel as well as validating the largest possible native tile.
            std::size_t perCell = 0, tile = 0;
            if (!addScatterCount(perVoxel, true, perCell) ||
                !addScatterCount(perVoxel * double(plan.maximumActive), true, tile) ||
                !dommy::generation::checkedProduct(perCell, plan.maximumActive,
                    dommy::generation::kMaxGeneratedPointCount, plan.count)) return MS::kInvalidParameter;
        } else if (settings.mode == 1) {
            WeightedScatterCount count(settings.density * localVolume);
            if (!processVdbScalarGrid(type, *grid, count) || !count.valid) return MS::kInvalidParameter;
            plan.count = count.count;
        } else {
            DenseScatterCount count(settings.perVoxel);
            if (!processVdbTopologyGrid(type, *grid, count) || !count.valid) return MS::kInvalidParameter;
            plan.count = count.count;
        }
    }
    if (!budget.addBytes(plan.count, 64) ||
        !budget.addWork(plan.count, 16 + dommy::vdb::pointSortPasses(plan.count)))
        return MS::kInvalidParameter;
    return MS::kSuccess;
}
struct ScatterInteriorMask {
    openvdb::BoolGrid::Ptr mask;
    template<typename GridT> void operator()(const GridT& grid) {
        mask = openvdb::tools::sdfInteriorMask(grid);
    }
};
template<typename Op>
bool dispatchScatterSource(DVdbGridType type, const Grid& grid, Op& operation, std::true_type) {
    return processVdbScalarGrid(type, grid, operation);
}
template<typename Op>
bool dispatchScatterSource(DVdbGridType type, const Grid& grid, Op& operation, std::false_type) {
    return processVdbTopologyGrid(type, grid, operation);
}
template<bool Scalar, typename Op>
bool applyScatter(const ScatterPlan& plan, Op& operation) {
    const auto type = getVdbGridType(*plan.grid);
    if (!plan.interior) return dispatchScatterSource(
        type, *plan.grid, operation, std::integral_constant<bool, Scalar>());
    ScatterInteriorMask interior;
    if (!processVdbRealGrid(type, *plan.grid, interior) || !interior.mask ||
        interior.mask->transform() != plan.grid->transform() ||
        interior.mask->activeVoxelCount() > plan.maximumActive) return false;
    dommy::vdb::TreeEditBudget budget;
    dommy::math::Bounds3d bounds;
    if (!dommy::vdb::pointGenerationDomain(*interior.mask, false, budget, bounds)) return false;
    operation(*interior.mask);
    return true;
}
using ScatterRandomGenerator = std::mersenne_twister_engine<std::uint32_t, 32, 351, 175, 19,
    0xccab8ee7, 11, 0xffffffff, 7, 0x31b6ab00, 15, 0xffe50000, 17, 1812433253>;

inline MStatus scatterPoints(const std::vector<ScatterPlan>& plans,
                              const ScatterSettings& settings, DPointList& positions) {
    std::size_t total = 0;
    for (const auto& plan : plans)
        if (!generation::checkedSum(total, plan.count, generation::kMaxGeneratedPointCount, total))
            return MS::kInvalidParameter;
    PointAccessor points(&positions, total);
    openvdb::util::NullInterrupter interrupt;
    ScatterRandomGenerator random(settings.seed);
    for (const auto& plan : plans) {
        bool dispatched = false;
        if (settings.mode == 0) {
            openvdb::tools::UniformPointScatter<PointAccessor, ScatterRandomGenerator, openvdb::util::NullInterrupter>
                scatter(points, settings.count, random, settings.spread, &interrupt);
            dispatched = applyScatter<true>(plan, scatter);
        } else if (settings.mode == 1 && !settings.local) {
            openvdb::tools::NonUniformPointScatter<PointAccessor, ScatterRandomGenerator, openvdb::util::NullInterrupter>
                scatter(points, settings.density, random, settings.spread, &interrupt);
            dispatched = applyScatter<true>(plan, scatter);
        } else if (settings.mode == 1) {
            openvdb::tools::UniformPointScatter<PointAccessor, ScatterRandomGenerator, openvdb::util::NullInterrupter>
                scatter(points, settings.density, random, settings.spread, &interrupt);
            dispatched = applyScatter<false>(plan, scatter);
        } else {
            openvdb::tools::DenseUniformPointScatter<PointAccessor, ScatterRandomGenerator, openvdb::util::NullInterrupter>
                scatter(points, settings.perVoxel, random, settings.spread, &interrupt);
            dispatched = applyScatter<false>(plan, scatter);
        }
        if (!dispatched || interrupt.wasInterrupted()) return MS::kFailure;
    }
    return MS::kSuccess;
}

} // namespace vdb
} // namespace dommy
