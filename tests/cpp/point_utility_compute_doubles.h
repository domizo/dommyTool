#pragma once
// Only Maya/DG and point-container ownership boundaries are doubled. Typed
// attributes, conversion, publication and merge planning use production code.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DFillSpacePlan.h"
#include "common/utility/DPointMergeBudget.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <set>

static int copies = 0, merges = 0, publications = 0;
static bool invalidRead = false, nullInput = false, rejectWrite = false;
static bool rejectFinal = false, emptyOutput = false;
static std::size_t stringLength = 0U, arrayCount = 0U;
static int arrayCopies = 0;
void resetCompute() {
    resetCalls(); copies = merges = publications = arrayCopies = 0;
    invalidRead = nullInput = rejectWrite = rejectFinal = emptyOutput = false;
    stringLength = arrayCount = 0U; injectedStatus = MS::kFailure;
}
void voidApi(const char* label) {
    if (!operation(label)) throw std::runtime_error("injected container failure");
}
struct TestString : MString {
    using MString::MString;
    std::size_t length() const {return stringLength ? stringLength : std::strlen(asChar());}
};
#define MString TestString
using float3 = float[3];
using DPoint = Imath::V3f;
struct MVector {
    double x, y, z;
    MVector(double x=0, double y=0, double z=0):x(x),y(y),z(z) {}
    MVector operator*(double s) const {return {x*s,y*s,z*s};}
};
struct MPoint {
    double x, y, z, w;
    MPoint(double x=0,double y=0,double z=0):x(x),y(y),z(z),w(1) {}
    MVector operator-(const MPoint& p) const {return {x-p.x,y-p.y,z-p.z};}
    MPoint operator+(const MVector& v) const {return {x+v.x,y+v.y,z+v.z};}
    double distanceTo(const MPoint& p) const {
        const auto d=*this-p; return std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    }
};
struct MPointArray {
    std::vector<MPoint> values;
    unsigned int length() const {return static_cast<unsigned int>(values.size());}
    MPoint& operator[](unsigned int i) {return values.at(i);}
    const MPoint& operator[](unsigned int i) const {return values.at(i);}
    MStatus append(const MPoint& p) {
        const MStatus s=operation("point.append"); if(s)values.push_back(p); return s;
    }
};
struct Group {std::vector<bool> ids; const std::vector<bool>& getGroupIds() const {return ids;}};
struct DPData {
    static const int id = 22;
    std::vector<MPoint> points;
    DPAttrList attributes;
    std::map<std::string, std::shared_ptr<Group>> groups;
    bool valid = true;
    std::size_t reportedCount = 0U;
    std::size_t numberOfPoints() const {return reportedCount ? reportedCount : points.size();}
    DPAttrIter beginAttr() {return attributes.begin();} DPAttrIter endAttr() {return attributes.end();}
    ConstDPAttrIter beginAttr() const {return attributes.begin();}
    ConstDPAttrIter endAttr() const {return attributes.end();}
    auto beginGrp() const -> decltype(groups.begin()) {return groups.begin();}
    auto endGrp() const -> decltype(groups.end()) {return groups.end();}
    DPAttributeData::ConstPtr getAttr(const std::string& key) const {
        auto i=attributes.find(key);return i==attributes.end()?nullptr:i->second;
    }
    void addAttr(const std::string& key,const DPAttributeData& attr) {
        voidApi("attribute.add");if(!rejectWrite)attributes[key]=attr.copy();
    }
    void removeAttr(const std::string& key) {voidApi("attribute.remove");attributes.erase(key);}
    bool hasValidPointTopology() const {
        dommy::attribute::ValidationBudget budget(numberOfPoints());
        return valid && dommy::attribute::validateAttributeList(attributes,numberOfPoints(),budget);
    }
    void setDPData(const DPData& other) {
        voidApi("point.copy");++copies;points=other.points;attributes.clear();
        groups=other.groups;reportedCount=other.reportedCount;
        for(const auto& a:other.attributes) attributes[a.first]=a.second->copy();
    }
    bool mergeDPData(const DPData& other) {
        const MStatus s=operation("point.merge");if(!s)return false;
        ++merges;points.insert(points.end(),other.points.begin(),other.points.end());return true;
    }
    // DG boundary only; complete production merge and metadata behavior has a
    // separate typed-container transaction fixture.
    MStatus mergeFrom(const std::vector<const DPData*>& sources) {
        const MStatus status=operation("point.batch.merge");if(!status)return status;
        std::size_t work=0;if(!dommy::point::buildPointMergeBudget(sources,work))return MS::kInvalidParameter;
        ++copies;DPData candidate;
        for(const auto* source:sources)
            candidate.points.insert(candidate.points.end(),source->points.begin(),source->points.end());
        candidate.updateBBox();*this=std::move(candidate);return MS::kSuccess;
    }
    MPointArray getMPosition() const {voidApi("point.export");++arrayCopies;return {points};}
    void setPosition(const MPointArray& p) {voidApi("point.import");points=p.values;}
    void updateBBox() {voidApi("bounds.update");}
};
static DPData inputData, outputData;
static std::vector<const DPData*> arrayInputs;
struct Input {double number=0;float3 vector={0,1,0};MString text;};
struct MDataHandle {
    bool valid=false;const Input* value=nullptr;const DPData* payload=nullptr;
    void check() const {invalidRead=invalidRead||!valid;assert(valid);}
    bool asBool() const {check();return value->number!=0;}
    short asShort() const {check();return static_cast<short>(value->number);}
    int asInt() const {check();return static_cast<int>(value->number);}
    float asFloat() const {check();return static_cast<float>(value->number);}
    double asDouble() const {check();return value->number;}
    const float3& asFloat3() const {check();return value->vector;}
    MString asString() const {check();return value->text;}
    MObject data() const {check();return payload?MObject{"point"}:MObject::kNullObj;}
};
struct MArrayDataHandle {
    std::size_t index=0U;bool valid=false;
    unsigned int elementCount(MStatus* s) {
        assert(valid);*s=operation("array.count");
        return static_cast<unsigned int>(arrayCount ? arrayCount : arrayInputs.size());
    }
    MDataHandle inputValue(MStatus* s) {
        assert(valid);*s=operation("array.input");MDataHandle h;h.valid=bool(*s);
        h.payload=arrayInputs.at(index);return h;
    }
    MStatus next() {const MStatus s=operation("array.next");if(s)++index;return s;}
};
struct MPlug {MObject object;explicit MPlug(const MObject& o):object(o){}
    bool operator!=(const MObject& o) const {return object.name!=o.name;}};
