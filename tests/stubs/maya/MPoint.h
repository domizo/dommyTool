#pragma once

#include "MVector.h"

class MPoint
{
public:
    double x, y, z, w;

    MPoint(double xValue = 0.0, double yValue = 0.0, double zValue = 0.0)
        : x(xValue), y(yValue), z(zValue), w(1.0) {}

    operator MVector() const { return MVector(x, y, z); }
};
