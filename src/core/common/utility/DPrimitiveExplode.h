#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributeLookup.h"
#include "DAttributeValidation.h"
#include "DPointMean.h"
#include "DPointTransform.h"
#include "DGroupPattern.h"

#include <array>
#include <cmath>
#include <iterator>
#include <string>
#include <vector>

namespace dommy {
namespace geometry {

struct ExplodeSettings {
    double scale = 1.0;
    Imath::V3d center = Imath::V3d(0.0, 0.0, 0.0);
    short vectorType = 0;
    std::string centerAttribute, vectorAttribute, groupPattern;
};

inline bool scaledExplodeOffset(double center, double origin, double scale, double& output)
{
    if (!std::isfinite(center) || !std::isfinite(origin) || !std::isfinite(scale)) return false;
    const long double a = center, b = origin, s = scale;
    const long double difference = a - b;
    // A finite displacement may have an overflowing unscaled difference on
    // 53-bit-long-double hosts. Halve only that case, then restore the exponent.
    const long double value = std::isfinite(difference) ? difference * s
        : ((a * 0.5L - b * 0.5L) * s) * 2.0L;
    return numeric::checkedNumericCast(value, output);
}

// Inputs are world-space vectors. Keep the linear part of the primitive's
// transform and compose the translation after it; no inverse is required,
// including for reflected, sheared, or zero-scale primitives.
template <typename Matrix, typename Vector>
bool translatedWorldMatrix(const Matrix& matrix, const Vector& offset, Matrix& output)
{
    if (!attribute::isFiniteMatrix(matrix)) return false;
    Matrix candidate(matrix);
    for (unsigned int axis = 0U; axis < 3U; ++axis) {
        if (!std::isfinite(offset[axis]) ||
            !numeric::checkedNumericCast(static_cast<long double>(matrix[3][axis]) +
                offset[axis], candidate[3][axis])) return false;
    }
    output = candidate;
    return true;
}

// Each group is averaged from the original geometric centers, not previously
// overwritten groups or the optional center attribute. Preserve last-match wins.
template <typename Mask>
bool replaceGroupCenter(const std::vector<Imath::V3d>& geometricCenters,
                         const Mask& mask, std::vector<Imath::V3d>& centers)
{
    if (mask.size() != geometricCenters.size() || centers.size() != mask.size() ||
        !attribute::validArrayConversionBudget(mask.size(), sizeof(Imath::V3d))) return false;
    std::vector<Imath::V3d> selected;
    selected.reserve(mask.size());
    for (std::size_t i = 0U; i < mask.size(); ++i)
        if (mask[i]) selected.push_back(geometricCenters[i]);
    if (selected.empty()) return true;
    std::array<double, 3> mean;
    if (!numeric::checkedPointMean(selected, mean)) return false;
    for (std::size_t i = 0U; i < mask.size(); ++i)
        if (mask[i]) centers[i] = Imath::V3d(mean[0], mean[1], mean[2]);
    return true;
}

// Plan before copying/publishing Maya plug-in data. A failed later primitive or
// attribute must leave both input data and the previous plan untouched.
template <typename Owner, typename Matrix>
bool buildExplodeTransforms(const Owner& owner, const ExplodeSettings& settings,
                            std::vector<Matrix>& output)
{
    const std::size_t count = static_cast<std::size_t>(
        std::distance(owner.beginPrims(), owner.endPrims()));
    if (count > generation::kMaxGeneratedMeshCount ||
        !attribute::validArrayConversionBudget(count, sizeof(Matrix) + 3U * sizeof(Imath::V3d)) ||
        !std::isfinite(settings.scale) || settings.vectorType < 0 || settings.vectorType > 1 ||
        !group::validPatternInput(settings.groupPattern)) return false;
    for (unsigned int axis = 0U; axis < 3U; ++axis)
        if (!std::isfinite(settings.center[axis])) return false;
    for (const auto* name : {&settings.centerAttribute, &settings.vectorAttribute})
        if (!name->empty() && !attribute::validAttributeName(*name)) return false;

    std::size_t work = 0U;
    std::vector<Imath::V3d> centers;
    centers.reserve(count);
    for (auto it = owner.beginPrims(); it != owner.endPrims(); ++it) {
        if (!*it || !generation::checkedSum(work, (*it)->numberOfPoints(),
                generation::kMaxWorkItemCount, work)) return false;
        if (settings.vectorType == 0) {
            const auto center = (*it)->getCenterDouble();
            centers.push_back(Imath::V3d(center[0], center[1], center[2]));
        }
    }
    std::vector<Imath::V3d> directions(count, Imath::V3d(0.0, 0.0, 0.0));
    if (settings.vectorType == 0) {
        const std::vector<Imath::V3d> geometricCenters(centers);
        const auto attributeData = owner.getAttr(settings.centerAttribute);
        if (attributeData && !attribute::convertVector3Attribute(attributeData, count, centers))
            return false;
        if (!settings.groupPattern.empty()) {
            if (!group::forEachMatchingGroup(owner.beginGrp(), owner.endGrp(), settings.groupPattern,
                    [&](decltype(owner.beginGrp()) matched) -> bool {
                        if (!matched->second || !generation::checkedSum(work, count + 1U,
                                generation::kMaxWorkItemCount, work)) return false;
                        return replaceGroupCenter(geometricCenters, matched->second->getGroupIds(), centers);
                    })) return false;
        }
        for (std::size_t i = 0U; i < count; ++i)
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                if (!scaledExplodeOffset(centers[i][axis], settings.center[axis],
                        settings.scale, directions[i][axis])) return false;
    } else {
        const auto attributeData = owner.getAttr(settings.vectorAttribute);
        if (attributeData && !attribute::convertVector3Attribute(attributeData, count, directions))
            return false;
        for (auto& direction : directions)
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                if (!numeric::checkedNumericCast(static_cast<long double>(direction[axis]) *
                        settings.scale, direction[axis])) return false;
    }

    std::vector<Matrix> candidate(count);
    std::size_t index = 0U;
    for (auto it = owner.beginPrims(); it != owner.endPrims(); ++it, ++index) {
        const auto* points = (*it)->getPositionPtrConst();
        math::Bounds3d bounds;
        if (!points || !translatedWorldMatrix((*it)->getTransformMatrix(), directions[index],
                candidate[index]) || !numeric::appendTransformedBounds(*points, candidate[index], bounds))
            return false;
    }
    output.swap(candidate);
    return true;
}

} // namespace geometry
} // namespace dommy
