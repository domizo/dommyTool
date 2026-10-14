#pragma once

#include "common/utility/DGeometryBatchBudget.h"
#include "common/utility/DMeshCopyBudget.h"
#include "common/utility/DCurveOperationBudget.h"

namespace dommy {
namespace bullet {

// Callers validate topology first. Reuse the same component/collection metadata
// accounting as the data-copy transactions before admitting another clone.
template <typename Owner>
bool admitMeshInput(const Owner& source, geometry::GeometryBatchBudget& batch,
                    bool retain = true)
{
    mesh::MeshMergePlan plan;
    attribute::ValidationBudget metadata;
    return mesh::appendMeshRangePlan(source.beginMeshes(), source.endMeshes(), plan) &&
        mesh::addMeshMetadataCost(source.beginMeshes(), source.endMeshes(), metadata) &&
        geometry::addCopyMetadataCost(source, source.numberOfMesh(), metadata) &&
        batch.append(plan, metadata.workItems, metadata.stringBytes, retain);
}

template <typename Owner>
bool admitCurveInput(const Owner& source, geometry::GeometryBatchBudget& batch,
                     bool retain = true)
{
    mesh::MeshMergePlan plan;
    attribute::ValidationBudget metadata;
    return curve::addCurveOperationCost(source.beginCurves(), source.endCurves(), plan, metadata) &&
        geometry::addCopyMetadataCost(source, source.numberOfCurves(), metadata) &&
        batch.append(plan, metadata.workItems, metadata.stringBytes, retain);
}

} // namespace bullet
} // namespace dommy
