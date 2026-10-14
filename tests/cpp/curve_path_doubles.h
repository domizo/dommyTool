#pragma once

// Geometry is a parameter trace, not NURBS. DOMMY_TYPED_PATH_ATTRIBUTES uses
// actual typed attributes, validation and the extracted legacy writer in one
// map; other geometry suites retain their documented attribute transport seam.
#include "curve_copy_doubles.h"
#include "modules/curve/DCurve/Utility/DCurveDividePlan.h"

#if defined(DOMMY_REAL_POINT_ATTRIBUTES) || defined(DOMMY_TYPED_PATH_ATTRIBUTES)
#include "common/utility/DAttributeArrayConversion.h"
#else
namespace Imath {
struct V3f {
    float x, y, z;
    V3f(float x, float y, float z) : x(x), y(y), z(z) {}
};
} // namespace Imath
struct DPVec3fAttr {
    std::vector<Imath::V3f> values;
    DPVec3fAttr(const std::vector<Imath::V3f>& v, Imath::V3f) : values(v) {}
};
struct DPDoubleAttr {
    std::vector<double> values;
    DPDoubleAttr(const std::vector<double>& v, double) : values(v) {}
};
#endif
namespace {
DPData pointStorage, pointOutput;
bool shortAttribute = false;
bool shortPointStorage = false;
} // namespace
inline void DPData::setPosition(const MPointArray& values) {
    if (!api("point.set"))
        throw std::runtime_error("point replacement failed");
    points.clear();
    for (const auto& p : values)
        points.push_back({{p.x, p.y, p.z}});
    if (shortPointStorage && !points.empty())
        points.pop_back();
}
inline void DPData::updateBBox() {
    if (!api("point.bounds"))
        throw std::runtime_error("bounds failed");
}
#ifndef DOMMY_TYPED_PATH_ATTRIBUTES
template <> inline void DPData::addAttr(const std::string& name, const DPVec3fAttr& attr) {
    if (!api("point.attribute"))
        throw std::runtime_error("attribute failed");
    // This is private mock storage, not a second Maya API call.
    attributes[name] = MVectorArray();
    scalarAttributes.erase(name);
#ifdef DOMMY_REAL_POINT_ATTRIBUTES
    for (const auto& v : attr.value())
#else
    for (const auto& v : attr.values)
#endif
        attributes[name].push_back({v.x, v.y, v.z});
}
template <> inline void DPData::addAttr(const std::string& name, const DPDoubleAttr& attr) {
    if (!api("point.attribute"))
        throw std::runtime_error("attribute failed");
    attributes.erase(name);
#ifdef DOMMY_REAL_POINT_ATTRIBUTES
    scalarAttributes[name] = attr.value();
#else
    scalarAttributes[name] = attr.values;
#endif
}
#endif
namespace dommy {
namespace maya {
inline bool isFloatRepresentablePoint(const MPoint& p) {
    return isFinitePoint(p) && numeric::canRepresentAsFloat(p.x) &&
           numeric::canRepresentAsFloat(p.y) && numeric::canRepresentAsFloat(p.z);
}
template <class T> MStatus createPluginDataObject(MFnData::Type, MObject& object, T*& output) {
    const auto s = api("plugin.create");
    if (s) {
        pointStorage = DPData();
        object = {"pointData"};
        output = &pointStorage;
    }
    return s;
}
inline MStatus publishPluginDataObject(MDataHandle& handle, const MObject& object,
                                       const DPData& value) {
    handle.check();
    assert(!object.isNull() && value.hasValidPointTopology());
    const auto s = api("publish");
    if (s) {
        pointOutput = value;
        ++published;
    }
    return s;
}
} // namespace maya
} // namespace dommy
inline void DCGetMDoubleArrayFromCurveAttribute(const DCurveData& input, const std::string& name,
                                                MDoubleArray& values) {
    // Attribute dispatch/conversion is a boundary; malformed cardinality is injected.
    if (!api("attribute.read"))
        throw std::runtime_error("attribute failed");
    const auto& source = input.attributes.at(name);
    for (unsigned i = 0; i < values.length() && i < source.size(); ++i)
        values[i] = source[i];
    if (shortAttribute && values.length())
        values.pop_back();
}
