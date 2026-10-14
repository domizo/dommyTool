#pragma once

class MVector
{
public:
    double x, y, z;

    MVector(double xValue = 0.0, double yValue = 0.0, double zValue = 0.0)
        : x(xValue), y(yValue), z(zValue) {}

    double operator[](unsigned int axis) const { return axis == 0 ? x : axis == 1 ? y : z; }

    MVector operator+(const MVector& rhs) const { return MVector(x+rhs.x, y+rhs.y, z+rhs.z); }
    MVector operator-(const MVector& rhs) const { return MVector(x-rhs.x, y-rhs.y, z-rhs.z); }
    MVector operator*(double scalar) const { return MVector(x*scalar, y*scalar, z*scalar); }
};
