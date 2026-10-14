#pragma once

#include "DAttributeLookup.h"
#include "DMeshMergePlan.h"
#include "DWorkBudget.h"

#include <array>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dommy {
namespace attribute {

enum class ExportAttributeDomain { Point = 0, Primitive = 1 };

struct ExportAttributeCost {
    std::size_t fields = 0U, nameBytes = 0U, storageBytes = 0U;
};

// Shared by one archive's procedural and Maya attribute writers. This is a conservative metadata
// staging allowance, not a measurement of native archive/geometry memory.
// Export commands use it serially; it is not a concurrent allocator.
class ExportAttributeBudget {
    friend class ExportAttributeState;
public:
    void require(const ExportAttributeCost& next,
                 const ExportAttributeCost& replacing = ExportAttributeCost()) const
    {
        ExportAttributeCost total;
        std::size_t copied = 0U;
        if (used_.fields < replacing.fields || used_.nameBytes < replacing.nameBytes ||
            used_.storageBytes < replacing.storageBytes ||
            !generation::checkedSum(used_.fields - replacing.fields, next.fields,
                generation::kMaxGeneratedStringCount, total.fields) ||
            !generation::checkedSum(used_.nameBytes - replacing.nameBytes, next.nameBytes,
                generation::kMaxGeneratedStringByteCount, total.nameBytes) ||
            !generation::checkedSum(used_.storageBytes - replacing.storageBytes, next.storageBytes,
                mesh::kMaxMeshOperationBytes, total.storageBytes) ||
            !generation::checkedProduct(total.storageBytes, 4U,
                mesh::kMaxMeshOperationBytes, copied))
            throw std::length_error("Alembic attribute schemas exceed the archive metadata budget");
    }

    ExportAttributeCost used() const { return used_; }

    generation::WorkBudget& inspectionWork() { return inspectionWork_; }
    void retainInspectionStorage(std::size_t bytes)
    {
        if (bytes == 0U) throw std::invalid_argument("Empty Alembic inspection storage cost");
        ExportAttributeCost cost;
        cost.storageBytes = bytes;
        acquire(cost);
    }

    // Native properties survive their temporary C++ writers. Retain their
    // allowance until the archive's shared budget dies, even after failure:
    // a throwing backend constructor may already have created native metadata.
    // Such failures require abandoning the archive and starting with fresh JobArgs.
    void retainNativeProperty(const std::string& name, std::size_t recordBytes)
    {
        ExportAttributeCost cost;
        cost.fields = 1U;
        cost.nameBytes = name.size();
        std::size_t fixed = 0U, names = 0U;
        if (!validAttributeName(name) ||
            !memory::checkedMultiply(name.size(), 2U, names) ||
            !memory::checkedAdd(recordBytes, 2U * sizeof(std::string) + 6U * sizeof(void*), fixed) ||
            !memory::checkedAdd(fixed, names, cost.storageBytes))
            throw std::length_error("Invalid or excessive native Alembic attribute metadata");
        acquire(cost);
    }

private:
    void acquire(const ExportAttributeCost& cost)
    {
        require(cost);
        used_.fields += cost.fields;
        used_.nameBytes += cost.nameBytes;
        used_.storageBytes += cost.storageBytes;
    }

    void release(const ExportAttributeCost& cost) noexcept
    {
        used_.fields -= cost.fields;
        used_.nameBytes -= cost.nameBytes;
        used_.storageBytes -= cost.storageBytes;
    }

    ExportAttributeCost used_;
    generation::WorkBudget inspectionWork_;
};

class ExportAttributeState;

class ExportAttributeFrame {
    friend class ExportAttributeState;
public:
    struct Field {
        ExportAttributeDomain domain;
        int type;
        bool sampled;
        bool written;
    };

    void add(const std::string& name, ExportAttributeDomain domain, int type, bool sampled)
    {
        if (!budget_ || !validAttributeName(name) || type < 0 || static_cast<std::size_t>(domain) >= layouts_.size())
            throw std::invalid_argument("Invalid Alembic attribute schema");
        const auto existing = fields_.find(name);
        if (existing != fields_.end())
            throw std::invalid_argument(existing->second.domain != domain
                ? "Alembic primitive/point attribute names collide: " + name
                : "Duplicate Alembic export attribute: " + name);
        ExportAttributeCost candidate = cost_;
        std::size_t bytes = 0U;
        if (!generation::checkedSum(candidate.fields, 1U,
                generation::kMaxGeneratedStringCount, candidate.fields) ||
            !generation::checkedSum(candidate.nameBytes, name.size(),
                generation::kMaxGeneratedStringByteCount, candidate.nameBytes) ||
            !memory::checkedMultiply(name.size(), 2U, bytes) ||
            !memory::checkedAdd(bytes, 2U * (sizeof(std::string) + sizeof(Field) + 3U * sizeof(void*)), bytes) ||
            !memory::checkedAdd(candidate.storageBytes, bytes, candidate.storageBytes))
            throw std::length_error("Alembic attribute schema size overflows");
        budget_->require(candidate, replacing_);
        fields_.emplace(name, Field{domain, type, sampled, false});
        cost_ = candidate;
    }

    bool hasStatic(ExportAttributeDomain domain) const
    {
        for (const auto& field : fields_)
            if (field.second.domain == domain && !field.second.sampled) return true;
        return false;
    }

