#pragma once

#include "DOpenVDBValuePolicy.h"
#include "../Common/DOpenVDBLimits.h"
#include <openvdb/math/Coord.h>

namespace dommy {
namespace vdb {
struct TreeEditBudget {
    std::size_t bytes = 0, work = 0;
    bool addBytes(std::size_t count, std::size_t width) {
        // Four-copy allowance within the existing 512 MiB VDB payload limit.
        // Admission estimate only; allocator/native peak usage is not guaranteed.
        const std::size_t limit = dommy::vdb::kMaxVdbPayloadBytes / 4U;
        std::size_t added = 0;
        return dommy::generation::checkedProduct(count, width, limit, added) &&
               dommy::generation::checkedSum(bytes, added, limit, bytes);
    }
    bool addWork(std::size_t count, std::size_t width) {
        using namespace dommy::generation;
        std::size_t added = 0;
        return checkedProduct(count, width, kMaxWorkItemCount, added) &&
               checkedSum(work, added, kMaxWorkItemCount, work);
    }
};

inline bool treeBlockCount(const openvdb::CoordBBox& bounds, unsigned int dimension,
                           std::size_t& count) {
    if (dimension == 0)
        return false;
    count = 1;
    for (int axis = 0; axis < 3; ++axis) {
        // Double represents every Int32 coordinate and power-of-two node origin.
        const double low = std::floor(double(bounds.min()[axis]) / dimension);
        const double high = std::floor(double(bounds.max()[axis]) / dimension);
        std::size_t width = 0;
        if (!dommy::numeric::checkedNumericCast(high - low + 1, width) ||
            !dommy::generation::checkedProduct(count, width, dommy::generation::kMaxWorkItemCount,
                                               count))
            return false;
    }
    return true;
}

template <typename NodeT>
bool chargeTreeNode(const openvdb::CoordBBox& bounds, TreeEditBudget& budget) {
    std::size_t nodes = 0;
    if (!treeBlockCount(bounds, NodeT::DIM, nodes))
        return false;
    using ValueT = typename NodeT::ValueType;
    // Includes whole boundary nodes, masks/tables and value buffers. Counting
    // full expansion also covers sparse fills that split existing tiles.
    const std::size_t nodeBytes =
        sizeof(NodeT) + std::size_t(NodeT::NUM_VALUES) * (sizeof(ValueT) + sizeof(void*)) + 64U;
    const std::size_t nodeWork =
        std::size_t(NodeT::NUM_VALUES) * dommy::vdb::ValueComponents<ValueT>::count * 2U;
    return budget.addBytes(nodes, nodeBytes) && budget.addWork(nodes, nodeWork);
}
template <typename NodeT, int Level = NodeT::LEVEL> struct TreeNodeBudget {
    static bool add(const openvdb::CoordBBox& bounds, TreeEditBudget& budget) {
        return chargeTreeNode<NodeT>(bounds, budget) &&
               TreeNodeBudget<typename NodeT::ChildNodeType>::add(bounds, budget);
    }
};
template <typename NodeT> struct TreeNodeBudget<NodeT, 0> {
    static bool add(const openvdb::CoordBBox& bounds, TreeEditBudget& budget) {
        return chargeTreeNode<NodeT>(bounds, budget);
    }
};

// Reserve dense legacy-kernel workspace plus typed sparse-tree overhead.
// This is an admission estimate, not a bound on the native allocator's peak.
template <typename GridT>
bool chargeDenseTreeRegion(const openvdb::CoordBBox& bounds, TreeEditBudget& budget,
                           std::size_t copies, std::size_t bytesPerVoxel,
                           std::size_t workPerVoxel) {
    if (copies == 0 || copies > kMaxVdbGridCount) return false;
    using Root = typename GridT::TreeType::RootNodeType::ChildNodeType;
    std::size_t voxels = 0, total = 0;
    if (!treeBlockCount(bounds, 1, voxels) ||
        !generation::checkedProduct(voxels, copies, generation::kMaxWorkItemCount, total) ||
        !budget.addBytes(total, bytesPerVoxel) || !budget.addWork(total, workPerVoxel)) return false;
    for (std::size_t i = 0; i < copies; ++i)
        if (!TreeNodeBudget<Root>::add(bounds, budget)) return false;
    return true;
}

} // namespace vdb
} // namespace dommy
