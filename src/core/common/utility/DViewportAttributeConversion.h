#pragma once

#include "DAttributeValueConversion.h"
#include "DGeometryLayout.h"
#include "DViewportGeometryBudget.h"
#include "../color/DColorOperation.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace dommy {
namespace viewport {

// These are display layouts, not conversions of the stored attribute schema.
enum class AttributeDisplayMode {
    Scalar, ScalarVector, ScalarColor, ScalarLuminanceColor,
    Vector2, Vector2XZ, Vector2Color, Vector3, Vector3Color
};

inline unsigned int attributeTupleSize(AttributeDisplayMode mode)
{
    switch (mode) {
    case AttributeDisplayMode::Scalar: return 1U;
    case AttributeDisplayMode::Vector2: return 2U;
    case AttributeDisplayMode::ScalarVector:
    case AttributeDisplayMode::Vector2XZ:
    case AttributeDisplayMode::Vector3: return 3U;
    case AttributeDisplayMode::ScalarColor:
    case AttributeDisplayMode::ScalarLuminanceColor:
    case AttributeDisplayMode::Vector2Color:
    case AttributeDisplayMode::Vector3Color: return 4U;
    }
    throw std::invalid_argument("Unknown viewport attribute layout.");
}

inline std::size_t attributeScalarCount(std::size_t count, unsigned int dimension)
{
    std::size_t scalars = 0U;
    if (count > kMaxViewportVertexCount ||
        !geometry::checkedTupleScalarCount(count, dimension, scalars) ||
        scalars > kMaxViewportScalarCount)
        throw std::length_error("Viewport attribute buffer exceeds its budget.");
    return scalars;
}

template <typename T>
std::array<float, 4> attributeTuple(const T& value, AttributeDisplayMode mode,
                                  bool blackTransparent)
{
    using Components = numeric::ValueComponents<T>;
    const unsigned int required =
        mode == AttributeDisplayMode::Vector2 || mode == AttributeDisplayMode::Vector2XZ ||
        mode == AttributeDisplayMode::Vector2Color ? 2U :
        (mode == AttributeDisplayMode::Vector3 || mode == AttributeDisplayMode::Vector3Color ? 3U : 1U);
    if (Components::count != required)
        throw std::invalid_argument("Viewport attribute type does not match its layout.");

    std::array<float, 4> tuple{{0.0f, 0.0f, 0.0f, 1.0f}};
    std::array<float, 3> converted{{0.0f, 0.0f, 0.0f}};
    std::array<double, 3> original{{0.0, 0.0, 0.0}};
    for (unsigned int axis = 0U; axis < required; ++axis) {
        if (!numeric::checkedScalarCast(Components::get(value, axis), converted[axis]))
            throw std::range_error("Viewport attribute is not representable as a finite float.");
        // Preserve native source-to-float rounding after range validation.
        // A 53-bit long double can double-round large int64 values at float ties.
        converted[axis] = static_cast<float>(Components::get(value, axis));
        original[axis] = static_cast<double>(Components::get(value, axis));
    }
    switch (mode) {
    case AttributeDisplayMode::Scalar: tuple[0] = converted[0]; break;
    case AttributeDisplayMode::ScalarVector: tuple[1] = converted[0]; break;
    case AttributeDisplayMode::Vector2XZ:
        tuple[0] = converted[0]; tuple[2] = converted[1]; break;
    case AttributeDisplayMode::ScalarColor:
    case AttributeDisplayMode::ScalarLuminanceColor:
        tuple[0] = tuple[1] = tuple[2] = converted[0]; break;
    default:
        for (unsigned int axis = 0U; axis < required; ++axis) tuple[axis] = converted[axis];
        break;
    }
    if (blackTransparent && attributeTupleSize(mode) == 4U) {
        if (mode == AttributeDisplayMode::ScalarColor) tuple[3] = converted[0];
        else {
            if (mode == AttributeDisplayMode::ScalarLuminanceColor)
                original[1] = original[2] = original[0];
            if (!numeric::checkedScalarCast(
                    rgbToLuminance(original[0], original[1], original[2]), tuple[3]))
                throw std::range_error("Viewport attribute alpha is not a finite float.");
        }
    }
    return tuple;
}

template <typename T>
std::vector<float> makeAttributeValues(const std::vector<T>& source,
                                       AttributeDisplayMode mode, bool blackTransparent = false)
{
    const unsigned int dimension = attributeTupleSize(mode);
    std::vector<float> candidate(attributeScalarCount(source.size(), dimension));
    for (std::size_t index = 0U; index < source.size(); ++index) {
        // Copy vector<bool>'s proxy to an actual bool before component access.
        const T value = source[index];
        const auto tuple = attributeTuple(value, mode, blackTransparent);
        std::copy_n(tuple.begin(), dimension, candidate.begin() + index * dimension);
    }
    return candidate;
}

inline void appendAttributeValues(std::vector<float>& destination, const std::vector<float>& source)
{
    std::size_t combined = 0U;
    if (!memory::checkedAdd(destination.size(), source.size(), combined) ||
        combined > kMaxViewportScalarCount)
        throw std::length_error("Viewport attribute append exceeds its budget.");
    if (!geometry::checkedAppendBuffer(destination, source))
        throw std::length_error("Viewport attribute append size is invalid.");
}

// Primitive colors are repeated per point; primitive numeric/vector values
// remain one tuple per primitive, as required by the existing marker shaders.
template <typename T, typename Iterator>
std::vector<float> makePrimitiveAttributeValues(
    const std::vector<T>& source, std::size_t expectedPrimitives,
    Iterator begin, Iterator end, AttributeDisplayMode mode, bool blackTransparent = false)
{
    const unsigned int dimension = attributeTupleSize(mode);
    const bool repeatColor = dimension == 4U;
    if (expectedPrimitives > kMaxViewportVertexCount || source.size() != expectedPrimitives)
        throw std::invalid_argument("Viewport primitive attribute cardinality is invalid.");
    std::size_t count = 0U, primitives = 0U;
    for (auto it = begin; it != end; ++it) {
        if (!*it || ++primitives > expectedPrimitives ||
            !memory::checkedAdd(count, repeatColor ? (*it)->numberOfPoints() : 1U, count))
            throw std::invalid_argument("Viewport primitive geometry is invalid.");
    }
    if (primitives != expectedPrimitives)
        throw std::invalid_argument("Viewport primitive count is inconsistent.");
    std::vector<float> candidate(attributeScalarCount(count, dimension));
    std::size_t offset = 0U, index = 0U;
    for (auto it = begin; it != end; ++it, ++index) {
        const T value = source[index];
        const auto tuple = attributeTuple(value, mode, blackTransparent);
        const std::size_t repetitions = repeatColor ? (*it)->numberOfPoints() : 1U;
        for (std::size_t point = 0U; point < repetitions; ++point) {
            std::copy_n(tuple.begin(), dimension, candidate.begin() + offset);
            offset += dimension;
        }
    }
    return candidate;
}

// Missing point attributes retain the legacy zero/opaque-or-black-alpha
// fallback. Present attributes must have exactly one value per point.
template <typename T, typename Iterator>
std::vector<float> makePointAttributeValues(
    Iterator begin, Iterator end, const std::string& name,
    AttributeDisplayMode mode, bool blackTransparent = false)
{
    const unsigned int dimension = attributeTupleSize(mode);
    std::size_t count = 0U, primitives = 0U;
    for (auto it = begin; it != end; ++it)
        if (!*it || ++primitives > kMaxViewportVertexCount ||
            !memory::checkedAdd(count, (*it)->numberOfPoints(), count))
            throw std::invalid_argument("Viewport point geometry is invalid.");
    std::vector<float> candidate(attributeScalarCount(count, dimension), 0.0f);
    std::size_t offset = 0U;
    for (auto it = begin; it != end; ++it) {
        const std::size_t points = (*it)->numberOfPoints();
        if ((*it)->isExistAttr(name)) {
            const auto& source = (*it)->template attrListValue<T>(name);
            if (source.size() != points)
                throw std::invalid_argument("Viewport point attribute cardinality is invalid.");
            for (std::size_t point = 0U; point < points; ++point) {
                const T value = source[point];
                const auto tuple = attributeTuple(value, mode, blackTransparent);
                std::copy_n(tuple.begin(), dimension, candidate.begin() + offset);
                offset += dimension;
            }
        } else {
            for (std::size_t point = 0U; point < points; ++point) {
                if (dimension == 4U) candidate[offset + 3U] = blackTransparent ? 0.0f : 1.0f;
                offset += dimension;
            }
        }
    }
    return candidate;
}

} // namespace viewport
} // namespace dommy