struct MDataBlock {
    std::map<std::string,Input> inputs;
    MDataHandle inputValue(const MObject& a,MStatus* s) {
        *s=operation("input "+a.name);MDataHandle h;h.valid=bool(*s);
        h.value=&inputs[a.name];h.payload=&inputData;return h;
    }
    MArrayDataHandle inputArrayValue(const MObject&,MStatus* s) {
        *s=operation("array.bind");MArrayDataHandle h;h.valid=bool(*s);return h;
    }
    MStatus setClean(const MPlug&) {return operation("output.clean");}
};
namespace dommy {namespace maya {
inline bool isFinitePoint(const MPoint& p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.w);
}
inline bool isFiniteVector(const MVector& p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
template <typename T> MStatus getPluginDataFromHandle(const MDataHandle& h,int,const T*& value) {
    h.check();const MStatus s=operation("point.bind");if(s)value=nullInput?nullptr:h.payload;return s;
}
template <typename T,typename Populate>
MStatus setPluginDataOutput(MDataBlock&,const MObject&,int,Populate populate) {
    MStatus s=operation("output.create");if(!s)return s;
    outputData=DPData();if(emptyOutput)return MS::kFailure;
    s=invokeStatusOperation(populate,outputData);if(!s)return s;
    if(rejectFinal||!outputData.hasValidPointTopology())return MS::kInvalidParameter;
    s=operation("output.handle");if(!s)return s;
    s=operation("output.publish");if(s)++publications;return s;
}
}}
inline const DPData* getInputDPfromHandle(const MDataHandle& h) {
    const DPData* result=nullptr;const MStatus s=dommy::maya::getPluginDataFromHandle(h,DPData::id,result);
    return s&&result&&result->hasValidPointTopology()?result:nullptr;
}
