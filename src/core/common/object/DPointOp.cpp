//
// Point-generation and XY convex-hull adapters for Maya.
//

#include "DPointOp.h"
#include "DPointGeneration.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DConvexHull2D.h"

#define JC_VORONOI_IMPLEMENTATION
#include "common/utility/DRelaxPoint2D.h"

MStatus DGenerateRelaxPoint2D(std::size_t pointCount, std::size_t maxIteration,
    int seed, float minX, float maxX, float minY, float maxY, MPointArray& newPoints)
{
    std::vector<std::array<double, 3>> points;
    if (!dommy::geometry::generateRelaxedPoints2D(pointCount, maxIteration, seed,
                                                 minX, maxX, minY, maxY, points))
        return MS::kInvalidParameter;
    return dommy::maya::buildCheckedArray(points.size(),
        [&](unsigned int i, MPoint& point) -> MStatus {
            point = MPoint(points[i][0], points[i][1], points[i][2]);
            return MS::kSuccess;
        }, newPoints);
}

DPointList CreateConvex2DList(const DPointList& points)
{
    DPointList result;
    if (!dommy::geometry::buildConvexHull2D(points, result))
        throw std::invalid_argument("Convex hull input or work size is invalid");
    return result;
}
