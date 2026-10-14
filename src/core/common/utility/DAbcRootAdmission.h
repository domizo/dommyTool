#pragma once

#include "DWorkBudget.h"

#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace dommy {
namespace exporting {

const std::size_t kMaxAbcDagDepth = 256U;
const std::size_t kMaxAbcRootPathBytes = 32768U;

// One index per archive; the command may share its work allowance across jobs.
// Native selection/path materialization is outside this logical storage policy.
class RootAdmission final {
public:
    explicit RootAdmission(std::shared_ptr<generation::WorkBudget> work =
                               std::make_shared<generation::WorkBudget>())
        : work_(std::move(work)), names_(Compare(work_.get()))
    {
        if (!work_) throw std::invalid_argument("Missing Alembic root work budget");
    }

    void admitSelection(std::size_t count)
    {
        if (count > generation::kMaxConnectedGeometryCount)
            throw std::length_error("Too many Alembic selection items");
        consume(count);
    }

    void consume(std::size_t count)
    {
        if (!work_->consume(count))
            throw std::length_error("Alembic root inspection exceeds the work budget");
    }

    bool admit(const std::string& name, std::size_t pathRecordBytes)
    {
        if (name.size() < 2U || name.size() > kMaxAbcRootPathBytes ||
            name.front() != '|' || name.back() == '|')
            throw std::invalid_argument("Invalid Alembic root path");
        consume(name.size() + 1U);
        std::size_t depth = 0U;
        for (std::size_t i = 0U; i < name.size(); ++i) {
            const unsigned char value = static_cast<unsigned char>(name[i]);
            if (value < 0x20U || value == 0x7fU ||
                (value == '|' && i != 0U && name[i - 1U] == '|'))
                throw std::invalid_argument("Invalid Alembic root path component");
            if (value == '|') ++depth;
        }
        if (depth > kMaxAbcDagDepth)
            throw std::length_error("Alembic root path exceeds the depth limit");
        if (names_.find(name) != names_.end()) return false;

        std::size_t nextBytes = 0U, recordBytes = 0U, records = 0U, storage = 0U;
        const std::size_t count = names_.size() + 1U;
        if (count > generation::kMaxGeneratedTransformCount ||
            !generation::checkedSum(bytes_, name.size(),
                generation::kMaxGeneratedStringByteCount, nextBytes) ||
            !memory::checkedAdd(pathRecordBytes, sizeof(std::string) + 6U * sizeof(void*), recordBytes) ||
            !memory::checkedMultiply(count, std::max(maximumRecordBytes_, recordBytes), records) ||
            !memory::checkedAdd(nextBytes, records, storage) ||
            !generation::checkedProduct(storage, 4U, 256U * 1024U * 1024U, storage))
            throw std::length_error("Alembic roots exceed the count/storage budget");

        const std::size_t maximum = std::max(maximumNameBytes_, name.size());
        // Account conservatively for the separate legacy MDagPath set's
        // full-name comparisons/copies before inserting into that container.
        if (!work_->consumeProduct(generation::orderedContainerWork(count), 2U * maximum))
            throw std::length_error("Alembic root insertion exceeds the work budget");
        names_.insert(name);
        bytes_ = nextBytes;
        maximumNameBytes_ = maximum;
        maximumRecordBytes_ = std::max(maximumRecordBytes_, recordBytes);
        return true;
    }

    void validate()
    {
        for (const auto& name : names_) {
            consume(name.size() + 1U);
            const std::string prefix = name + '|';
            const auto descendant = names_.lower_bound(prefix);
            if (descendant == names_.end()) continue;
            consume(prefix.size());
            if (descendant->compare(0U, prefix.size(), prefix) == 0)
                throw std::invalid_argument("Alembic roots have an ancestor relationship: " +
                                            name + " and " + *descendant);
        }
    }

    void requireTemporaryStorage(std::size_t bytes) const
    {
        // Preflight scratch must coexist with the retained root index. This is
        // a logical allowance, not a cap on native host/backend allocations.
        std::size_t records = 0U, storage = 0U;
        if (!memory::checkedMultiply(names_.size(), maximumRecordBytes_, records) ||
            !memory::checkedAdd(bytes_, records, storage) ||
            !memory::checkedAdd(storage, bytes, storage) ||
            !generation::checkedProduct(storage, 4U, 256U * 1024U * 1024U, storage))
            throw std::length_error("Alembic root preflight exceeds the combined storage budget");
    }

    std::size_t size() const { return names_.size(); }
    std::size_t nameBytes() const { return bytes_; }
    generation::WorkBudget& work() { return *work_; }

private:
    struct Compare {
        explicit Compare(generation::WorkBudget* value) : work(value) {}
        bool operator()(const std::string& lhs, const std::string& rhs) const
        {
            if (!work->consume(std::min(lhs.size(), rhs.size()) + 1U))
                throw std::length_error("Alembic root comparisons exceed the work budget");
            return lhs < rhs;
        }
        generation::WorkBudget* work;
    };
    RootAdmission(const RootAdmission&) = delete;
    RootAdmission& operator=(const RootAdmission&) = delete;
    std::shared_ptr<generation::WorkBudget> work_;
    std::set<std::string, Compare> names_;
    std::size_t bytes_ = 0U, maximumNameBytes_ = 0U, maximumRecordBytes_ = 0U;
};

} // namespace exporting
} // namespace dommy
