#pragma once

#include "curve_path_doubles.h"
#include <random>

using MObjectArray = Array<MObject>;
using MFloatArray = Array<float>;
using MFloatVector = MVector;
using DPoint = MPoint;
using DPointList = MPointArray;
using ConstDCItem = DCItem;
inline MVector operator/(const MVector& v, double n) {
    return v * (1.0 / n);
}
inline MPoint& operator+=(MPoint& p, const MVector& v) {
    p = p + v;
    return p;
}
#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus s = (x);                                                                     \
        if (!s)                                                                                    \
            s.pAPIerror("emitter", __LINE__);                                                      \
    } while (false)

struct EmitterBlock : MDataBlock {
    MDataHandle inputValue(MObject attr, MStatus* status = nullptr) {
        MStatus local;
        return MDataBlock::inputValue(attr, status ? status : &local);
    }
};
// World queries are recorded but not Maya geometry emulation. The fixture's
// standalone curve data is sampled with the same coordinates in either space.
struct EmitterCurve : MFnNurbsCurve {
    MStatus getCVs(MPointArray& p, MSpace::Space space) const {
        assert(space == MSpace::kWorld);
        return MFnNurbsCurve::getCVs(p, MSpace::kObject);
    }
    MStatus getPointAtParam(double t, MPoint& p, MSpace::Space space) const {
        assert(space == MSpace::kWorld);
        return MFnNurbsCurve::getPointAtParam(t, p, MSpace::kObject);
    }
    MVector tangent(double t, MSpace::Space space, MStatus* s) const {
        assert(space == MSpace::kWorld);
        return MFnNurbsCurve::tangent(t, MSpace::kObject, s);
    }
    MVector normal(double t, MSpace::Space space, MStatus* s) const {
        assert(space == MSpace::kWorld);
        return MFnNurbsCurve::normal(t, MSpace::kObject, s);
    }
    MPoint closestPoint(const MPoint& p, double* t, double, MSpace::Space space, MStatus* s) const {
        assert(space == MSpace::kWorld);
        *s = api("curve.closest");
        if (t && *s)
            *t = p.x;
        return p + MVector(0, 1, 0);
    }
};
namespace {
bool badRadiusRamp = false;
double noiseValue = .5;
} // namespace
struct EmitterRamp {
    explicit EmitterRamp(const MPlug&, MStatus* s = nullptr) {
        const auto status = api("ramp.bind");
        if (s)
            *s = status;
    }
    void sampleValueRamp(unsigned n, MFloatArray& values, MStatus* s) {
        const int ordinal = callCount;
        *s = api("ramp.sample");
        if (*s) {
            values.assign(n, badRadiusRamp ? NAN : 1.0f);
            if (ordinal == shortAt && !values.empty()) {
                values.pop_back();
                shortApplied = true;
            }
        }
    }
};
struct EmitterDependency {
    EmitterDependency(MObject, MStatus* s) {
        *s = api("node.bind");
    }
    MPlug findPlug(MObject a, bool, MStatus* s) {
        *s = api("node.findPlug");
        return MPlug({}, a);
    }
};
namespace noise {
enum { QUALITY_BEST };
namespace module {
struct Perlin {
    void SetSeed(long) {}
    void SetOctaveCount(int) {}
    void SetFrequency(float) {}
    void SetLacunarity(float) {}
    void SetPersistence(float) {}
    void SetNoiseQuality(int) {}
    double GetValue(double, double, double) const {
        return noiseValue;
    }
};
} // namespace module
} // namespace noise
