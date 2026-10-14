#pragma once

// Maya boundaries only. The complete production compute bodies, numeric
// helpers and actual vendored Perlin implementation execute in the fixture.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DSignalMath.h"
#include "common/utility/DRandomValueGeneration.h"
#include <maya/MTime.h>
#include <perlin.h>
#include <array>
#include <cfenv>
#include <map>
#include <iostream>

#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
using double2 = double[2];
namespace {
int throwAt = -1, published = 0, cleaned = 0;
std::array<double, 3> output = {{91.0, 92.0, 93.0}};
std::map<std::string, std::array<float, 2>> ramps;
MStatus api(const std::string& name) {
    if (callCount == throwAt) throw std::runtime_error("injected boundary exception");
    return operation(name);
}
}
struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0):x(x),y(y),z(z) {}
    double& operator[](unsigned i) { assert(i < 3); return i == 0 ? x : i == 1 ? y : z; }
    double operator[](unsigned i) const { assert(i < 3); return i == 0 ? x : i == 1 ? y : z; }
};
struct MPlug {
    std::string name;
    MPlug(const MObject&, const MObject& attr):name(attr.name) {}
    bool operator!=(const MObject& attr) const { return name != attr.name; }
};
struct SignalInput {
    double number = 0.0;
    double2 range = {0.0, 1.0};
    MVector vector;
};
struct MDataHandle {
    SignalInput* input = nullptr;
    bool valid = true;
    void check() const { assert(valid); }
    double asDouble() const { check(); return input->number; }
    int asInt() const { check(); return static_cast<int>(input->number); }
    short asShort() const { check(); return static_cast<short>(input->number); }
    bool asBool() const { check(); return input->number != 0.0; }
    MTime asTime() const { check(); return MTime(input->number); }
    const double2& asDouble2() const { check(); return input->range; }
    MVector asVector() const { check(); return input->vector; }
    void set(double value) { check(); output[0] = value; ++published; }
    void set(double x, double y, double z) { check(); output = {{x,y,z}}; ++published; }
    void setClean() { check(); ++cleaned; }
};
struct MDataBlock {
    std::map<std::string, SignalInput> inputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = api("input " + attribute.name);
        MDataHandle handle; handle.valid = bool(*status); handle.input = &inputs[attribute.name];
        return handle;
    }
    MDataHandle outputValue(const MObject& attribute, MStatus* status) {
        *status = api("output " + attribute.name);
        MDataHandle handle; handle.valid = bool(*status); return handle;
    }
    MStatus setClean(const MPlug&) {
        const auto status = api("data.clean"); if (status) ++cleaned; return status;
    }
};
struct SignalRamp {
    std::string name;
    explicit SignalRamp(const MPlug& plug):name(plug.name) {}
    void getValueAtPosition(float position, float& value, MStatus* status) {
        *status = api("ramp " + name);
        if (*status) value = ramps.at(name)[position == 1.0f ? 1U : 0U];
    }
};
#define MRampAttribute SignalRamp
