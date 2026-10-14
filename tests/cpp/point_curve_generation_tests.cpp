// Complete production compute with real typed attributes and small host doubles.
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static int copies = 0, constructions = 0, publications = 0;
static std::size_t admittedBytes = 0;
static bool throwCopy = false, failInsert = false, failPublish = false;
static MStatus splitStatus = MS::kSuccess;
static std::string failedInput;
static MStatus inputFailureStatus = MS::kFailure, bindingStatus = MS::kSuccess;
static bool nullRequiredInput = false, invalidRequiredInput = false;
struct MStringArray;
struct MString {
    std::string value;
    MString(const char* text = "") : value(text) {}
    const char* asChar() const { return value.c_str(); }
    std::size_t length() const { return value.size(); }
    MStatus split(char separator, MStringArray& result) const;
};
struct MStringArray {
    std::vector<MString> values;
    unsigned length() const { return static_cast<unsigned>(values.size()); }
    const MString& operator[](unsigned index) const { return values.at(index); }
};
MStatus MString::split(char separator, MStringArray& result) const {
    if (splitStatus != MS::kSuccess) return splitStatus;
    std::istringstream stream(value); std::string token;
    while (std::getline(stream, token, separator))
        if (!token.empty()) result.values.emplace_back(token.c_str());
    return MS::kSuccess;
}
struct MPoint {
    double x, y, z;
    MPoint(double a=0, double b=0, double c=0) : x(a), y(b), z(c) {}
    double distanceTo(const MPoint& other) const {
        return std::sqrt((x-other.x)*(x-other.x)+(y-other.y)*(y-other.y)+(z-other.z)*(z-other.z));
    }
};
struct MPointArray {
    std::vector<MPoint> values;
    explicit MPointArray(unsigned count=0) : values(count) {}
    unsigned length() const { return static_cast<unsigned>(values.size()); }
    MStatus append(const MPoint& point) { values.push_back(point); return MS::kSuccess; }
    MStatus clear() { values.clear(); return MS::kSuccess; }
    MPoint& operator[](unsigned index) { return values.at(index); }
};
struct MDoubleArray { std::vector<double> values; };
using MDoubleArrayPtr = std::shared_ptr<MDoubleArray>;
using DPointList = std::vector<Imath::V3f>;
static MPoint DPToMP(const Imath::V3f& point) { return {point[0], point[1], point[2]}; }
struct DPGroupData {
    using ConstPtr = std::shared_ptr<const DPGroupData>;
    DPIdList ids;
    explicit DPGroupData(const DPIdList& values) : ids(values) {}
    const DPIdList& getGroupIds() const { return ids; }
};
struct DPData {
    static const int id = 6;
    DPointList points;
    std::map<std::string, DPAttributeData::ConstPtr> attributes;
    std::map<std::string, DPGroupData::ConstPtr> groups;
    std::size_t numberOfPoints() const { return points.size(); }
    const DPointList* getPositionPtrConst() const { return &points; }
    using Iterator = decltype(attributes)::const_iterator;
    Iterator beginAttr() const { return attributes.begin(); }
    Iterator endAttr() const { return attributes.end(); }
    bool isExistGrp(const std::string& name) const { return groups.count(name) != 0; }
    DPGroupData::ConstPtr getGrp(const std::string& name) const { return groups.at(name); }
    bool hasValidPointTopology() const {
        if (invalidRequiredInput) return false;
        for (const auto& point : points)
            for (unsigned int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(point[axis])) return false;
        return true;
    }
};
template <typename T> struct CountedAttribute : TypedDPAttributeData<T> {
    CountedAttribute(const std::vector<T>& values, const T& fallback) : TypedDPAttributeData<T>(values, fallback) {}
    DPAttributeData::Ptr copy() const override {
        ++copies;
        if (throwCopy) throw std::runtime_error("attribute copy");
        return TypedDPAttributeData<T>::copy();
    }
};
struct DCurveInternalData {
    std::vector<MPoint> points;
    int degree;
    bool open;
    std::map<std::string, DPAttributeData::ConstPtr> attributes;
    DCurveInternalData(const MPointArray& cvs, const MDoubleArray&, int value, bool isOpen=true)
        : points(cvs.values), degree(points.size() > static_cast<std::size_t>(value) ? value : 1), open(isOpen) { ++constructions; }
    bool hasValidCurveTopology() const { return points.size() > 1U; }
    std::size_t numberOfPoints() const { return points.size(); }
    void addAttr(const std::string& name, const DPAttributeData& value) { attributes[name] = value.copy(); }
};
using DCItem = std::shared_ptr<DCurveInternalData>;
static void GenerateDCurveKnots(std::size_t points, int degree, MDoubleArray& knots) {
    knots.values.assign(points + static_cast<std::size_t>(degree-1), 0);
}
struct DCurveData {
    static const int id = 7;
    using DCurveList = std::vector<DCItem>;
    DCurveList curves;
    bool insert(const DCurveList& values) { if (failInsert) return false; curves = values; return true; }
    std::size_t numberOfCurves() const { return curves.size(); }
    bool getCheckedTotalCvs(std::size_t& count) const {
        count=0; for (const auto& curve : curves) count += curve->points.size(); return true;
    }
    void updateBBox() {}
};
struct MObject { std::string name; };
struct MPlug {
    std::string name;
    explicit MPlug(const MObject& object) : name(object.name) {}
    bool operator==(const MObject& object) const { return name == object.name; }
};
struct Value { double number = 0; MString text; };
static std::map<std::string, Value> controls;
struct MDataHandle {
    Value value;
    bool valid=true;
    void check() const { assert(valid); }
    int asInt() const { check(); return static_cast<int>(value.number); }
    short asShort() const { check(); return static_cast<short>(value.number); }
    bool asBool() const { check(); return value.number != 0; }
    double asDouble() const { check(); return value.number; }
    MString asString() const { check(); return value.text; }
    void setClean() {}
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& object, MStatus* status) {
        *status = failedInput == object.name ? inputFailureStatus : MStatus(MS::kSuccess);
        MDataHandle handle; handle.value = controls.at(object.name); handle.valid = *status == MS::kSuccess; return handle;
    }
    MDataHandle outputValue(const MObject&, MStatus* status) { *status=MS::kSuccess; return {}; }
    MStatus setClean(const MPlug&) { return MS::kSuccess; }
};
static DPData source, target;
static bool validTarget = true;
static const DPData* getInputDP(const MObject& object, MDataBlock&) {
    return object.name == "aTargetDP" ? (validTarget ? &target : nullptr) : &source;
}
static std::shared_ptr<DCurveData> staging, published;
struct MFnPluginData {
    MObject create(int, MStatus* status) { staging=std::make_shared<DCurveData>(); *status=MS::kSuccess; return {"output"}; }
    MObject object() const { return {"output"}; }
};
namespace dommy { namespace maya {
inline MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DPData*& output) {
    handle.check(); output = bindingStatus && !nullRequiredInput ? &source : nullptr;
    return bindingStatus;
}
inline MStatus getPluginData(MFnPluginData&, DCurveData*& output) { output=staging.get(); return MS::kSuccess; }
template <typename T> MStatus publishTypedPluginDataObject(MDataHandle&, const MObject&, int) {
    if (failPublish) return MS::kFailure;
    published=staging; ++publications; return MS::kSuccess;
}
}}
namespace MGlobal { inline void displayError(const char*) {} }
static void EchoWarning(const char*, const char*) {}
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#define DOMMY_COMPUTE_CATCH(name) catch (...) { return MS::kFailure; }
#include "point_curve_generation.inc"

