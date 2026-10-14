#pragma once

#define DOMMY_REAL_POINT_ATTRIBUTES
#include "mesh_emitter_doubles.h"
#undef DOMMY_REAL_POINT_ATTRIBUTES
#include "common/utility/DTimeSamplingBudget.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DNumericValidation.h"
#include "math/DIndexOrder.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include <maya/MTime.h>
#include <numeric>

using DAUIntList = std::vector<unsigned int>;
using ShiftPointList = std::vector<MPoint>;
void AddAttributeList(DPAttrList&, const std::string&, const DPAttributeData&);
static bool rejectFloatField=false, rejectIdField=false;
struct ShiftData {
    static constexpr MFnData::Type id = MFnData::kAny;
    ShiftPointList points;
    DPAttrList fields;
    DPAttrIter beginAttr() { return fields.begin(); }
    DPAttrIter endAttr() { return fields.end(); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(fields, name);
    }
    size_t numberOfPoints() const { return points.size(); }
    const ShiftPointList* getPositionPtrConst() const { return &points; }
    bool isExistAttr(const std::string& name) const { return bool(getAttr(name)); }
    bool hasValidPointTopology() const {
        for (const auto& point : points)
            if (!dommy::maya::isFinitePoint(point)) return false;
        dommy::attribute::ValidationBudget budget;
        return dommy::attribute::validateAttributeList(fields, points.size(), budget);
    }
    void setPosition(const ShiftPointList& p) {
        if (!api("point.set")) throw std::runtime_error("point set failed");
        points = p;
        if (shortPointStorage && !points.empty()) points.pop_back();
    }
    void addAttrWithValues(const std::string& name, const DScalarDataList& v) {
        addAttr(name, DPFloatAttr(v));
    }
    void addAttr(const std::string& name, const DPAttributeData& v) {
        if (!api(v.getAttrType()==DP_ATTR_FLOAT ? "point.times" : "point.ids"))
            throw std::runtime_error("attribute failed");
        if ((!rejectFloatField || v.getAttrType()!=DP_ATTR_FLOAT) &&
            (!rejectIdField || v.getAttrType()!=DP_ATTR_UINT32)) AddAttributeList(fields, name, v);
    }
    void updateBBox() { if (!api("point.bounds")) throw std::runtime_error("bounds failed"); }
};
ShiftData shiftStaging, shiftOutput;
std::map<double, ShiftData> timePoints;
std::map<double, MObject> timeObjects;
std::map<std::string, std::vector<double>> scalarPayloads;
unsigned scalarReportedCount = 0U;
MTime activeTime;
std::vector<MTime> queriedTimes;
struct MDGContext { MTime time; explicit MDGContext(MTime t) : time(t) {} };
struct MDGContextGuard {
    MTime previous;
    explicit MDGContextGuard(const MDGContext& c) : previous(activeTime) { activeTime = c.time; }
    ~MDGContextGuard() { activeTime = previous; }
};
struct ShiftPlug : MPlug {
    using MPlug::MPlug;
    MStatus getValue(MObject& object) const {
        const MStatus s = api("plug.object");
        queriedTimes.push_back(activeTime);
        if (s) object = timeObjects.at(activeTime.value());
        return s;
    }
};
struct ShiftHandle : MDataHandle {
    ShiftHandle(MDataHandle h = {}) : MDataHandle(h) {}
    MTime asTime() const { check(); return MTime(input->number, MTime::kNTSCFrame); }
};
struct ShiftBlock : MDataBlock {
    std::map<std::string,Input> contextInputs;
    ShiftHandle inputValue(MObject a, MStatus* s) {
        ShiftHandle handle = MDataBlock::inputValue(a,s);
        const auto found = contextInputs.find(a.name);
        if (*s && found != contextInputs.end()) handle.input = &found->second;
        return handle;
    }
    ShiftHandle outputValue(MObject a, MStatus* s) { return MDataBlock::outputValue(a,s); }
};
namespace dommy { namespace maya {
struct ScopedPlugDataHandle {
    Input input;
    ShiftHandle value;
    ShiftHandle& handle() { return value; }
    MStatus read(const ShiftPlug& plug) {
        const auto s = api("plug.handle");
        queriedTimes.push_back(activeTime);
        if (s) {
            if (plug.attribute == "inDP") {
                input.plugin = &timePoints.at(activeTime.value());
                input.type = MFnData::kAny;
            } else input = inputs.at(plug.attribute);
            value.input = &input; value.valid = true;
        }
        return s;
    }
};
template <> inline MStatus createPluginDataObject<ShiftData>(MFnData::Type, MObject& object, ShiftData*& out) {
    const auto s = api("plugin.create");
    if (s) { shiftStaging = ShiftData(); object = {"outputData"}; out = &shiftStaging; }
    return s;
}
inline MStatus publishPluginDataObject(MDataHandle& h, const MObject&, const ShiftData& value) {
    h.check();
    dommy::attribute::ValidationBudget budget;
    if (!dommy::attribute::validateAttributeList(value.fields, value.numberOfPoints(), budget))
        return MS::kInvalidParameter;
    const auto s = api("publish");
    if (s) { shiftOutput = value; ++published; }
    return s;
}
} }
const ShiftData* getInputDPfromHandle(ShiftHandle h) {
    h.check();
    if (!api("point.read")) return nullptr;
    return static_cast<const ShiftData*>(h.input->plugin);
}
template<class A> struct ShiftArrayData {
    MObject object;
    ShiftArrayData(MObject o,MStatus* s):object(o){*s=api("scalar.bind");}
    unsigned length(MStatus* s) const {*s=api("scalar.length");return scalarReportedCount ? scalarReportedCount : scalarPayloads.at(object.name).size();}
    A array(MStatus* s) const {
        const int index = callCount; *s=api("scalar.array"); A values;
        if (*s) values.assign(scalarPayloads.at(object.name).begin(),scalarPayloads.at(object.name).end());
        if (*s && index == shortAt && !values.empty()) {values.pop_back();shortApplied=true;}
        return values;
    }
};
struct ShiftCurve : MFnNurbsCurve {
    using MFnNurbsCurve::MFnNurbsCurve;
    MStatus getCVs(MPointArray& p, MSpace::Space space) const {
        assert(space==MSpace::kWorld);
        return MFnNurbsCurve::getCVs(p,MSpace::kObject);
    }
};
struct ShiftSurface {
    MObject object;
    ShiftSurface(MObject o,MStatus* s):object(o){*s=api("surface.bind");}
    int numCVsInU(MStatus* s) const {*s=api("surface.u");return geometries.at(object.name).reportedCvCount;}
    int numCVsInV(MStatus* s) const {*s=api("surface.v");return 1;}
    MStatus getCVs(MPointArray& p,MSpace::Space space) const {
        assert(space==MSpace::kWorld); const int index=callCount; const auto s=api("surface.cvs");
        if(s){p=geometries.at(object.name).points;if(index==shortAt&&!p.empty()){p.pop_back();shortApplied=true;}}
        return s;
    }
};
struct ShiftMatrixData {
    ShiftMatrixData(MObject,MStatus* s){*s=api("matrix.bind");}
    MMatrix matrix(MStatus* s)const{*s=api("matrix.read");return MMatrix();}
};
struct ShiftRamp : MRampAttribute {
    ShiftRamp() = default;
    ShiftRamp(const ShiftPlug&, MStatus* s) {*s=api("ramp.bind");}
    void getValueAtPosition(float position,float& value,MStatus* s)const{
        *s=api("ramp.value");if(*s)value=position;
    }
};
namespace tbb {
template<class T>struct blocked_range {
    T first,last;
    blocked_range(T a,T b,int):first(a),last(b){}
    T begin()const{return first;} T end()const{return last;}
};
}

#define DPData ShiftData
#define DPointList ShiftPointList
#define MDataHandle ShiftHandle
#define MDataBlock ShiftBlock
#define MPlug ShiftPlug
#define MFnNurbsCurve ShiftCurve
#define MFnNurbsSurface ShiftSurface
#define MFnMatrixData ShiftMatrixData
#define MRampAttribute ShiftRamp
#define MFnFloatArrayData ShiftArrayData<MFloatArray>
#define MFnDoubleArrayData ShiftArrayData<MDoubleArray>
#define MFnIntArrayData ShiftArrayData<MIntArray>
#include "timeshift.inc"
#undef MFnIntArrayData
#undef MFnDoubleArrayData
#undef MFnFloatArrayData
#undef MRampAttribute
#undef MFnMatrixData
#undef MFnNurbsSurface
#undef MFnNurbsCurve
#undef MPlug
#undef MDataBlock
#undef MDataHandle
#undef DPData
#undef DPointList
