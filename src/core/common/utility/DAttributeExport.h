#pragma once

#include "DAttributeArrayConversion.h"

#include <algorithm>

namespace dommy {
namespace attribute {

// A collection primitive field has one value per source mesh. A merged
// PolyMesh uniform field instead needs one value per face, in topology order.
template <typename Source, typename Destination, typename Iterator>
bool buildPrimitiveFaceAttributeValues(const DPAttributeData::ConstPtr& input,
                                      Iterator first, Iterator last,
                                      std::vector<Destination>& output)
{
    std::size_t partitions = 0U, faces = 0U;
    for (auto owner = first; owner != last; ++owner) {
        if (!*owner || (*owner)->numFace() < 0 ||
            !generation::checkedSum(partitions, 1U,
                generation::kMaxGeneratedMeshCount, partitions) ||
            !generation::checkedSum(faces, static_cast<std::size_t>((*owner)->numFace()),
                generation::kMaxGeneratedFaceCount, faces)) return false;
    }
    ValidationBudget budget;
    std::size_t primitiveBytes = 0U, faceBytes = 0U, totalBytes = 0U;
    if (!validateTypedAttribute<Source>(input, partitions, budget) ||
        !memory::checkedMultiply(partitions, sizeof(Destination), primitiveBytes) ||
        !memory::checkedMultiply(faces, sizeof(Destination), faceBytes) ||
        !memory::checkedAdd(primitiveBytes, faceBytes, totalBytes) ||
        !validArrayConversionBudget(1U, totalBytes)) return false;
    std::vector<Destination> primitives;
    if (!convertTypedArray<Source, Destination>(input, partitions, primitives)) return false;
    std::vector<Destination> candidate;
    candidate.reserve(faces);
    std::size_t index = 0U;
    for (auto owner = first; owner != last; ++owner, ++index) {
        if (!*owner || (*owner)->numFace() < 0 || index >= primitives.size()) return false;
        const std::size_t count = static_cast<std::size_t>((*owner)->numFace());
        if (candidate.size() > faces || count > faces - candidate.size()) return false;
        candidate.insert(candidate.end(), count, primitives[index]);
    }
    if (index != partitions || candidate.size() != faces) return false;
    output.swap(candidate);
    return true;
}

// Export one vertex-scoped field without compacting missing partitions. The
// first present field supplies the default for absent owners. Present fields
// must have the same type and exact point cardinality. Publish only on success.
template <typename Source, typename Destination, typename Iterator>
bool buildExportAttributeValues(Iterator first, Iterator last,
                                DP_ATTR_DATA_TYPE type, const std::string& name,
                                std::vector<Destination>& output)
{
    if (!validAttributeName(name)) return false;
    std::size_t total = 0U, partitions = 0U;
    ValidationBudget budget;
    Destination defaultValue;
    bool foundDefault = false;
    for (auto owner = first; owner != last; ++owner) {
        if (!*owner ||
            !generation::checkedSum(partitions, 1U,
                generation::kMaxConnectedGeometryCount, partitions) ||
            !generation::checkedSum(total, (*owner)->numberOfPoints(),
                generation::kMaxGeneratedPointCount, total)) return false;
        const DPAttributeData::ConstPtr attribute = (*owner)->getAttr(name);
        if (!attribute) continue;
        if (attribute->getAttrType() != type ||
            !validateTypedAttribute<Source>(attribute, (*owner)->numberOfPoints(), budget))
            return false;
        if (!foundDefault) {
            const auto typed = std::dynamic_pointer_cast<const TypedDPAttributeData<Source>>(attribute);
            if (!typed || !numeric::checkedValueCast(typed->getDefaultValue(), defaultValue))
                return false;
            foundDefault = true;
        }
    }
    if (!foundDefault || !validArrayConversionBudget(total, sizeof(Destination)) ||
        !validArrayConversionBudget(total, sizeof(Source))) return false;

    std::vector<Destination> candidate(total, defaultValue);
    std::size_t offset = 0U;
    for (auto owner = first; owner != last; ++owner) {
        const std::size_t count = (*owner)->numberOfPoints();
        // Defend the write boundary as well as the preflight.
        if (offset > candidate.size() || count > candidate.size() - offset) return false;
        const DPAttributeData::ConstPtr attribute = (*owner)->getAttr(name);
        if (attribute) {
            std::vector<Destination> converted;
            if (attribute->getAttrType() != type ||
                !convertTypedArray<Source, Destination>(attribute, count, converted)) return false;
            std::copy(converted.begin(), converted.end(), candidate.begin() + offset);
        }
        offset += count;
    }
    if (offset != candidate.size()) return false;
    output.swap(candidate);
    return true;
}

} // namespace attribute
} // namespace dommy
