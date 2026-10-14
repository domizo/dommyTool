#pragma once

#include "DRandomValueGeneration.h"
#include "DMayaCheckedArray.h"

namespace dommy {
namespace maya {

// Numeric generation and Maya allocation both finish before publication.
// A Maya copy failure is reported; the API cannot promise rollback if copy
// itself modifies the destination before failing.
template <typename Scalar, typename Array>
MStatus generateRandomArray(int seed, Scalar minimum, Scalar maximum,
                            std::size_t count, Array& output,
                            bool includeMaximum = true)
{
    try {
        std::vector<Scalar> values;
        random::generateValues(static_cast<std::uint32_t>(seed),
                               minimum, maximum, count, values, includeMaximum);
        return buildCheckedArray(count,
            [&](unsigned int i, Scalar& value) -> MStatus {
                value = values[i];
                return MS::kSuccess;
            }, output);
    } catch (const std::invalid_argument&) {
        return MS::kInvalidParameter;
    } catch (const std::length_error&) {
        return MS::kInvalidParameter;
    }
}

template <typename Array>
inline bool generateRandomDoubleArray(int seed, double minimum, double maximum,
                                      std::size_t count, Array& values)
{
    return generateRandomArray(seed, minimum, maximum, count, values) == MS::kSuccess;
}

} // namespace maya
} // namespace dommy
