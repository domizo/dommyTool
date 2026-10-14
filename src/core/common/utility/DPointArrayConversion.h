#pragma once

#include "DMayaCheckedArray.h"
#include "DNumericValidation.h"

#include <stdexcept>
#include <type_traits>
#include <vector>

namespace dommy {
namespace geometry {

// Both DPData and DPrimBaseData store Cartesian XYZ, not homogeneous points.
// Keep that legacy interpretation: do not divide incoming XYZ by Maya's w.
template <typename Source, typename Point>
void replacePointArray(const Source& source, std::size_t count,
                       std::vector<Point>& output)
{
    if (count > generation::kMaxGeneratedPointCount ||
        !numeric::checkedVectorArrayCopy(source, count, count, output))
        throw std::invalid_argument("Point array exceeds count or numeric storage limits");
}

template <typename Point, typename Array>
MStatus copyPointsToMayaArray(const std::vector<Point>& source, Array& output)
{
    using MayaPoint = typename std::remove_reference<decltype(output[0U])>::type;
    return maya::buildCheckedArray(source.size(),
        [&](unsigned int i, MayaPoint& point) -> MStatus {
            if (!numeric::checkedNumericCast(source[i][0], point.x) ||
                !numeric::checkedNumericCast(source[i][1], point.y) ||
                !numeric::checkedNumericCast(source[i][2], point.z))
                return MS::kInvalidParameter;
            point.w = 1;
            return MS::kSuccess;
        }, output);
}

} // namespace geometry
} // namespace dommy
