#pragma once

#include "DGenerationLimits.h"
#include "geometry/DGeometryTypes.h"

namespace dommy {
namespace geometry {

// XY projected hull, retaining the selected input points' Z values. Convert
// components before arithmetic; a double return type alone does not widen float
// multiplication. Duplicate XY points retain the first input's Z deterministically.
template <typename Points>
bool buildConvexHull2D(const Points& points, Points& output)
{
    std::size_t bytes = 0U;
    if (points.size() > generation::kMaxGeneratedPointCount ||
        !generation::checkedProduct(points.size(), 3U * sizeof(typename Points::value_type),
                                    64U * 1024U * 1024U, bytes)) return false;
    for (const auto& point : points)
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            if (!std::isfinite(point[axis])) return false;
    Points sorted(points);
    std::stable_sort(sorted.begin(), sorted.end(), [](const typename Points::value_type& a,
                                                     const typename Points::value_type& b) {
        return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]);
    });
    sorted.erase(std::unique(sorted.begin(), sorted.end(), [](const typename Points::value_type& a,
                                                             const typename Points::value_type& b) {
        return a[0] == b[0] && a[1] == b[1];
    }), sorted.end());
    if (sorted.size() < 2U) { output.swap(sorted); return true; }
    Points hull(2U * sorted.size());
    std::size_t k = 0U;
    const auto turn = [](const typename Points::value_type& o, const typename Points::value_type& a,
                         const typename Points::value_type& b) {
        return cross2(Point2(o[0], o[1]), Point2(a[0], a[1]), Point2(b[0], b[1]));
    };
    for (const auto& point : sorted) {
        while (k >= 2U) {
            const double orientation = turn(hull[k-2U], hull[k-1U], point);
            if (!std::isfinite(orientation)) return false;
            if (orientation > 0.0) break;
            --k;
        }
        hull[k++] = point;
    }
    const std::size_t threshold = k + 1U;
    for (std::size_t i = sorted.size() - 1U; i-- > 0U;) {
        while (k >= threshold) {
            const double orientation = turn(hull[k-2U], hull[k-1U], sorted[i]);
            if (!std::isfinite(orientation)) return false;
            if (orientation > 0.0) break;
            --k;
        }
        hull[k++] = sorted[i];
    }
    hull.resize(k - 1U);
    output.swap(hull);
    return true;
}

} // namespace geometry
} // namespace dommy
