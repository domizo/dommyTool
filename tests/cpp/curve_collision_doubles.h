#pragma once

// Host boundaries only. The straight-line/slab fixtures do not emulate Maya
// NURBS, mesh topology, quaternion rotation or ramp interpolation.
#include "curve_utility_doubles.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMeshIntersection.h"
#include "common/utility/DCurveReflection.h"

using DVec3DataList = std::vector<MVector>;
using ConstDCItem = DCItem;
namespace dommy {
namespace maya {
inline bool isFiniteQuaternion(const MQuaternion& q) {
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}
inline bool isFloatRepresentableVector(const MVector& v) {
    return isFiniteVector(v) && numeric::canRepresentAsFloat(v.x) &&
           numeric::canRepresentAsFloat(v.y) && numeric::canRepresentAsFloat(v.z);
}
} // namespace maya
namespace attribute {
inline bool convertVector3Attribute(const std::vector<MVector>& source, std::size_t n,
                                    std::vector<Imath::V3d>& out) {
    if (!api("attribute.vectorRead") || source.size() != n)
        return false;
    std::vector<Imath::V3d> candidate;
    candidate.reserve(source.size());
    for (const auto& value : source)
        candidate.push_back(Imath::V3d(value[0], value[1], value[2]));
    out.swap(candidate);
    return true;
}
} // namespace attribute
} // namespace dommy
struct TestCurveRamp {
    TestCurveRamp() = default;
    TestCurveRamp(const MPlug&, MStatus* s = nullptr) {
        const auto status = api("ramp.bind");
        if (s)
            *s = status;
    }
};
namespace dommy {
namespace maya {
inline bool sampleFiniteRamp(TestCurveRamp&, double position, float& result) {
    if (!api("ramp.sample") || !std::isfinite(position) || position < 0 || position > 1)
        return false;
    result = 0;
    return true;
}
} // namespace maya
} // namespace dommy
inline MStatus DCPAddAttrVec3(DCItem& curve, bool, bool, const std::string& name, const char*,
                              const DVec3DataList& values) {
    const auto s = api("attribute.vectorWrite");
    if (s)
        curve->vectorAttributes[name] = values;
    return s;
}
void DGenerateRandomValue3(int seed, const double3 low, const double3 high, size_t count,
                           DVec3DataList& values) {
    dommy::random::generateRealTriples<float>(seed, low, high, count, values);
}
