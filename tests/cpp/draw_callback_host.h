#pragma once

// Recording doubles for callback control flow, not Maya/GPU lifetime evidence.
#include <cassert>
#include <cmath>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>
#include <maya/MStatus.h>

using uint = unsigned int;
std::vector<std::string> operations;
int throwAt = -1, failAt = -1, exceptionKind = 0, reportedExceptions = 0;
int liveUserData = 0, beginAttempts = 0, endAttempts = 0;
bool cameraReadSucceeds = true;

void api(const char* operation)
{
    operations.push_back(operation);
    if (static_cast<int>(operations.size()) - 1 == throwAt) {
        if (exceptionKind == 1) throw std::bad_alloc();
        throw std::runtime_error("injected callback failure");
    }
}

MStatus apiStatus(const char* operation)
{
    api(operation);
    return static_cast<int>(operations.size()) - 1 == failAt
        ? MS::kFailure : MS::kSuccess;
}

void resetOperations()
{
    operations.clear();
    throwAt = -1;
    failAt = -1;
    exceptionKind = reportedExceptions = beginAttempts = endAttempts = 0;
    cameraReadSucceeds = true;
}

struct MUserData {
    explicit MUserData(bool) { api("data.construct"); ++liveUserData; }
    virtual ~MUserData() { --liveUserData; }
    MUserData(const MUserData&) = delete;
    MUserData& operator=(const MUserData&) = delete;
};

struct MObject {};
struct MDagPath {
    MObject node() const { api("path.node"); return {}; }
};
struct MGlobal {
    static void displayWarning(const char*) { api("warning"); }
};

struct MMatrix {
    double elements[4][4]{};
    MMatrix() { for (unsigned int i = 0; i < 4; ++i) elements[i][i] = 1; }
    const double* operator[](unsigned int i) const { return elements[i]; }
    double* operator[](unsigned int i) { return elements[i]; }
};

struct MVector {
    enum Axis { kYaxis, kZaxis };
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    MVector rotateBy(Axis axis, double radians) const {
        const double c = std::cos(radians), s = std::sin(radians);
        return axis == kYaxis ? MVector(x*c+z*s, y, -x*s+z*c)
                             : MVector(x*c-y*s, x*s+y*c, z);
    }
};

struct MPoint {
    double x, y, z, w = 1;
    MPoint(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    MPoint(const MVector& vector) : x(vector.x), y(vector.y), z(vector.z) {}
};

MPoint operator*(const MPoint& point, const MMatrix& matrix)
{
    return MPoint(point.x*matrix[0][0]+point.y*matrix[1][0]+point.z*matrix[2][0]+matrix[3][0],
                  point.x*matrix[0][1]+point.y*matrix[1][1]+point.z*matrix[2][1]+matrix[3][1],
                  point.x*matrix[0][2]+point.y*matrix[1][2]+point.z*matrix[2][2]+matrix[3][2]);
}

template <typename T> struct Array {
    std::vector<T> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    const T& operator[](unsigned int index) const {
        assert(index < values.size());
        api("array.read");
        return values[index];
    }
    T& operator[](unsigned int index) {
        assert(index < values.size());
        api("array.read");
        return values[index];
    }
    MStatus append(const T& value) {
        const MStatus status = apiStatus("array.append");
        if (status) values.push_back(value);
        return status;
    }
    MStatus setLength(unsigned int count) {
        const MStatus status = apiStatus("array.resize");
        if (status) values.resize(count);
        return status;
    }
    MStatus copy(const Array& source) {
        const MStatus status = apiStatus("array.copy");
        // A failed API status need not imply an unchanged destination. Test
        // admission independently of array length/content after failure.
        values = source.values;
        return status;
    }
    void clear() { values.clear(); }
};

using MMatrixArray = Array<MMatrix>;
using MDoubleArray = Array<double>;
using MVectorArray = Array<MVector>;
using MPointArray = Array<MPoint>;

struct MColor {
    float r, g, b, a;
    MColor(float red, float green, float blue, float alpha = 1)
        : r(red), g(green), b(blue), a(alpha) {}
};

std::vector<MColor> submittedColors;
std::vector<MPoint> submittedPoints;
int submittedLines = 0;

namespace MHWRender {
struct MFrameContext {};
struct MUIDrawManager {
    static void validPoint(const MPoint& point) {
        const double limit = std::numeric_limits<float>::max();
        assert(std::isfinite(point.x) && std::fabs(point.x) <= limit &&
               std::isfinite(point.y) && std::fabs(point.y) <= limit &&
               std::isfinite(point.z) && std::fabs(point.z) <= limit);
    }
    void beginDrawable() { api("draw.begin"); ++beginAttempts; }
    void endDrawable() { ++endAttempts; api("draw.end"); }
    void setLineWidth(float) { api("draw.lineWidth"); }
    void setDepthPriority(unsigned int) { api("draw.priority"); }
    void setPointSize(float size) {
        api("draw.pointSize"); assert(std::isfinite(size) && size > 0);
    }
    void setColor(const MColor& color) {
        api("draw.color");
        assert(std::isfinite(color.r) && std::isfinite(color.g) &&
               std::isfinite(color.b) && std::isfinite(color.a));
        submittedColors.push_back(color);
    }
    void point(const MPoint& point) {
        api("draw.point"); validPoint(point); submittedPoints.push_back(point);
    }
    void lineList(const MPointArray& points, bool) {
        api("draw.lineList"); for (const auto& point : points.values) validPoint(point);
        ++submittedLines;
    }
    void lineStrip(const MPointArray& points, bool) {
        api("draw.lineStrip"); for (const auto& point : points.values) validPoint(point);
        ++submittedLines;
    }
};
}

namespace dommy {
namespace math {
double degreesToRadians(double degrees) { return degrees*3.14159265358979323846/180.0; }
}
namespace maya {
void reportViewportCallbackException(const char*, const char*, const std::exception_ptr&) noexcept {
    ++reportedExceptions;
}
}
}

template <typename Frustum>
MStatus populateCameraFrustumData(const MObject&, Frustum&)
{
    api("camera.populate");
    return cameraReadSucceeds ? MS::kSuccess : MS::kFailure;
}
