#pragma once

#include "DMayaCheckedArray.h"

namespace dommy {
namespace maya {

template <typename Array>
MStatus buildCurveKnots(std::size_t cvCount, int degree, bool loop, Array& knots)
{
    std::size_t knotCount = 0U;
    // Preserve the legacy empty result for an invalid curve layout.
    if (degree < 1 || cvCount <= static_cast<std::size_t>(degree) ||
        !memory::checkedAdd(cvCount, static_cast<std::size_t>(degree - 1), knotCount) ||
        knotCount > std::numeric_limits<unsigned int>::max()) {
        return buildCheckedArray(0U,
            [](unsigned int, double&) -> MStatus { return MS::kSuccess; }, knots);
    }
    const std::size_t degreeSize = static_cast<std::size_t>(degree);
    const std::size_t finalKnotValue = cvCount - degreeSize;
    return buildCheckedArray(knotCount,
        [&](unsigned int index, double& value) -> MStatus {
            if (loop) value = static_cast<double>(index);
            else if (index >= knotCount - degreeSize) value = static_cast<double>(finalKnotValue);
            else if (index >= degreeSize) value = static_cast<double>(index - degreeSize + 1U);
            else value = 0.0;
            return MS::kSuccess;
        }, knots);
}

} // namespace maya
} // namespace dommy
