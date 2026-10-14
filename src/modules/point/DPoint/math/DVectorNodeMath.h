#pragma once

#include "DUnaryMath.h"
#include "common/utility/DAttributeMathInput.h"
#include "common/utility/DAttributeMathPublication.h"
#include "common/utility/DPointMeanAccumulator.h"
#include "common/utility/DGenerationLimits.h"

namespace dommy {
namespace math {

enum class VectorNodeKind { Length, Minimum, Maximum, Average, Dot, Angle, Cross };

struct VectorNodeInput {
    DMathVec3DataList values;
    unsigned int components = 3U;
};

template <typename Owner>
bool readVectorNodeAttribute(const Owner& owner, const std::string& name,
                             std::size_t count, VectorNodeInput& output)
{
    if (!validMathBudget(count, 1U)) return false;
    const auto input = owner.getAttr(name);
    if (!input) return false;
    VectorNodeInput candidate;
    switch (input->getAttrType()) {
    case DP_ATTR_VEC2I: case DP_ATTR_VEC2F: case DP_ATTR_VEC2D: {
        DMathVec2DataList values;
        if (!attribute::readNumericAttributeValues(owner, name, count, values)) return false;
        candidate.components = 2U;
        candidate.values.reserve(count);
        for (const auto& v : values) candidate.values.emplace_back(v[0], v[1], 0.0);
        break;
    }
    case DP_ATTR_VEC3I: case DP_ATTR_VEC3F: case DP_ATTR_VEC3D:
        if (!attribute::readNumericAttributeValues(owner, name, count, candidate.values)) return false;
        break;
    default: return false;
    }
    output = std::move(candidate);
    return true;
}

inline bool finiteVectorNodeValue(const DMathVec3Data& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

inline DMathVec3Data normalizedVectorNodeValue(const DMathVec3Data& value)
{
    DMathVec3Data result(0.0, 0.0, 0.0);
    // Input finiteness is checked before this helper. Zero retains the legacy
    // zero-vector rule; scale-safe normalization handles extreme magnitudes.
    normalized3OrZero(value[0], value[1], value[2], result[0], result[1], result[2]);
    return result;
}

inline bool vectorNodeValue(VectorNodeKind kind, const DMathVec3Data& source,
                             unsigned int dimensions, const DMathVec3Data& target,
                             double& result)
{
    if ((dimensions != 2U && dimensions != 3U) || !finiteVectorNodeValue(source) ||
        !finiteVectorNodeValue(target)) return false;
    double value = 0.0;
    switch (kind) {
    case VectorNodeKind::Length:
        value = std::hypot(std::hypot(source[0], source[1]), source[2]);
        break;
    case VectorNodeKind::Minimum:
    case VectorNodeKind::Maximum:
        value = source[0];
        for (unsigned int axis = 1U; axis < dimensions; ++axis)
            value = kind == VectorNodeKind::Minimum ? std::min(value, source[axis])
                                                   : std::max(value, source[axis]);
        break;
    case VectorNodeKind::Average: {
        // Scaling can discard a tiny component before large terms cancel.
        // Reuse the exact mean grid, without allocating a component copy.
        numeric::detail::PointMeanAccumulator sum;
        for (unsigned int axis = 0U; axis < dimensions; ++axis)
            if (!sum.add(source[axis])) return false;
        if (!sum.mean(dimensions, value)) return false;
        break;
    }
    case VectorNodeKind::Dot:
    case VectorNodeKind::Angle: {
        const auto a = normalizedVectorNodeValue(source);
        const auto b = normalizedVectorNodeValue(target);
        const double dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
        // These nodes historically use normalized dot/cross and radians.
        const bool zero = (a[0] == 0.0 && a[1] == 0.0 && a[2] == 0.0) ||
                          (b[0] == 0.0 && b[1] == 0.0 && b[2] == 0.0);
        value = kind == VectorNodeKind::Dot ? dot : (zero ? 0.0 : acosClamped(dot));
        break;
    }
    default: return false;
    }
    return numeric::checkedNumericCast(value, result);
}

inline bool vectorNodeValue(VectorNodeKind kind, const DMathVec3Data& source,
                             unsigned int, const DMathVec3Data& target,
                             DMathVec3Data& result)
{
    if (kind != VectorNodeKind::Cross || !finiteVectorNodeValue(source) ||
        !finiteVectorNodeValue(target)) return false;
    const auto a = normalizedVectorNodeValue(source);
    const auto b = normalizedVectorNodeValue(target);
    const DMathVec3Data value(a[1]*b[2] - a[2]*b[1],
                             a[2]*b[0] - a[0]*b[2],
                             a[0]*b[1] - a[1]*b[0]);
    return numeric::checkedValueCast(value, result);
}

template <VectorNodeKind Kind> struct VectorNodeOperation {
    using Result = typename std::conditional<Kind == VectorNodeKind::Cross,
                                             DMathVec3Data, double>::type;
    static const bool binary = Kind == VectorNodeKind::Dot ||
        Kind == VectorNodeKind::Angle || Kind == VectorNodeKind::Cross;
    bool operator()(const DMathVec3Data& source, unsigned int dimensions,
                    const DMathVec3Data& target, Result& result) const {
        return vectorNodeValue(Kind, source, dimensions, target, result);
    }
};

// A flat Maya target is sliced per primitive; only a constant may broadcast.
template <typename Operation>
bool evaluateVectorNode(const VectorNodeInput& source, const DMathVec3DataList& target,
                         bool broadcast, std::size_t offset, Operation operation,
                         std::vector<typename Operation::Result>& output)
{
    const std::size_t count = source.values.size();
    if ((source.components != 2U && source.components != 3U) ||
        !validMathBudget(count, Operation::binary ? 1U : 0U)) return false;
    if (Operation::binary && (broadcast ? target.size() != 1U :
        (offset > target.size() || count > target.size() - offset))) return false;
    if (Operation::binary && broadcast && !finiteVectorNodeValue(target[0])) return false;
    std::vector<typename Operation::Result> candidate(count);
    const DMathVec3Data zero(0.0, 0.0, 0.0);
    for (std::size_t i = 0U; i < count; ++i) {
        const auto& a = source.values[i];
        const auto& b = Operation::binary ? target[broadcast ? 0U : offset + i] : zero;
        if (!finiteVectorNodeValue(a) || !finiteVectorNodeValue(b) ||
            !operation(a, source.components, b, candidate[i])) return false;
    }
    output.swap(candidate);
    return true;
}

template <typename Owner, typename Operation>
bool processVectorNodeAttribute(Owner& owner, std::size_t count,
                                 const std::string& sourceName, const std::string& outputName,
                                 int targetType, const std::string& targetName,
                                 const DMathVec3DataList& flatTarget, std::size_t offset,
                                 Operation operation)
{
    VectorNodeInput source, target;
    if (!readVectorNodeAttribute(owner, sourceName, count, source)) return false;
    if (Operation::binary && (targetType < 0 || targetType > 2)) return false;
    if (Operation::binary && targetType == 1 &&
        !readVectorNodeAttribute(owner, targetName, count, target)) return false;
    std::vector<typename Operation::Result> result;
    if (!evaluateVectorNode(source, targetType == 1 ? target.values : flatTarget,
            targetType == 0, targetType == 1 ? 0U : offset, operation, result)) return false;
    // Only this staged owner is updated. Never delete attributes on other
    // curves/meshes, including when outputName aliases either input name.
    return attribute::writeNumericAttributeValues(
        owner, true, false, outputName, sourceName, result);
}

template <typename Iterator>
bool vectorNodePartitionCount(Iterator begin, Iterator end, std::size_t& count)
{
    std::size_t points = 0U, partitions = 0U;
    for (auto it = begin; it != end; ++it) {
        if (!*it || !generation::checkedSum(points, (*it)->numberOfPoints(),
                generation::kMaxGeneratedPointCount, points) ||
            !generation::checkedSum(partitions, 1U,
                generation::kMaxGeneratedPointCount, partitions)) return false;
    }
    if (!validMathBudget(points, 1U, partitions)) return false;
    count = points;
    return true;
}

template <typename Iterator, typename Operation>
bool processVectorNodePartitions(Iterator begin, Iterator end,
                                  const std::string& sourceName, const std::string& outputName,
                                  int targetType, const std::string& targetName,
                                  const DMathVec3DataList& flatTarget, Operation operation)
{
    std::size_t count = 0U;
    if (!attribute::validAttributeName(sourceName) || !attribute::validAttributeName(outputName) ||
        !vectorNodePartitionCount(begin, end, count)) return false;
    if (Operation::binary && (targetType < 0 || targetType > 2 ||
        (targetType == 0 && (flatTarget.size() != 1U || !finiteVectorNodeValue(flatTarget[0]))) ||
        (targetType == 1 && !attribute::validAttributeName(targetName)) ||
        (targetType == 2 && flatTarget.size() != count))) return false;
    std::size_t offset = 0U;
    for (auto it = begin; it != end; ++it) {
        const std::size_t size = (*it)->numberOfPoints();
        if (!processVectorNodeAttribute(**it, size, sourceName, outputName,
                targetType, targetName, flatTarget, offset, operation)) return false;
        offset += size;
    }
    return true;
}

} // namespace math
} // namespace dommy
