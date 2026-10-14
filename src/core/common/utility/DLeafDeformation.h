#pragma once

#include <cmath>

namespace dommy {
namespace generation {

// Rotation about X leaves X unchanged, including the historical pivot's X
// coordinate. Store the remaining affine transform in the Y/Z plane.
struct LeafBendTransform {
    LeafBendTransform()
        : yy(1.0), yz(0.0), zy(0.0), zz(1.0), offsetY(0.0), offsetZ(0.0) {}

    double yy, yz, zy, zz;
    double offsetY, offsetZ;
};

inline bool finiteLeafBendTransform(const LeafBendTransform& transform)
{
    return std::isfinite(transform.yy) && std::isfinite(transform.yz) &&
           std::isfinite(transform.zy) && std::isfinite(transform.zz) &&
           std::isfinite(transform.offsetY) && std::isfinite(transform.offsetZ);
}

// Prepend this row's bend to the accumulated later bends. A point generated
// at row r historically underwent r, r+1, ..., last-1 in that order.
inline bool prependLeafBendStep(double radians,
                                double pivotY,
                                LeafBendTransform& later)
{
    if (!std::isfinite(radians) || !std::isfinite(pivotY) ||
        !finiteLeafBendTransform(later)) return false;

    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const double offsetY = (1.0 - c) * pivotY;
    const double offsetZ = -s * pivotY;
    LeafBendTransform result;
    result.yy = later.yy * c + later.yz * s;
    result.yz = -later.yy * s + later.yz * c;
    result.zy = later.zy * c + later.zz * s;
    result.zz = -later.zy * s + later.zz * c;
    result.offsetY = later.yy * offsetY + later.yz * offsetZ + later.offsetY;
    result.offsetZ = later.zy * offsetY + later.zz * offsetZ + later.offsetZ;
    if (!finiteLeafBendTransform(result)) return false;
    later = result;
    return true;
}

inline bool applyLeafBend(const LeafBendTransform& transform,
                          double y, double z,
                          double& outputY, double& outputZ)
{
    if (!std::isfinite(y) || !std::isfinite(z) ||
        !finiteLeafBendTransform(transform)) return false;
    const double resultY = transform.yy * y + transform.yz * z + transform.offsetY;
    const double resultZ = transform.zy * y + transform.zz * z + transform.offsetZ;
    if (!std::isfinite(resultY) || !std::isfinite(resultZ)) return false;
    outputY = resultY;
    outputZ = resultZ;
    return true;
}

} // namespace generation
} // namespace dommy
