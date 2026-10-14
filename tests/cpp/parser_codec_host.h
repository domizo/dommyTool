#pragma once

// The position container mirrors the float-backed XYZ storage only. The SDK
// mode uses actual MPoint/MStatus but does not instantiate a Maya MPxData object.
#include "common/file/DBoundedTextRead.h"
#include "common/file/DNumericTextRead.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DIndexPattern.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include <maya/MStatus.h>
#ifdef DOMMY_SDK_CHECK
#include <maya/MPoint.h>
#else
struct MPoint {
    double x, y, z;
    MPoint(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    double& operator[](unsigned int axis) { return axis == 0U ? x : axis == 1U ? y : z; }
    const double& operator[](unsigned int axis) const { return axis == 0U ? x : axis == 1U ? y : z; }
};
#endif

using std::ostream;
using DPoint = std::array<float, 3>;
using DPointList = std::vector<DPoint>;
struct DPData {
    DPointList pointArray;
    std::vector<int> mAttributeList, mGrooupList;
    MStatus writeASCII(ostream& out);
    MStatus writePointArrayASCII(ostream& out);
};
