#include "dommyMath.h"
#include "DPoint/math/DNumeric.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace dommy {
namespace math {

namespace {

const double kDegreesToRadians = 0.0174532925199432957692369076848;
const double kRadiansToDegrees = 57.295779513082320876798154814114;

bool hasFiniteCoordinates(const MPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
}

bool hasFiniteCoordinates(const MVector& vector)
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) &&
           std::isfinite(vector.z);
}

double stableDistance(double x, double y, double z)
{
    const double scale =
        std::max(std::fabs(x), std::max(std::fabs(y), std::fabs(z)));
    if (!std::isfinite(scale)) {
        return std::numeric_limits<double>::infinity();
    }
    if (scale == 0.0) return 0.0;

    x /= scale;
    y /= scale;
    z /= scale;
    return scale * std::sqrt(x * x + y * y + z * z);
}

} // namespace

double degreesToRadians(double degrees)
{
    return degrees * kDegreesToRadians;
}

double radiansToDegrees(double radians)
{
    return radians * kRadiansToDegrees;
}

void aimY(const MVector& sourceVector, double& xRotation, double& zRotation)
{
    const double xyLength =
        stableDistance(sourceVector.x, sourceVector.y, 0.0);
    const double vectorLength =
        stableDistance(sourceVector.x, sourceVector.y, sourceVector.z);

    if (!std::isfinite(vectorLength) || vectorLength == 0.0) {
        xRotation = 0.0;
        zRotation = 0.0;
        return;
    }

    double zAngle = xyLength == 0.0
        ? 0.0
        : acosClamped(sourceVector.y / xyLength);
    const double xAngle = acosClamped(xyLength / vectorLength);

    xRotation = sourceVector.z > 0.0 ? xAngle : -xAngle;
    zRotation = sourceVector.x > 0.0 ? -zAngle : zAngle;
}

double distance(const MVector& first, const MVector& second)
{
    return stableDistance(
        second.x - first.x, second.y - first.y, second.z - first.z);
}

double distance(const MPoint& first, const MPoint& second)
{
    return stableDistance(
        second.x - first.x, second.y - first.y, second.z - first.z);
}

bool intersectSegmentAabb(const MPoint& firstBound,
                          const MPoint& secondBound,
                          const MVector& segmentStart,
                          const MVector& segmentEnd,
                          MVector& intersection)
{
    if (!hasFiniteCoordinates(firstBound) ||
        !hasFiniteCoordinates(secondBound) ||
        !hasFiniteCoordinates(segmentStart) ||
        !hasFiniteCoordinates(segmentEnd)) {
        return false;
    }

    const double minimum[3] = {
        std::min(firstBound.x, secondBound.x),
        std::min(firstBound.y, secondBound.y),
        std::min(firstBound.z, secondBound.z),
    };
    const double maximum[3] = {
        std::max(firstBound.x, secondBound.x),
        std::max(firstBound.y, secondBound.y),
        std::max(firstBound.z, secondBound.z),
    };
    const double start[3] = {
        segmentStart.x, segmentStart.y, segmentStart.z,
    };
    const double direction[3] = {
        segmentEnd.x - segmentStart.x,
        segmentEnd.y - segmentStart.y,
        segmentEnd.z - segmentStart.z,
    };

    double firstTime = 0.0;
    double lastTime = 1.0;
    for (unsigned int axis = 0; axis < 3U; ++axis) {
        if (direction[axis] == 0.0) {
            if (start[axis] < minimum[axis] ||
                start[axis] > maximum[axis]) {
                return false;
            }
            continue;
        }

        double enterTime =
            (minimum[axis] - start[axis]) / direction[axis];
        double exitTime =
            (maximum[axis] - start[axis]) / direction[axis];
        if (enterTime > exitTime) std::swap(enterTime, exitTime);

        firstTime = std::max(firstTime, enterTime);
        lastTime = std::min(lastTime, exitTime);
        if (firstTime > lastTime) return false;
    }

    const MVector candidate(
        start[0] + direction[0] * firstTime,
        start[1] + direction[1] * firstTime,
        start[2] + direction[2] * firstTime);
    if (!hasFiniteCoordinates(candidate)) return false;

    intersection = candidate;
    return true;
}

} // namespace math
} // namespace dommy
