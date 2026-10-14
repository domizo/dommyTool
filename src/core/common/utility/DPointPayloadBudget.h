#pragma once

#include "common/utility/DAttributeArrayConversion.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace dommy {
namespace point {

// Aggregate host payload with the existing 256 MiB/four-copy allowance.
// This is not a peak-memory cap for native workspaces or allocator overhead.
class PointPayloadBudget {
  public:
    void add(std::size_t count, std::size_t elementBytes) {
        std::size_t bytes = 0U, total = 0U;
        if (!dommy::generation::checkedProduct(count, elementBytes,
                                               std::numeric_limits<std::size_t>::max(), bytes) ||
            !dommy::generation::checkedSum(bytes_, bytes, std::numeric_limits<std::size_t>::max(),
                                           total) ||
            !dommy::attribute::validArrayConversionBudget(1U, total))
            throw std::length_error("DPoint workspace exceeds its allocation budget");
        bytes_ = total;
    }

    // add() bounds payload to one quarter of 256 MiB, so this cannot overflow.
    std::size_t estimatedBytes() const { return bytes_ * 4U; }

  private:
    std::size_t bytes_ = 0U;
};

inline std::size_t pointAttributeStorageBytes(DP_ATTR_DATA_TYPE type) {
    switch (type) {
    case DP_ATTR_BOOL:
        return sizeof(bool);
    case DP_ATTR_INT:
        return sizeof(int32_t);
    case DP_ATTR_UINT32:
        return sizeof(uint32_t);
    case DP_ATTR_INT64:
        return sizeof(int64_t);
    case DP_ATTR_FLOAT:
        return sizeof(float);
    case DP_ATTR_DOUBLE:
        return sizeof(double);
    case DP_ATTR_VEC2I:
        return sizeof(Imath::V2i);
    case DP_ATTR_VEC2F:
        return sizeof(Imath::V2f);
    case DP_ATTR_VEC2D:
        return sizeof(Imath::V2d);
    case DP_ATTR_VEC3I:
        return sizeof(Imath::V3i);
    case DP_ATTR_VEC3F:
        return sizeof(Imath::V3f);
    case DP_ATTR_VEC3D:
        return sizeof(Imath::V3d);
    case DP_ATTR_STRING:
        return sizeof(std::string);
    case DP_ATTR_MATRIX:
        return sizeof(MMatrix);
    case DP_ATTR_INT_ARRAY:
        return sizeof(DIntArray);
    default:
        return 0U;
    }
}

template <typename Iterator>
void preflightAttributePayload(Iterator begin, Iterator end, std::size_t count,
                               PointPayloadBudget& workspace, bool retainDefaultValue = true) {
    dommy::attribute::ValidationBudget cost;
    for (auto field = begin; field != end; ++field) {
        if (!field->second || !dommy::attribute::validAttributeName(field->first))
            throw std::invalid_argument("DPoint input attribute is invalid");
        const std::size_t width = pointAttributeStorageBytes(field->second->getAttrType());
        if (width == 0U)
            throw std::invalid_argument("DPoint cannot validate an input attribute type");
        workspace.add(count, width);
        workspace.add(1U, width);
        workspace.add(field->first.size(), sizeof(char));
        const std::size_t previousWork = cost.workItems;
        const std::size_t previousStrings = cost.stringBytes;
        // A value-only transfer constructs the historical type default. The
        // source default is validated, but its dynamic storage is not copied.
        if (!dommy::attribute::validateAttribute(field->second, count, cost, retainDefaultValue))
            throw std::invalid_argument("DPoint input attribute values are invalid");
        workspace.add(cost.stringBytes - previousStrings, sizeof(char));
        workspace.add(cost.workItems - previousWork - count, sizeof(int));
    }
}

template <typename Iterator>
void preflightGroupPayload(Iterator begin, Iterator end, PointPayloadBudget& workspace) {
    for (auto group = begin; group != end; ++group) {
        if (!group->second)
            throw std::invalid_argument("DPoint input group is invalid");
        workspace.add(group->second->getGroupIds().size(), sizeof(bool));
        workspace.add(group->first.size(), sizeof(char));
    }
}

template <typename Owner>
void preflightMetadataPayload(const Owner& input, std::size_t count,
                              PointPayloadBudget& workspace) {
    preflightAttributePayload(input.beginAttr(), input.endAttr(), count, workspace);
    preflightGroupPayload(input.beginGrp(), input.endGrp(), workspace);
}

template <typename Owner>
void preflightPointPayload(const Owner& input, std::size_t pointBytes,
                           PointPayloadBudget& workspace) {
    const std::size_t count = input.numberOfPoints();
    if (count > dommy::generation::kMaxGeneratedPointCount)
        throw std::length_error("DPoint count exceeds its safety limit");
    workspace.add(count, pointBytes);
    // Existing topology validation also rejects nonfinite point coordinates.
    if (!input.hasValidPointTopology())
        throw std::invalid_argument("DPoint input topology is invalid");
    // Transactional output copies retain all source fields/defaults/groups.
    preflightMetadataPayload(input, count, workspace);
    for (auto group = input.beginGrp(); group != input.endGrp(); ++group)
        if (group->second->getGroupIds().size() != count)
            throw std::invalid_argument("DPoint input group is invalid");
}

inline void preflightGeneratedPointAttribute(std::size_t count, const std::string& name,
                                             std::size_t width, PointPayloadBudget& workspace) {
    if (width == 0U) return; // No generated field.
    workspace.add(count, width);
    workspace.add(1U, width);
    workspace.add(name.size(), sizeof(char));
}

// Admit retained point metadata and scratch across the entire collection,
// including defaults/names of generated fields on empty primitives. Native
// query workspaces and mesh/curve topology are outside this point estimate.
template <typename Owner>
void preflightPrimitivePointPayload(const Owner& input, std::size_t primitiveCount,
                                    std::size_t pointBytes, const std::string& generatedName,
                                    std::size_t generatedWidth, PointPayloadBudget& workspace) {
    workspace.add(primitiveCount, sizeof(std::vector<Imath::V3f>));
    preflightMetadataPayload(input, primitiveCount, workspace);
    for (auto item = input.beginPrims(); item != input.endPrims(); ++item) {
        if (!*item) throw std::invalid_argument("DPoint primitive is null");
        const std::size_t count = (*item)->numberOfPoints();
        workspace.add(count, pointBytes);
        preflightMetadataPayload(**item, count, workspace);
        preflightGeneratedPointAttribute(count, generatedName, generatedWidth, workspace);
    }
}
} // namespace point
} // namespace dommy
