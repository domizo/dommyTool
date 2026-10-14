#pragma once

// Reuse the existing detached curve-data and failure-injectable Maya boundary.
// Numeric/ramp admission and the complete production compute body are real.
#include "curve_copy_doubles.h"
#include "common/utility/DSignalMath.h"
#include <cfenv>

using DScalarData = float;
namespace {
float rampStart = 1.0f, rampEnd = 0.0f;
bool invalidRamp = false;
std::vector<float> sampledPositions;
}
struct TestWeightMapRamp {
    TestWeightMapRamp(const MPlug&, MStatus* status) { *status = api("ramp.bind"); }
    void getValueAtPosition(float position, float& value, MStatus* status) {
        assert(position >= 0.0f && position <= 1.0f);
        *status = api("ramp.sample");
        if (*status) {
            sampledPositions.push_back(position);
            if (invalidRamp) value = std::numeric_limits<float>::quiet_NaN();
            else assert(dommy::math::checkedLinearInterpolation(rampStart, rampEnd, position, value));
        }
    }
};
namespace dommy {
namespace attribute {
inline bool convertScalarAttribute(const std::vector<float>& source, std::size_t count,
                                   std::vector<double>& result) {
    if (!api("attribute.read")) throw std::runtime_error("attribute read failed");
    return numeric::checkedScalarArrayCopy(source, source.size(), count, result);
}
} // namespace attribute
} // namespace dommy
inline MStatus DCPAddAttrScalar(const DCItem& curve, bool replace, bool add,
                               const std::string& name, const std::string& group,
                               const DScalarDataList& values) {
    assert(replace && !add && group.empty());
    const auto status = api("attribute.write");
    if (status) {
        assert(values.size() == curve->numberOfPoints());
        curve->attributes[name] = values;
    }
    return status;
}
