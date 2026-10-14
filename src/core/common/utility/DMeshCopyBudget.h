#pragma once

#include "common/utility/DPrimitiveCopyBudget.h"
#include <cstddef>

namespace dommy {
namespace mesh {

// Inspect component payloads copied by a mesh transaction. A merge also
// charges fields later discarded by schema filtering: cloning precedes that
// filtering, so those fields still occupy staging storage and validation work.
template <typename Iterator>
bool addMeshMetadataCost(Iterator begin, Iterator end,
                         attribute::ValidationBudget& cost)
{
    for (; begin != end; ++begin) {
        const auto& mesh = *begin;
        if (!mesh || mesh->numFace() < 0 ||
            !geometry::addCopyMetadataCost(*mesh, mesh->numberOfPoints(), cost) ||
            !geometry::addCopyAttributeCost(mesh->beginFaceAttr(), mesh->endFaceAttr(),
                static_cast<std::size_t>(mesh->numFace()), cost) ||
            !geometry::addCopyAttributeCost(mesh->beginPVtxAttr(), mesh->endPVtxAttr(),
                mesh->numberOfConnections(), cost) ||
            !geometry::addCopyGroupCost(mesh->beginFaceGrp(), mesh->endFaceGrp(), cost) ||
            !geometry::addCopyGroupCost(mesh->beginEdgeGrp(), mesh->endEdgeGrp(), cost)) return false;
    }
    return true;
}

// Charge the final retained metadata, not the cumulative prefixes of a series
// of append operations. Component schemas/defaults are copied per mesh, while
// collection schemas/defaults occur once and only their rows are repeated.
// The caller validates topology first. No geometry or repeated-index array is
// allocated here; limits are logical work/payload guards, not peak-memory caps.
template <typename Owner>
bool validateRepeatedMeshMetadata(const Owner& source, std::size_t copies,
                                  const MeshMergePlan& output)
{
    using namespace generation;
    if (copies == 0U || copies > kMaxGeneratedTransformCount) return false;
    attribute::ValidationBudget componentCost;
    if (!addMeshMetadataCost(source.beginMeshes(), source.endMeshes(), componentCost)) return false;
    attribute::ValidationBudget budget(output.workItems);
    std::size_t repeated = 0U;
    if (!checkedProduct(componentCost.workItems, copies, kMaxWorkItemCount, repeated) ||
        !checkedSum(budget.workItems, repeated, kMaxWorkItemCount, budget.workItems) ||
        !checkedProduct(componentCost.stringBytes, copies, kMaxGeneratedStringByteCount, budget.stringBytes))
        return false;

    return geometry::addRepeatedCollectionMetadataCost(source, source.numberOfMesh(),
        copies, output.meshes, budget);
}

} // namespace mesh
} // namespace dommy
