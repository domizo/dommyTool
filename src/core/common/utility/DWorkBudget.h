#pragma once

#include "common/utility/DGenerationLimits.h"

namespace dommy {
namespace generation {

// Share one instance across an evaluation's primitives and algorithm stages.
// Units represent visits, estimated comparisons and copied entries, not CPU
// time or memory bytes. Exhaustion is sticky, including multiplication failure.
class WorkBudget {
public:
    explicit WorkBudget(std::size_t limit = kMaxWorkItemCount)
        : limit_(limit), remaining_(limit), exceeded_(false) {}

    bool consume(std::size_t count)
    {
        if (exceeded_ || count > remaining_) {
            exceeded_ = true;
            return false;
        }
        remaining_ -= count;
        return true;
    }

    bool consumeProduct(std::size_t count, std::size_t cost)
    {
        std::size_t work = 0U;
        if (!memory::checkedMultiply(count, cost, work)) {
            exceeded_ = true;
            return false;
        }
        return consume(work);
    }

    std::size_t used() const { return limit_ - remaining_; }
    std::size_t remaining() const { return remaining_; }
    bool exceeded() const { return exceeded_; }

private:
    std::size_t limit_;
    std::size_t remaining_;
    bool exceeded_;
};

inline std::size_t orderedContainerWork(std::size_t entries)
{
    std::size_t work = 1U;
    while (entries > 0U) {
        ++work;
        entries /= 2U;
    }
    return work;
}

} // namespace generation
} // namespace dommy