static void defaults() {
    controls.clear();
    for (const char* name : {"inDP", "aGroup", "aCreateType", "aDegreeType", "aDistanceMethod", "aRandomMethod", "aIsClose", "aSeed", "aStepCount", "aSearchDistance", "aSearchMaxCount"}) controls[name] = {};
    controls["aStepCount"].number = 2;
    controls["aSearchDistance"].number = 1.01;
    controls["aSeed"].number = 7;
    copies=constructions=publications=0;
    throwCopy=failInsert=failPublish=false; failedInput.clear(); splitStatus=MS::kSuccess; validTarget=true;
    inputFailureStatus=MS::kFailure; bindingStatus=MS::kSuccess;
    nullRequiredInput=invalidRequiredInput=false;
    staging.reset(); published.reset();
    source=DPData(); target=DPData();
    for (int i=0; i<6; ++i) source.points.emplace_back(static_cast<float>(i), 0, 0);
    target.points = {{10,0,0}, {11,0,0}, {12,0,0}};
    source.groups["even"] = std::make_shared<DPGroupData>(DPIdList{true,false,true,false,true,false});
    source.groups["first"] = std::make_shared<DPGroupData>(DPIdList{true,false,false,false,false,false});
    source.groups["empty"] = std::make_shared<DPGroupData>(DPIdList(6,false));
    source.attributes["label"] = std::make_shared<CountedAttribute<std::string> >(
        std::vector<std::string>{"a","b","c","d","e","f"}, "fallback");
    source.attributes["ids"] = std::make_shared<CountedAttribute<DIntArray> >(
        std::vector<DIntArray>{{0},{1},{2},{3},{4},{5}}, DIntArray{20,21});
}
static MStatus execute() { MDataBlock block; DCurveFromDP node; return node.compute(MPlug(DCurveFromDP::outDC),block); }
static std::vector<std::vector<double> > positions() {
    std::vector<std::vector<double> > output;
    for (const auto& curve : published->curves) {
        output.emplace_back(); for (const auto& point : curve->points) output.back().push_back(point.x);
    }
    return output;
}
static void expect(const std::vector<std::vector<double> >& expected) {
    assert(execute() == MS::kSuccess && publications == 1);
    assert(positions() == expected);
}