    // Admit before allocation. Per-partition counts preserve static vertex
    // fields even when the total stays equal but curve partitions change.
    template <typename CountAt>
    void setLayout(ExportAttributeDomain domain, std::size_t count, CountAt countAt)
    {
        const std::size_t index = static_cast<std::size_t>(domain);
        if (index >= layouts_.size() || layoutSet_[index] || !hasStatic(domain) ||
            count > generation::kMaxConnectedGeometryCount)
            throw std::invalid_argument("Invalid static Alembic attribute layout");
        ExportAttributeCost candidate = cost_;
        std::size_t bytes = 0U;
        if (!memory::checkedMultiply(count, sizeof(std::size_t), bytes) ||
            !memory::checkedAdd(candidate.storageBytes, bytes, candidate.storageBytes))
            throw std::length_error("Alembic attribute layout size overflows");
        budget_->require(candidate, replacing_);
        std::vector<std::size_t> layout(count);
        for (std::size_t i = 0U; i < count; ++i) {
            layout[i] = countAt(i);
            if (layout[i] > generation::kMaxGeneratedPointCount)
                throw std::invalid_argument("Invalid Alembic attribute layout count");
        }
        layouts_[index].swap(layout);
        layoutSet_[index] = true;
        cost_ = candidate;
    }

private:
    ExportAttributeFrame() = default;
    ExportAttributeFrame(const ExportAttributeState* owner,
                         std::shared_ptr<ExportAttributeBudget> budget,
                         ExportAttributeCost replacing)
        : owner_(owner), budget_(std::move(budget)), replacing_(replacing) {}
    const ExportAttributeState* owner_ = nullptr;
    std::shared_ptr<ExportAttributeBudget> budget_;
    ExportAttributeCost replacing_, cost_;
    std::map<std::string, Field> fields_;
    std::array<std::vector<std::size_t>, 2> layouts_;
    std::array<bool, 2> layoutSet_{{false, false}};
};

class ExportAttributeState {
public:
    explicit ExportAttributeState(std::shared_ptr<ExportAttributeBudget> budget)
        : budget_(std::move(budget))
    {
        if (!budget_) throw std::invalid_argument("Missing Alembic attribute budget");
    }
    ExportAttributeState(const ExportAttributeState&) = delete;
    ExportAttributeState& operator=(const ExportAttributeState&) = delete;
    ~ExportAttributeState() { budget_->release(cost_); }

    ExportAttributeFrame beginFrame() const { return ExportAttributeFrame(this, budget_, cost_); }
    bool initialized() const { return samples_ != 0U; }
    std::size_t samples() const { return samples_; }

    void prepare(ExportAttributeFrame frame)
    {
        if (frame.owner_ != this || frame.budget_ != budget_)
            throw std::logic_error("Foreign Alembic attribute frame");
        budget_->require(frame.cost_, cost_);
        for (std::size_t index = 0U; index < 2U; ++index)
            if (frame.hasStatic(static_cast<ExportAttributeDomain>(index)) != frame.layoutSet_[index])
                throw std::logic_error("Missing static Alembic attribute layout");
        if (initialized()) {
            if (fields_.size() != frame.fields_.size())
                throw std::invalid_argument("Alembic export attribute set changed between frames");
            for (const auto& field : fields_) {
                const auto current = frame.fields_.find(field.first);
                if (current == frame.fields_.end() || current->second.domain != field.second.domain ||
                    current->second.type != field.second.type || current->second.sampled != field.second.sampled)
                    throw std::invalid_argument("Alembic export attribute schema changed: " + field.first);
            }
            if (layouts_ != frame.layouts_)
                throw std::invalid_argument("Static Alembic attribute element layout changed");
        }
        pending_ = std::move(frame);
        ready_ = true;
    }

    void requireCurrent(const std::string& name, ExportAttributeDomain domain,
                        int type, bool sampled) const
    {
        const auto field = pending_.fields_.find(name);
        if (!ready_ || field == pending_.fields_.end() || field->second.written ||
            field->second.domain != domain || field->second.type != type || field->second.sampled != sampled)
            throw std::logic_error("Alembic attribute serialization does not match its frame plan: " + name);
    }

    void markWritten(const std::string& name) { pending_.fields_.at(name).written = true; }

    void requireComplete(std::size_t propertyCount) const
    {
        if (!ready_ || propertyCount != pending_.fields_.size())
            throw std::logic_error("Incomplete Alembic attribute property registry");
        for (const auto& field : pending_.fields_)
            if (!field.second.written)
                throw std::logic_error("Alembic attribute sample was omitted: " + field.first);
    }

    // Call only after the owning geometry schema succeeds. Native property
    // writes are not rolled back: any failure abandons the whole archive.
    void commit()
    {
        requireComplete(pending_.fields_.size());
        std::size_t next = 0U;
        if (!generation::checkedSum(samples_, 1U, generation::kMaxWorkItemCount, next))
            throw std::length_error("Too many Alembic attribute frames");
        if (!initialized()) {
            budget_->acquire(pending_.cost_);
            fields_.swap(pending_.fields_);
            layouts_.swap(pending_.layouts_);
            cost_ = pending_.cost_;
        }
        samples_ = next;
        ready_ = false;
    }

private:
    std::shared_ptr<ExportAttributeBudget> budget_;
    ExportAttributeCost cost_;
    std::map<std::string, ExportAttributeFrame::Field> fields_;
    std::array<std::vector<std::size_t>, 2> layouts_;
    ExportAttributeFrame pending_;
    std::size_t samples_ = 0U;
    bool ready_ = false;
};

} // namespace attribute
} // namespace dommy
