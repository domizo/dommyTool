#pragma once

#include "mesh_curve_copy_doubles.h"
#include "math/DRemapMath.h"
#include <random>
#include "DDataUtility/Data/DPAttributeData.h"
#include "common/utility/DAttributeLookup.h"

#ifndef DOMMY_TYPED_CURVE_ATTRIBUTES
// Older geometry fixtures expose only float metadata. Attribute semantics are
// verified independently by the complete typed Character callback fixture.
namespace dommy { namespace attribute {
template<class Owner>
bool commitStagedAttribute(Owner& owner,const std::string& name,
                           DPAttributeData::Ptr candidate,std::size_t count) {
    const auto field=std::dynamic_pointer_cast<DPFloatAttr>(candidate);
    if (!validAttributeName(name) || !field || field->attrSize()!=count) return false;
    owner.addAttrWithValues(name,field->value());return true;
}
} }
#endif

using DScalarData = float;
using DPIndexList = std::vector<std::size_t>;

namespace {
bool shortAttribute = false, badRamp = false;
int rebuiltReportedCount = -1;
unsigned int rampEntries = 0;
MFloatArray defaultRamp;
} // namespace
struct MCurveAttribute {
    MCurveAttribute(const MPlug&, MStatus* status) {
        *status = api("defaultRamp.bind");
    }
    unsigned int getNumEntries(MStatus* status) {
        *status = api("defaultRamp.count");
        return rampEntries;
    }
    MStatus setCurve(const MFloatArray& values, const MFloatArray& positions) {
        const auto status = api("defaultRamp.set");
        assert(values.size() == 2 && values == positions);
        if (status) {
            defaultRamp = values;
            rampEntries = 2;
        }
        return status;
    }
};
inline MObject MFnNurbsCurve::rebuild(int spans, unsigned int degree, short keepRange,
                                      short endKnots, bool, bool, bool keepCv, double tolerance,
                                      MStatus* status) {
    *status = api("curve.rebuild");
    if (!*status)
        return MObject::kNullObj;
    assert(spans > 0 && (degree == 1 || degree == 3) && keepRange >= 0 && keepRange <= 2 &&
           endKnots >= 0 && endKnots <= 1 && std::isfinite(tolerance));
    auto candidate = geometries.at(object.name);
    candidate.degree = int(degree);
    const MPoint first = candidate.points.front(), last = candidate.points.back();
    if (!keepCv) {
        candidate.points.resize(spans + degree, MPoint(0, 0, 0));
        if (candidate.linearSamples)
            for (unsigned i = 0; i < candidate.points.size(); ++i)
                candidate.points[i] =
                    first + (last - first) * (double(i) / (candidate.points.size() - 1));
    }
    candidate.reportedCvCount = rebuiltReportedCount;
    geometries["rebuilt"] = candidate;
    return {"rebuilt"};
}
// This is only a status/value boundary; it does not model Maya interpolation.
struct TestValueRamp {
    TestValueRamp(const MPlug&, MStatus* status) {
        *status = api("ramp.bind");
    }
    void sampleValueRamp(unsigned count, MFloatArray& values, MStatus* status) {
        const int index = callCount;
        *status = api("ramp.sample");
        if (!*status)
            return;
        values.resize(count);
        for (unsigned i = 0; i < count; ++i)
            values[i] = count > 1 ? float(i) / float(count - 1) : 0;
        if (index == shortAt && values.size())
            values.pop_back();
        if (badRamp && values.size())
            values[0] = NAN;
    }
};
inline void checkDCurveAttribute(const std::string& name, DCurveData& data, bool remove) {
    if (remove) {
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
        data.removeAttr(name);
#else
        data.attributes.erase(name);
#endif
    }
}
inline void checkDCurvePointAttribute(const std::string& name, DCurveData& data, bool remove) {
    if (remove)
        for (auto& curve : data.curves)
#ifdef DOMMY_TYPED_CURVE_ATTRIBUTES
            curve->removeAttr(name);
#else
            curve->attributes.erase(name);
#endif
}
inline void getDCAttributeMDoubleArrayFromScalar(const DCurveData& data, const std::string& name,
                                                 DScalarDataList& values) {
    if (!api("attribute.read"))
        throw std::runtime_error("attribute read failed");
    values = data.attributes.at(name);
    if (shortAttribute && values.size())
        values.pop_back();
}
