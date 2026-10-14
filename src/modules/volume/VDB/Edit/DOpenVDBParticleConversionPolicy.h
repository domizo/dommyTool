#pragma once

#include "DOpenVDBMeshConversionPolicy.h"
#include "../Common/DVdbParticleRasterPlan.h"

namespace dommy {
namespace vdb {

// A conservative admission estimate for the whole raster domain, including
// inactive interiors, mask/fog copies and optional attribute-transfer trees.
// Widely separated particles may be rejected even when their surface is sparse.
inline bool planParticleConversion(std::size_t count, const ParticleInputExtrema& extrema,
                                   const ParticleRasterParameters& settings,
                                   std::size_t outputCopies, std::size_t attributeCount,
                                   TreeEditBudget& budget, float& background) {
    std::size_t work = 0;
    float candidateBackground = 0;
    if (extrema.positions().empty() || extrema.positions().count() != count ||
        outputCopies == 0 || outputCopies > 4 ||
        attributeCount > (kMaxVdbGridCount - outputCopies) / 2 ||
        !checkedParticleRasterWork(count, extrema.maximumRadius(), extrema.maximumSpeed(),
                                  settings, generation::kMaxWorkItemCount, work) ||
        !numeric::checkedNumericCast(settings.voxelSize * settings.halfBandVoxels,
                                     candidateBackground) || candidateBackground <= 0)
        return false;
    auto transform = openvdb::math::Transform::createLinearTransform(settings.voxelSize);
    if (!transform) return false;
    auto box = gridBounds(extrema.positions());
    box = openvdb::BBoxd(transform->worldToIndex(box.min()), transform->worldToIndex(box.max()));
    const double trail = settings.rasterizeTrails ?
        extrema.maximumSpeed() * settings.velocityMultiplier / settings.voxelSize : 0;
    const double radius = extrema.maximumRadius() * settings.radiusScale *
        (1 + settings.maskWidth) / settings.voxelSize;
    box.expand(std::ceil(std::max(radius, settings.minimumRadiusVoxels) +
                         settings.halfBandVoxels + trail) + 8);
    openvdb::CoordBBox region;
    TreeEditBudget candidate = budget;
    if (!pointGenerationBox(box, *transform, region) ||
        !candidate.addBytes(count, 160) || !candidate.addWork(work, 1) ||
        !chargeDenseTreeRegion<openvdb::FloatGrid>(region, candidate, outputCopies + 2, 64, 128))
        return false;
    // Vec3d is the widest supported attribute; a tuple can produce two trees.
    // Charge the closest-point index tree even if all requested names are empty.
    if (attributeCount &&
        (!chargeDenseTreeRegion<openvdb::Int32Grid>(region, candidate, 1, 16, 32) ||
         !chargeDenseTreeRegion<openvdb::Vec3DGrid>(region, candidate, attributeCount * 2, 32, 64)))
        return false;
    budget = candidate;
    background = candidateBackground;
    return true;
}

} // namespace vdb
} // namespace dommy
