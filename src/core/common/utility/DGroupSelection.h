#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace dommy {
namespace group {

inline std::size_t clampedSelectionIndex(int value, std::size_t limit)
{
    if (value <= 0) return 0U;

    const unsigned long long unsignedValue =
        static_cast<unsigned long long>(value);
    return unsignedValue >= static_cast<unsigned long long>(limit)
               ? limit
               : static_cast<std::size_t>(unsignedValue);
}

inline void selectPeriodicRange(
    int start,
    int end,
    int selectedPerPeriod,
    int period,
    std::size_t componentCount,
    std::vector<bool>& selection)
{
    const std::size_t limit = std::min(componentCount, selection.size());
    if (limit == 0U || selectedPerPeriod <= 0 || period <= 0) return;

    std::size_t first = clampedSelectionIndex(start, limit);
    std::size_t last = clampedSelectionIndex(end, limit);
    if (first > last) std::swap(first, last);

    const std::size_t periodSize = static_cast<std::size_t>(period);
    const std::size_t selectedCount = std::min(
        static_cast<std::size_t>(selectedPerPeriod), periodSize);

    for (std::size_t index = first; index < last; ++index) {
        if (((index - first) % periodSize) < selectedCount) {
            selection[index] = true;
        }
    }
}

inline bool isInsideOpenBox(
    double pointX,
    double pointY,
    double pointZ,
    double centerX,
    double centerY,
    double centerZ,
    double sizeX,
    double sizeY,
    double sizeZ)
{
    const double values[] = {
        pointX, pointY, pointZ,
        centerX, centerY, centerZ,
        sizeX, sizeY, sizeZ};
    for (double value : values) {
        if (!std::isfinite(value)) return false;
    }

    const double halfX = std::fabs(sizeX) * 0.5;
    const double halfY = std::fabs(sizeY) * 0.5;
    const double halfZ = std::fabs(sizeZ) * 0.5;
    const double minimumX = centerX - halfX;
    const double maximumX = centerX + halfX;
    const double minimumY = centerY - halfY;
    const double maximumY = centerY + halfY;
    const double minimumZ = centerZ - halfZ;
    const double maximumZ = centerZ + halfZ;
    if (!std::isfinite(minimumX) || !std::isfinite(maximumX) ||
        !std::isfinite(minimumY) || !std::isfinite(maximumY) ||
        !std::isfinite(minimumZ) || !std::isfinite(maximumZ)) {
        return false;
    }
    return pointX > minimumX && pointX < maximumX &&
           pointY > minimumY && pointY < maximumY &&
           pointZ > minimumZ && pointZ < maximumZ;
}

inline bool isInsideEllipsoid(
    double pointX,
    double pointY,
    double pointZ,
    double centerX,
    double centerY,
    double centerZ,
    double radiusX,
    double radiusY,
    double radiusZ)
{
    const double values[] = {
        pointX, pointY, pointZ,
        centerX, centerY, centerZ,
        radiusX, radiusY, radiusZ};
    for (double value : values) {
        if (!std::isfinite(value)) return false;
    }

    const double minimumRadius =
        std::sqrt(std::numeric_limits<double>::min());
    radiusX = std::fabs(radiusX);
    radiusY = std::fabs(radiusY);
    radiusZ = std::fabs(radiusZ);
    if (radiusX <= minimumRadius ||
        radiusY <= minimumRadius ||
        radiusZ <= minimumRadius) {
        return false;
    }

    const double normalizedX = (pointX - centerX) / radiusX;
    const double normalizedY = (pointY - centerY) / radiusY;
    const double normalizedZ = (pointZ - centerZ) / radiusZ;
    const double squaredDistance =
        normalizedX * normalizedX +
        normalizedY * normalizedY +
        normalizedZ * normalizedZ;
    return std::isfinite(squaredDistance) && squaredDistance <= 1.0;
}

}  // namespace group
}  // namespace dommy
