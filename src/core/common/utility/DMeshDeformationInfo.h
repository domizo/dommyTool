#pragma once

#include "DAttributeArrayConversion.h"
#include "DAttributePublication.h"
#include "DAttributeValidation.h"
#include "DDirectionMath.h"
#include "DPointMean.h"
#include "DPointTransform.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace dommy {
namespace mesh {

struct DeformationInfo {
    std::vector<double> distances;
    std::vector<Imath::V3f> vectors;
};

// Compare like-indexed world vertices. Keep historical normalized defDist and
// float defVec storage, with an explicit zero result for unchanged vertices.
template <typename Points, typename Matrix>
bool buildDeformationInfo(const Points& base, const Matrix& baseMatrix,
                           const Points& target, const Matrix& targetMatrix,
                           double vectorScale, bool normalize, DeformationInfo& output)
{
    if (base.size() != target.size() || !std::isfinite(vectorScale) ||
        !attribute::isFiniteMatrix(baseMatrix) || !attribute::isFiniteMatrix(targetMatrix) ||
        !attribute::validArrayConversionBudget(target.size(), sizeof(double) + sizeof(Imath::V3f)))
        return false;
    DeformationInfo candidate;
    candidate.distances.resize(target.size());
    candidate.vectors.resize(target.size());
    double maximumDistance = 0.0;
    for (std::size_t i = 0U; i < target.size(); ++i) {
        std::array<double, 3> from, to;
        direction::Delta delta;
        if (!numeric::checkedTransformPoint(base[i], baseMatrix, from) ||
            !numeric::checkedTransformPoint(target[i], targetMatrix, to) ||
            !direction::difference(from, to, delta)) return false;
        candidate.distances[i] = delta.length;
        maximumDistance = std::max(maximumDistance, delta.length);
        if (normalize && delta.length != 0.0)
            for (double& component : delta.values) component /= delta.length;
        if (!direction::adjust(delta.values, false, false, vectorScale, candidate.vectors[i])) return false;
    }
    if (maximumDistance != 0.0)
        for (double& distance : candidate.distances) distance /= maximumDistance;
    output = std::move(candidate);
    return true;
}

// Average only the incident faces that contracted/expanded, preserving the
// original red/green meaning. Maya face areas are acquired once per face.
template <typename Faces>
bool vertexAreaChange(const std::vector<double>& baseAreas, const std::vector<double>& targetAreas,
                       const Faces& faces, std::size_t faceCount, double scale, Imath::V3f& output)
{
    if (baseAreas.size() != targetAreas.size() || !std::isfinite(scale) ||
        !attribute::validArrayConversionBudget(faceCount, 2U * sizeof(double))) return false;
    std::vector<double> compression, expansion;
    for (std::size_t i = 0U; i < faceCount; ++i) {
        std::size_t face = 0U;
        if (!numeric::checkedScalarCast(faces[i], face) || face >= baseAreas.size()) return false;
        const double base = baseAreas[face], target = targetAreas[face];
        if (!std::isfinite(base) || !std::isfinite(target) || base < 0.0 || target < 0.0) return false;
        if (base > target) compression.push_back(base - target);
        else if (target > base) expansion.push_back(target - base);
    }
    double means[3] = {0.0, 0.0, 0.0};
    if ((!compression.empty() && !numeric::checkedScalarMean(compression, means[0])) ||
        (!expansion.empty() && !numeric::checkedScalarMean(expansion, means[1]))) return false;
    return direction::adjust(means, false, false, scale, output);
}

// The owner is a detached output primitive. A later failure must cause the
// caller to discard the entire Maya output object, not publish partial fields.
template <typename Owner>
bool publishDeformationInfo(Owner& owner, const DeformationInfo& info,
                             const std::vector<Imath::V3f>& areas, std::size_t count)
{
    if (info.distances.size() != count || info.vectors.size() != count || areas.size() != count ||
        !attribute::validArrayConversionBudget(count, sizeof(double) + 2U * sizeof(Imath::V3f))) return false;
    const DPAttributeData::Ptr distance(new DPDoubleAttr(info.distances));
    const DPAttributeData::Ptr vector(new DPVec3fAttr(info.vectors));
    const DPAttributeData::Ptr area(new DPVec3fAttr(areas));
    attribute::ValidationBudget budget;
    for (const auto& field : {distance, vector, area})
        if (!attribute::validateAttribute(field, count, budget)) return false;
    return attribute::commitStagedAttribute(owner, "defDist", distance, count) &&
        attribute::commitStagedAttribute(owner, "defVec", vector, count) &&
        attribute::commitStagedAttribute(owner, "defArea", area, count);
}

} // namespace mesh
} // namespace dommy