int main() {
    defaults(); expect({{0,1,2,3,4,5}}); assert(copies==2);
    const std::size_t metadataBytes = 1024 + 4*(7*sizeof(std::string)+5+14) + 4*(7*sizeof(DIntArray)+3+8*sizeof(int));
    assert(admittedBytes == 6*8 + 6*256 + metadataBytes + 2048 + 6*96 + 6*512);
    defaults(); controls["aGroup"].text="even"; expect({{0,2,4}});
    assert(admittedBytes == 6*8 + 3*256 + 4*64 + metadataBytes + 2048 + 3*96 + 3*512);
    const auto& attributes = published->curves[0]->attributes;
    auto labels = std::dynamic_pointer_cast<const TypedDPAttributeData<std::string> >(attributes.at("label"));
    assert((labels->value() == std::vector<std::string>{"a","c","e"}) && labels->getDefaultValue()=="fallback");
    auto ids = std::dynamic_pointer_cast<const TypedDPAttributeData<DIntArray> >(attributes.at("ids"));
    assert((ids->value() == std::vector<DIntArray>{{0},{2},{4}}) && (ids->getDefaultValue()==DIntArray{20,21}));
    assert(source.points.size()==6 && source.attributes.at("label")->attrSize()==6);
    defaults(); controls["aGroup"].text="!even first missing"; expect({{0,1,3,5}});
    defaults(); controls["aGroup"].text="missing"; expect({{0,1,2,3,4,5}});
    defaults(); controls["aGroup"].text="empty"; expect({}); assert(copies==0);
    defaults(); controls["aGroup"].text="first"; expect({}); assert(copies==0);

    defaults(); controls["aCreateType"].number=DC_DP_GEN_STEP; throwCopy=true;
    expect({{0,1},{2,3},{4,5}}); assert(copies==0);
    defaults(); controls["aCreateType"].number=DC_DP_GEN_STEP; controls["aStepCount"].number=4;
    expect({{0,1,2,3},{4,5}});
    defaults(); controls["aCreateType"].number=DC_DP_GEN_STEP; controls["aStepCount"].number=5;
    expect({{0,1,2,3,4}});
    defaults(); controls["aCreateType"].number=DC_DP_GEN_DISTANCE; throwCopy=true;
    expect({{0,1},{1,2},{2,3},{3,4},{4,5}}); assert(copies==0);
    assert(admittedBytes == 6*8 + 6*256 + 5*128 + 5*2048 + 10*96 + 2*512);
    defaults(); controls["aCreateType"].number=DC_DP_GEN_DISTANCE; controls["aSearchDistance"].number=3;
    controls["aSearchMaxCount"].number=2;
    expect({{0,1,2},{1,2,3},{2,3,4},{3,4,5},{4,5}});
    defaults(); controls["aCreateType"].number=DC_DP_GEN_DISTANCE; controls["aSearchDistance"].number=3;
    controls["aSearchMaxCount"].number=2; controls["aDistanceMethod"].number=DC_DISTANCE_ONE;
    expect({{0,1},{0,2},{1,2},{1,3},{2,3},{2,4},{3,4},{3,5},{4,5}});
    defaults(); controls["aCreateType"].number=DC_DP_GEN_TARGET; throwCopy=true;
    expect({{0,10},{1,11},{2,12}}); assert(copies==0);
    for (int mode : {DC_RANDOM_ALL, DC_RANDOM_PAIR}) {
        defaults(); controls["aCreateType"].number=DC_DP_GEN_RANDOM; controls["aRandomMethod"].number=mode; throwCopy=true;
        std::vector<double> expected{0,1,2,3,4,5}; std::mt19937 random(7); std::shuffle(expected.begin(), expected.end(), random);
        if(mode==DC_RANDOM_ALL) expect({expected});
        else expect({{expected[0],expected[1]},{expected[2],expected[3]},{expected[4],expected[5]}});
        assert(copies==0);
    }
    for (const char* input : {"aGroup", "aCreateType", "aDegreeType", "aDistanceMethod", "aRandomMethod", "aIsClose", "aSeed", "aStepCount", "aSearchDistance", "aSearchMaxCount"}) {
        defaults(); failedInput=input; assert(execute()!=MS::kSuccess && publications==0 && constructions==0);
    }
    for (const auto status : {MS::kFailure, MS::kInvalidParameter}) {
        defaults(); failedInput="inDP"; inputFailureStatus=status;
        assert(execute()==status && publications==0 && constructions==0);
        defaults(); bindingStatus=status;
        assert(execute()==status && publications==0 && constructions==0);
        defaults(); expect({{0,1,2,3,4,5}});
    }
    defaults(); nullRequiredInput=true; assert(execute()==MS::kInvalidParameter && publications==0);
    defaults(); invalidRequiredInput=true; assert(execute()==MS::kInvalidParameter && publications==0);
    defaults(); splitStatus=MS::kFailure; assert(execute()!=MS::kSuccess && constructions==0 && copies==0);
    defaults(); throwCopy=true; assert(execute()!=MS::kSuccess && publications==0);
    defaults(); failInsert=true; assert(execute()!=MS::kSuccess && publications==0);
    defaults(); failPublish=true; assert(execute()!=MS::kSuccess && publications==0);
    defaults(); controls["aCreateType"].number=DC_DP_GEN_TARGET; validTarget=false; assert(execute()!=MS::kSuccess && publications==0);
    defaults(); source.groups["bad"] = std::make_shared<DPGroupData>(DPIdList{true}); controls["aGroup"].text="bad";
    assert(execute()!=MS::kSuccess && copies==0 && constructions==0);
    defaults(); source.groups["bad"]=nullptr; controls["aGroup"].text="bad";
    assert(execute()!=MS::kSuccess && constructions==0);

    // No large allocation: exercise exact cumulative output admission directly.
    using namespace dommy::generation;
    defaults();
    CurveGenerationStorage metadataExact(kMaxCurveGenerationBytes-metadataBytes);
    preflightPointCurveAttributes(source, metadataExact);
    assert(metadataExact.estimatedBytes()==kMaxCurveGenerationBytes && copies==0);
    bool metadataRejected=false;
    try { CurveGenerationStorage tooMuch(kMaxCurveGenerationBytes-metadataBytes+1); preflightPointCurveAttributes(source,tooMuch); }
    catch(const std::length_error&) { metadataRejected=true; }
    assert(metadataRejected && copies==0);
    const std::size_t outputBytes=2048+3*96+3*512;
    CurveGenerationStorage exact(kMaxCurveGenerationBytes-outputBytes);
    admitPointCurveOutput(exact,1,3,3); assert(exact.estimatedBytes()==kMaxCurveGenerationBytes);
    bool rejected=false; try { admitPointCurveOutput(exact,1,3,3); } catch(const std::length_error&) { rejected=true; }
    assert(rejected);
    // A moderately sized valid source proves admission precedes curve creation.
    defaults(); source.points.resize(350000, Imath::V3f(0,0,0)); source.attributes.clear(); source.groups.clear();
    assert(execute()!=MS::kSuccess && constructions==0 && publications==0);
    std::cout << "Point curves: all modes/order, groups, dynamic defaults, copy elision and failure admission passed\n";
}
