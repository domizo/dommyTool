#pragma once

#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace math {

// The caller admits storage/work before entering this traversal. Exceptions
// from allocation or the visitor propagate to the caller's staging boundary.
// Each nonempty group is visited once per emitted item, in original group order.
// The visitor receives group, item and group-major flat index; it must leave
// groupLengths unchanged for the duration of the traversal.
template <typename Visitor>
inline bool visitInterleavedIndices(const std::vector<std::size_t>& groupLengths,
                                    Visitor visit) {
    std::size_t total = 0;
    std::size_t nonempty = 0;
    for (std::size_t length : groupLengths) {
        if (length > std::numeric_limits<std::size_t>::max() - total) {
            return false;
        }
        total += length;
        if (length != 0U) ++nonempty;
    }
    struct ActiveGroup {
        std::size_t group;
        std::size_t offset;
    };
    std::vector<ActiveGroup> active;
    active.reserve(nonempty);
    std::size_t offset = 0;
    for (std::size_t group = 0; group < groupLengths.size(); ++group) {
        if (groupLengths[group] != 0U) active.push_back({group, offset});
        offset += groupLengths[group];
    }
    for (std::size_t item = 0; !active.empty(); ++item) {
        std::size_t retained = 0;
        for (std::size_t index = 0; index < active.size(); ++index) {
            const ActiveGroup current = active[index];
            visit(current.group, item, current.offset + item);
            // Nonempty lengths make subtraction safe. Stable compaction keeps
            // the historical item-major/group-major order, without empty slots.
            if (item < groupLengths[current.group] - 1U) {
                active[retained++] = current;
            }
        }
        active.resize(retained);
    }
    return true;
}

inline bool buildInterleavedOrder(const std::vector<std::size_t>& groupLengths,
                                  std::vector<std::size_t>& order) {
    order.clear();
    std::size_t total = 0;
    for (std::size_t length : groupLengths) {
        if (length > std::numeric_limits<std::size_t>::max() - total) return false;
        total += length;
    }
    order.reserve(total);
    return visitInterleavedIndices(groupLengths,
        [&order](std::size_t, std::size_t, std::size_t flatIndex) {
            order.push_back(flatIndex);
        }) && order.size() == total;
}

} // namespace math
} // namespace dommy
