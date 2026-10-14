#pragma once

#include "DPointPayloadBudget.h"
#include <algorithm>
#include <map>
#include <vector>

namespace dommy {
namespace attribute {

// Counts and borrowed attribute iterators only; no geometry/attribute copy.
// Slot zero is the point owner or collection's primitive owner. Later slots
// are its point owners. Planned names preserve sequential Not Add semantics.
class AttributeCreatePlan final {
public:
    explicit AttributeCreatePlan(const point::PointPayloadBudget& retained) : payload_(retained) {}

    template <typename Owner>
    void addTarget(const Owner& owner, std::size_t count) {
        payload_.add(1U, sizeof(Target));
        if (targets_.size() > generation::kMaxConnectedGeometryCount)
            throw std::length_error("AttributeCreate has too many owners");
        targets_.push_back(Target(count, owner.beginAttr(), owner.endAttr()));
    }

    std::size_t size() const { return targets_.size(); }
    std::size_t count(std::size_t slot) const { return targets_.at(slot).count; }

    bool shouldCreate(std::size_t slot, const std::string& name, bool replace) {
        addWork(1U);
        Target& target = targets_.at(slot);
        if (replace) return true;
        if (target.fields.count(name) != 0U) return false;
        for (auto field = target.begin; field != target.end; ++field) {
            addWork(1U);
            if (field->first == name) return false;
        }
        return true;
    }

    void addField(std::size_t slot, const std::string& name, std::size_t width,
                   std::size_t dynamicBytes) {
        Target& target = targets_.at(slot);
        std::size_t rows = 0U, bytes = 0U;
        if (!validAttributeName(name) || width == 0U ||
            !generation::checkedSum(target.count, 1U,
                generation::kMaxGeneratedPointCount + 1U, rows) ||
            !generation::checkedProduct(rows, width,
                std::numeric_limits<std::size_t>::max(), bytes) ||
            !generation::checkedSum(bytes, dynamicBytes,
                std::numeric_limits<std::size_t>::max(), bytes) ||
            !generation::checkedSum(bytes, name.size(),
                std::numeric_limits<std::size_t>::max(), bytes))
            throw std::length_error("AttributeCreate generated field is excessive");
        addWork(rows);
        auto previous = target.fields.find(name);
        const std::size_t old = previous == target.fields.end() ? 0U : previous->second;
        // Replacements of one name retain its largest planned width, not an
        // ever-growing sum of temporary candidates. The four-copy allowance
        // remains a conservative host estimate, not a native peak-memory cap.
        if (bytes > old) payload_.add(1U, bytes - old);
        if (previous == target.fields.end()) {
            payload_.add(1U, sizeof(std::pair<const std::string, std::size_t>) + 4U * sizeof(void*));
            target.fields.emplace(name, bytes);
        } else previous->second = std::max(old, bytes);
    }

    void addScratch(std::size_t count, std::size_t width, std::size_t dynamicBytes = 0U) {
        std::size_t bytes = 0U;
        if (count > generation::kMaxGeneratedPointCount ||
            !generation::checkedProduct(count, width,
                std::numeric_limits<std::size_t>::max(), bytes) ||
            !generation::checkedSum(bytes, dynamicBytes,
                std::numeric_limits<std::size_t>::max(), bytes))
            throw std::length_error("AttributeCreate input array is excessive");
        addScratchBytes(bytes);
        addWork(count);
    }

    void addScratchBytes(std::size_t bytes) {
        if (bytes > scratch_) payload_.add(1U, bytes - scratch_);
        scratch_ = std::max(scratch_, bytes);
    }

    void addWork(std::size_t count) {
        if (!generation::checkedSum(work_, count, generation::kMaxWorkItemCount, work_))
            throw std::length_error("AttributeCreate work exceeds its safety limit");
    }

private:
    struct Target {
        Target(std::size_t size, ConstDPAttrIter first, ConstDPAttrIter last)
            : count(size), begin(first), end(last) {}
        std::size_t count;
        ConstDPAttrIter begin, end;
        std::map<std::string, std::size_t> fields;
    };
    point::PointPayloadBudget payload_;
    std::vector<Target> targets_;
    std::size_t work_ = 0U, scratch_ = 0U;
};

} // namespace attribute
} // namespace dommy
