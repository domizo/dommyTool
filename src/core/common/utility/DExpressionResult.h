#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributeLookup.h"
#include "DAttributePublication.h"
#include "DExpressionEvaluation.h"

namespace dommy {
namespace expression {

template <typename Expression, typename Points>
bool evaluateResults(Expression& expression, const Points& points,
                     std::vector<Imath::V3d>& results) {
    if (!attribute::validArrayConversionBudget(points.size(), sizeof(Imath::V3d)))
        return false;
    std::vector<Imath::V3d> candidate;
    candidate.reserve(points.size());
    for (std::size_t i = 0U; i < points.size(); ++i) {
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!std::isfinite(points[i][axis]))
                return false;
        expression.setId(i);
        expression.setPos(points[i]);
        Imath::V3d value;
        if (!evaluateChecked(expression, value))
            return false;
        candidate.push_back(value);
    }
    results.swap(candidate);
    return true;
}

// Preserve the node's serialized storage contract: scalar=double, vec2/3=float.
// Only convert once, at the final destination, after the expression succeeds.
inline bool buildResult(const std::vector<Imath::V3d>& values, int dimension,
                        DPAttributeData::Ptr& output) {
    if (dimension < 1 || dimension > 3 ||
        !attribute::validArrayConversionBudget(values.size(), sizeof(Imath::V3d)))
        return false;
    for (const auto& value : values)
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!std::isfinite(value[axis]))
                return false;
    DPAttributeData::Ptr candidate;
    if (dimension == 1) {
        std::vector<double> scalars;
        scalars.reserve(values.size());
        for (const auto& value : values)
            scalars.push_back(value[0]);
        candidate = std::make_shared<DPDoubleAttr>(scalars);
    } else if (dimension == 2) {
        std::vector<Imath::V2f> vectors;
        vectors.reserve(values.size());
        for (const auto& value : values) {
            Imath::V2f converted;
            if (!numeric::checkedValueCast(Imath::V2d(value[0], value[1]), converted))
                return false;
            vectors.push_back(converted);
        }
        candidate = std::make_shared<DPVec2fAttr>(vectors);
    } else {
        std::vector<Imath::V3f> vectors;
        vectors.reserve(values.size());
        for (const auto& value : values) {
            Imath::V3f converted;
            if (!numeric::checkedValueCast(value, converted))
                return false;
            vectors.push_back(converted);
        }
        candidate = std::make_shared<DPVec3fAttr>(vectors);
    }
    output.swap(candidate);
    return true;
}

// The expression may read the old field or update other fields on this staged
// owner. Commit the generated field only after evaluation and conversion.
template <typename Owner>
bool publishResult(Owner& owner, const std::string& name, int dimension,
                   const std::vector<Imath::V3d>& values) {
    if (!attribute::validAttributeName(name) ||
        name.size() > generation::kMaxGeneratedStringByteCount ||
        values.size() != owner.numberOfPoints())
        return false;
    DPAttributeData::Ptr candidate;
    if (!buildResult(values, dimension, candidate))
        return false;
    return attribute::commitStagedAttribute(owner, name, candidate, values.size());
}

} // namespace expression
} // namespace dommy
