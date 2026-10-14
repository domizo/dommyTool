#pragma once

#include "common/utility/DMeshMergePlan.h"

namespace dommy {
namespace geometry {

// One instance spans an entire solver evaluation, not one connected payload.
// Estimates reuse the existing geometry storage/work/string policies. Native
// Maya DG caches and Bullet workspaces are not process-memory measurements.
class GeometryBatchBudget final {
public:
    bool append(const mesh::MeshMergePlan& source, std::size_t metadataWork,
                std::size_t strings, bool retain = true)
    {
        GeometryBatchBudget next = *this;
        using namespace generation;
        std::size_t work = 0U;
        if (!checkedSum(next.payloads_, 1U, kMaxConnectedGeometryCount, next.payloads_) ||
            !checkedSum(source.workItems, metadataWork, kMaxWorkItemCount, work) ||
            !checkedSum(next.work_, work, kMaxWorkItemCount, next.work_) ||
            !checkedSum(next.strings_, strings, kMaxGeneratedStringByteCount, next.strings_))
            return false;
        if (retain) {
            if (!checkedSum(next.plan_.meshes, source.meshes, kMaxGeneratedMeshCount, next.plan_.meshes) ||
                !checkedSum(next.plan_.points, source.points, kMaxGeneratedPointCount, next.plan_.points) ||
                !checkedSum(next.plan_.faces, source.faces, kMaxGeneratedFaceCount, next.plan_.faces) ||
                !checkedSum(next.plan_.connections, source.connections, kMaxWorkItemCount, next.plan_.connections) ||
                !checkedSum(next.plan_.uvs, source.uvs, kMaxWorkItemCount, next.plan_.uvs) ||
                !checkedSum(next.metadataWork_, metadataWork, kMaxWorkItemCount, next.metadataWork_) ||
                !checkedSum(next.retainedStrings_, strings, kMaxGeneratedStringByteCount, next.retainedStrings_) ||
                !checkedSum(next.retained_, 1U, kMaxConnectedGeometryCount, next.retained_)) return false;
            // Component counts include numeric widths and dynamic rows. Charge
            // schema/container entries conservatively, plus string payload and
            // ownership/list bookkeeping even for empty geometry collections.
            const std::size_t maximum = mesh::kMaxMeshOperationBytes;
            std::size_t extra = 0U, part = 0U, bytes = 0U;
            if (!checkedProduct(next.metadataWork_, 32U, maximum, extra) ||
                !checkedProduct(next.retainedStrings_, 4U, maximum, part) ||
                !checkedSum(extra, part, maximum, extra) ||
                !checkedProduct(next.retained_, 128U, maximum, part) ||
                !checkedSum(extra, part, maximum, extra) ||
                !mesh::meshOperationStorageBytes(next.plan_, bytes, extra)) return false;
        }
        *this = next;
        return true;
    }

private:
    mesh::MeshMergePlan plan_;
    std::size_t payloads_ = 0U, retained_ = 0U, work_ = 0U, strings_ = 0U;
    std::size_t metadataWork_ = 0U, retainedStrings_ = 0U;
};

} // namespace geometry
} // namespace dommy
