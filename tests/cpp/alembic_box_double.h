#pragma once
#ifdef DOMMY_SDK_CHECK
#include <maya/MBoundingBox.h>
#include <maya/MPoint.h>
#else
#include <maya/MPoint.h>
#include <algorithm>
// Box arithmetic is a test double, not Maya's implementation.
struct MBoundingBox {
    MPoint low, high;
    bool empty = true;
    MBoundingBox() = default;
    MBoundingBox(MPoint a, MPoint b) : low(a), high(b), empty(false) {}
    MPoint min() const { return low; }
    MPoint max() const { return high; }
    void expand(const MPoint& p) {
        if (empty) { low = high = p; empty = false; return; }
        low.x = std::min(low.x, p.x); low.y = std::min(low.y, p.y); low.z = std::min(low.z, p.z);
        high.x = std::max(high.x, p.x); high.y = std::max(high.y, p.y); high.z = std::max(high.z, p.z);
    }
};
#endif
