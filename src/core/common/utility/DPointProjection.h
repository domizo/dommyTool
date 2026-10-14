#pragma once

#include "DGenerationLimits.h"
#include "DNumericValidation.h"

#include <cstddef>
#include <vector>

namespace dommy {
namespace geometry {

// Prepare every projection input from its current position, including points
// that relaxation skipped. Validate the complete result before committing it.
// ProjectionPoint needs three writable indexed coordinates; Project returns
// bool and may change coordinates, but must preserve the point count.
template <typename ProjectionPoint, typename PointList, typename Project>
bool projectPointPositions(PointList& positions, Project project)
{
    if (positions.size() > generation::kMaxGeneratedPointCount) return false;
    if (positions.empty()) return true;

    std::vector<ProjectionPoint> projected(positions.size());
    for (std::size_t index = 0U; index < positions.size(); ++index) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!numeric::checkedNumericCast(
                    positions[index][axis], projected[index][axis])) return false;
        }
    }
    if (!project(projected) || projected.size() != positions.size()) return false;

    PointList candidate(positions);
    for (std::size_t index = 0U; index < positions.size(); ++index) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!numeric::checkedNumericCast(
                    projected[index][axis], candidate[index][axis])) return false;
        }
    }
    positions.swap(candidate);
    return true;
}

} // namespace geometry
} // namespace dommy
