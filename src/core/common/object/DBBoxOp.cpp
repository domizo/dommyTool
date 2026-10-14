//
//  DBBoxOp.cpp
//  dommyTool
//
//  Created by yuzo on 2017/07/06.
//

#include "DBBoxOp.h"

#include "math/DBounds.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointMeanAccumulator.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace {
MBoundingBox pointBoundsBox(const dommy::math::Bounds3d& bounds)
{
    if (bounds.empty()) return MBoundingBox();
    return MBoundingBox(
        MPoint(bounds.minimum(0), bounds.minimum(1), bounds.minimum(2)),
        MPoint(bounds.maximum(0), bounds.maximum(1), bounds.maximum(2)));
}
} // namespace

void CalBBoxFromPointArray( const DPointList &pts, MBoundingBox &bbox, DPoint &center) {
    if (pts.size() > dommy::generation::kMaxGeneratedPointCount)
        throw std::length_error("Point bounds exceed the generation budget");
    dommy::math::Bounds3d bounds;
    std::array<dommy::numeric::detail::PointMeanAccumulator, 3> sums;
    for (DPointList::const_iterator point = pts.begin(); point != pts.end(); ++point) {
        // Preserve this legacy helper's skip-invalid-point policy. Its mean
        // uses exactly the same finite points as its bounds, without copying.
        if (!bounds.add((*point)[0], (*point)[1], (*point)[2])) continue;
        for (std::size_t axis = 0U; axis < 3U; ++axis)
            if (!sums[axis].add(static_cast<double>((*point)[axis])))
                throw std::invalid_argument("Cannot accumulate the point mean");
    }

    if (bounds.empty()) {
        bbox = MBoundingBox();
        center = DPoint(0.0, 0.0, 0.0);
        return;
    }

    std::array<double, 3> mean;
    for (std::size_t axis = 0U; axis < 3U; ++axis)
        if (!sums[axis].mean(static_cast<std::uint32_t>(bounds.count()), mean[axis]))
            throw std::invalid_argument("Cannot calculate the point mean");
    const MBoundingBox candidateBounds = pointBoundsBox(bounds);
    const DPoint candidateCenter(mean[0], mean[1], mean[2]);
    bbox = candidateBounds;
    center = candidateCenter;
}

// BBox-only callers must not pay for the exact arithmetic vertex mean.
void CalBBoxFromPointArray(const DPointList& pts, MBoundingBox& bbox)
{
    if (pts.size() > dommy::generation::kMaxGeneratedPointCount)
        throw std::length_error("Point bounds exceed the generation budget");
    dommy::math::Bounds3d bounds;
    for (const auto& point : pts) bounds.add(point[0], point[1], point[2]);
    bbox = pointBoundsBox(bounds);
}


void CalBBoxFromMPointArray( const MVectorArray &pts, MBoundingBox &bbox) {
    dommy::math::Bounds3d bounds;
    for (size_t index = 0, count = pts.length(); index < count; ++index) {
        bounds.add(pts[index][0], pts[index][1], pts[index][2]);
    }

    bbox = pointBoundsBox(bounds);
}


MStatus DGenerateDisplayBBoxDataFromMinMaxPoint(const MPoint &minPt, const MPoint &maxPt, MPointArray &pts, MIntArray &connectArray)
{
    if (!dommy::maya::isFloatRepresentablePoint(minPt) ||
        !dommy::maya::isFloatRepresentablePoint(maxPt) ||
        minPt.x > maxPt.x || minPt.y > maxPt.y || minPt.z > maxPt.z)
        return MS::kInvalidParameter;
    MPointArray points;
    MIntArray connections;
    MStatus status = points.setLength(8U);
    if (!status) return status;
    status = connections.setLength(24U);
    if (!status) return status;
    if (points.length() != 8U || connections.length() != 24U) return MS::kFailure;
	
    points[0] = MPoint(minPt.x,maxPt.y,minPt.z);
    points[1] = MPoint(minPt.x,maxPt.y,maxPt.z);
    points[2] = MPoint(maxPt.x,maxPt.y,maxPt.z);
    points[3] = MPoint(maxPt.x,maxPt.y,minPt.z);
    points[4] = MPoint(minPt.x,minPt.y,minPt.z);
    points[5] = MPoint(minPt.x,minPt.y,maxPt.z);
    points[6] = MPoint(maxPt.x,minPt.y,maxPt.z);
    points[7] = MPoint(maxPt.x,minPt.y,minPt.z);
    const int edges[24] = {0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4, 0,4, 1,5, 2,6, 3,7};
    for (unsigned int index = 0U; index < 24U; ++index) connections[index] = edges[index];
    status = pts.copy(points);
    if (!status) return status;
    return connectArray.copy(connections);
}
