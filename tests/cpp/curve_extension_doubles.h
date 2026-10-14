#pragma once
#include "curve_copy_doubles.h"
// Only host/storage boundaries are doubled. Extension planning and direction
// math come from production; knot/attribute editing is not a Maya emulator.
enum { DC_VEC_kTangent = 0, DC_VEC_kNormal = 1 };
enum { DC_kLinear = 0, DC_kCubic = 1 };
enum { DC_kClose = 0, DC_kOpen = 1 };
using DPoint = MPoint;
inline MPoint DPToMP(const DPoint& point) {
    return point;
}
inline DPoint MVToDP(const MPoint& point) {
    return point;
}
namespace dommy {
namespace maya {
inline bool isFloatRepresentablePoint(const MPoint& p) {
    return isFinitePoint(p) && numeric::canRepresentAsFloat(p.x) &&
           numeric::canRepresentAsFloat(p.y) && numeric::canRepresentAsFloat(p.z);
}
} // namespace maya
} // namespace dommy
