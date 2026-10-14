// The complete production callback executes. Maya normal/ramp/history and
// payload-owner APIs are explicit injectable seams, not a native DG emulator.
#define MRampAttribute InitializerRamp
#include "maya_attribute_doubles.h"
#undef MRampAttribute
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DGeometryLayout.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DTimeSamplingBudget.h"
#include <maya/MTime.h>
#include <algorithm>
#include <iostream>
#include <map>

using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using DScalarDataList = std::vector<float>;
using DVec3Data = Imath::V3f;
using DVec3DataList = std::vector<DVec3Data>;
using MTypeId = int;
#define CHECK_MSTATUS_AND_RETURN_IT(expression) DOMMY_RETURN_IF_FAILURE(expression)
template <class T>
std::shared_ptr<TypedDPAttributeData<T>> field(std::size_t count, const T& value) {
    return std::make_shared<TypedDPAttributeData<T>>(std::vector<T>(count, value));
}
int copies = 0, samples = 0, publications = 0, cleans = 0;
bool nullInput = false, nullCreated = false, nullBinding = false;
bool invalidTopology = false, rejectAdd = false, shortWrite = false, rejectPositions = false;
float rampValue = 1.0f;
void hostVoid(const std::string& name) {
    if (!operation(name))
        throw std::runtime_error("injected host failure");
}
struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    MVector operator*(double n) const {
        return {x * n, y * n, z * n};
    }
    MVector operator/(double n) const {
        return *this * (1.0 / n);
    }
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    double angle(const MVector& v) const {
        return std::acos(
            std::max(-1.0, std::min(1.0, (x * v.x + y * v.y + z * v.z) / (length() * v.length()))));
    }
};
struct MPoint {
    double x, y, z, w;
    MPoint(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z), w(1) {}
    MVector operator-(const MPoint& p) const {
        return {x - p.x, y - p.y, z - p.z};
    }
    MPoint operator+(const MVector& v) const {
        return {x + v.x, y + v.y, z + v.z};
    }
};
void AddAttributeList(DPAttrList&, const std::string&, const DPAttributeData&);
struct DMeshInternalData {
    DPointList points;
    DPAttrList fields;
    std::size_t numberOfPoints() const {
        return points.size();
    }
    const DPointList& getPositionConst() const {
        return points;
    }
    const DPointList* getPositionPtrConst() const {
        return &points;
    }
    bool hasValidMeshData() const {
        if (invalidTopology)
            return false;
        for (const auto& p : points)
            for (int a = 0; a < 3; ++a)
                if (!std::isfinite(p[a]))
                    return false;
        dommy::attribute::ValidationBudget budget;
        return dommy::attribute::validateAttributeList(fields, points.size(), budget);
    }
    bool replacePositions(const DPointList& p) {
        hostVoid("positions.replace");
        if (rejectPositions)
            return false;
        points = p;
        return hasValidMeshData();
    }
    DPAttrIter beginAttr() {
        return fields.begin();
    }
    DPAttrIter endAttr() {
        return fields.end();
    }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(fields, name);
    }
    bool isExistAttr(const std::string& name) const {
        return bool(getAttr(name));
    }
    void removeAttr(const std::string& name) {
        hostVoid("attribute.remove");
        fields.erase(name);
    }
    void addAttr(const std::string& name, const DPAttributeData& field) {
        hostVoid("attribute.add");
        if (!rejectAdd) {
            AddAttributeList(fields, name, field);
            if (shortWrite)
                fields[name] = ::field<float>(0U, 0.0f);
        }
    }
    template <class T>
    void addAttrWithValues(const std::string& name, const std::vector<T>& values) {
        TypedDPAttributeData<T> field(values);
        addAttr(name, field);
    }
};
using DMItem = std::shared_ptr<DMeshInternalData>;
struct DMeshData {
    static const int id = 7;
    std::vector<DMItem> items;
    std::size_t numberOfMesh() const {
        return items.size();
    }
    auto beginMeshes() -> decltype(items.begin()) {
        return items.begin();
    }
    auto endMeshes() -> decltype(items.end()) {
        return items.end();
    }
    auto beginMeshes() const -> decltype(items.begin()) {
        return items.begin();
    }
    auto endMeshes() const -> decltype(items.end()) {
        return items.end();
    }
    bool hasValidMeshTopology() const {
        for (const auto& m : items)
            if (!m || !m->hasValidMeshData())
                return false;
        return !invalidTopology;
    }
    bool getCheckedTotalVertex(std::size_t& n) const {
        n = 0;
        for (const auto& m : items)
            if (!dommy::generation::checkedSum(n, m->numberOfPoints(),
                                               dommy::generation::kMaxGeneratedPointCount, n))
                return false;
        return true;
    }
    MStatus copyFrom(const DMeshData& other) {
        const auto status = operation("mesh.copy");
        if (!status)
            return status;
        items.clear();
        for (const auto& m : other.items) {
            auto c = std::make_shared<DMeshInternalData>(*m);
            c->fields.clear();
            for (const auto& f : m->fields)
                c->fields[f.first] = f.second->copy();
            items.push_back(c);
        }
        ++copies;
        return status;
    }
    void updateBBox() {
        hostVoid("bounds.update");
    }
};
const int DMeshData::id;
DMeshData inputData, stagedData, publishedData;
struct Control {
    double number = 0;
    std::string string;
};
std::map<std::string, Control> controls;
struct MDataHandle {
    std::string name;
    bool valid;
    MDataHandle(std::string name = "", bool valid = true) : name(name), valid(valid) {}
    void check() const {
        assert(valid);
    }
    int asInt() const {
        check();
        return static_cast<int>(controls.at(name).number);
    }
    double asDouble() const {
        check();
        return controls.at(name).number;
    }
    bool asBool() const {
        check();
        return controls.at(name).number != 0;
    }
    MString asString() const {
        check();
        return controls.at(name).string.c_str();
    }
    MStatus set(const MObject&) {
        const auto s = operation("output.set");
        if (s) {
            publishedData = stagedData;
            ++publications;
        }
        return s;
    }
    void setClean() {
        hostVoid("output.clean");
        ++cleans;
    }
};
struct MPlug {
    MObject attr;
    explicit MPlug(const MObject& a) : attr(a) {}
    MPlug(const MObject&, const MObject& a) : attr(a) {}
    bool operator!=(const MObject& a) const {
        return attr.name != a.name;
    }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& a, MStatus* s) {
        *s = operation("input." + a.name);
        return {a.name, bool(*s)};
    }
    MDataHandle outputValue(const MObject&, MStatus* s) {
        *s = operation("output.handle");
        return {"output", bool(*s)};
    }
    MStatus setClean(const MPlug&) {
        const auto s = operation("block.clean");
        if (s)
            ++cleans;
        return s;
    }
};
struct MFnPluginData {
    MObject create(int, MStatus* s) {
        *s = operation("data.create");
        return *s && !nullCreated ? MObject{"created"} : MObject::kNullObj;
    }
    MObject object() const {
        return nullCreated ? MObject::kNullObj : MObject{"created"};
    }
};
struct MItMeshVertex {
    MItMeshVertex(const MObject&, MStatus* s) {
        *s = operation("vertex.bind");
    }
    MStatus setIndex(int i, int& old) {
        assert(i >= 0);
        old = 0;
        return operation("vertex.index");
    }
    MStatus getNormal(MVector& n) {
        const auto s = operation("vertex.normal");
        if (s)
            n = {1, 0, 0};
        return s;
    }
};
struct MRampAttribute {
    MRampAttribute(const MPlug&, MStatus* s) {
        *s = operation("ramp.bind");
    }
    void getValueAtPosition(float p, float& value, MStatus* s) {
        assert(p >= 0 && p <= 1);
        *s = operation("ramp.value");
        if (*s)
            value = rampValue;
    }
};
namespace dommy {
namespace maya {
template <class T> MStatus getPluginDataFromHandle(const MDataHandle& h, int, const T*& out) {
    h.check();
    const auto s = operation("input.bind");
    out = s && !nullInput ? &inputData : nullptr;
    return s;
}
template <class T> T* pluginDataPointer(MFnPluginData&, MStatus* status) {
    *status = operation("data.bind");
    return *status && !nullBinding ? &stagedData : nullptr;
}
template <class T> MStatus getPluginDataFromObject(const MObject& o, int, const T*& out) {
    const auto s = operation("output.inspect");
    out = s && !o.isNull() ? &stagedData : nullptr;
    return s;
}
template <class T> bool validatePluginData(const T& d) {
    return d.hasValidMeshTopology();
}
MStatus getEvaluationTime(MDataBlock&, MTime& time) {
    const auto s = operation("evaluation.time");
    if (s)
        time = MTime(2.0);
    return s;
}
} // namespace maya
} // namespace dommy
MStatus generateMMeshFromDmesh(const DMeshInternalData&, MObject& out) {
    const auto s = operation("mesh.convert");
    if (s)
        out = {"mesh"};
    return s;
}
void checkDMeshPointAttribute(const std::string& name, DMeshData& data, bool remove) {
    if (remove)
        for (auto& m : data.items)
            m->removeAttr(name);
}
MStatus sampleHistory(std::vector<std::vector<DPointList>>& out,
                      const std::vector<std::size_t>& counts, std::size_t total, long count, double,
                      const MTime&) {
    const auto s = operation("history.sample");
    if (!s)
        return s;
    ++samples;
    assert(count >= 2 && counts.size() == inputData.items.size());
    std::size_t n = 0;
    out.clear();
    for (long sample = 0; sample < count; ++sample) {
        std::vector<DPointList> meshes;
        for (const auto& m : inputData.items) {
            DPointList points = m->points;
            for (auto& p : points)
                p[0] -= static_cast<float>(sample);
            meshes.push_back(points);
            if (sample == 0)
                n += points.size();
        }
        out.push_back(meshes);
    }
    assert(n == total);
    return s;
}
#include "motion_deform_compute.inc"

void fixture() {
    resetCalls();
    copies = samples = publications = cleans = 0;
    nullInput = nullCreated = nullBinding = invalidTopology = rejectAdd = shortWrite =
        rejectPositions = false;
    rampValue = 1.0f;
    inputData.items.clear();
    stagedData.items.clear();
    publishedData.items.clear();
    controls.clear();
    auto mesh = std::make_shared<DMeshInternalData>();
    mesh->points = {{4, 0, 0}, {5, 0, 0}};
    mesh->fields["weight"] = field<float>(2U, 0.25f);
    inputData.items.push_back(mesh);
    controls["aSampling"].number = 3;
    controls["aTimeRange"].number = 2;
    controls["aAmplitude"].number = 1;
    controls["aWeight"].number = 1;
    controls["aWeightAttributeName"].string = "weight";
    controls["aIsExportMotionPower"].number = 1;
    controls["aMotionPowerName"].string = "power";
    controls["aIsExportMotionVector"].number = 1;
    controls["aMotionVectorName"].string = "vector";
}
MStatus compute() {
    DMMotionDeform node;
    MDataBlock data;
    return node.compute(MPlug(DMMotionDeform::outDM), data);
}
void checkPosition(float expected) {
    assert(publications == 1);
    assert(publishedData.items[0]->points[0][0] == expected);
    assert(inputData.items[0]->points[0][0] == 4.0f);
}
void rejection() {
    assert(compute() != MS::kSuccess);
    assert(publications == 0 && cleans == 0);
    assert(inputData.items[0]->points[0][0] == 4.0f);
}
int main() {
    fixture();
    assert(compute() == MS::kSuccess);
    checkPosition(3.5f);
    const auto baseline = operations;
    assert(std::dynamic_pointer_cast<const DPFloatAttr>(publishedData.items[0]->getAttr("power"))
               ->value()[0] == 1.0f);
    assert(std::dynamic_pointer_cast<const DPVec3fAttr>(publishedData.items[0]->getAttr("vector"))
               ->value()[0][0] == 1.0f);
    for (const char* output : {"aMotionPowerName", "aMotionVectorName"}) {
        fixture();
        controls[output].string = "weight";
        assert(compute() == MS::kSuccess);
        checkPosition(3.5f);
        assert(inputData.items[0]->getAttr("weight")->getAttrType() == DP_ATTR_FLOAT);
        assert(std::dynamic_pointer_cast<const DPFloatAttr>(inputData.items[0]->getAttr("weight"))
                   ->value()[0] == 0.25f);
    }
    for (const auto& weight : std::vector<DPAttributeData::Ptr>{
             field<int32_t>(2U, 1), field<uint32_t>(2U, 1U), field<int64_t>(2U, 1),
             field<float>(2U, 1.0f), field<double>(2U, 1.0)}) {
        fixture();
        inputData.items[0]->fields["weight"] = weight;
        assert(compute() == MS::kSuccess);
        checkPosition(2.0f);
    }
    for (const auto& weight : std::vector<DPAttributeData::Ptr>{
             field<bool>(2U, true), field<Imath::V2f>(2U, Imath::V2f(1, 1)),
             field<Imath::V3f>(2U, Imath::V3f(1, 1, 1)), field<std::string>(2U, "x"),
             field<MMatrix>(2U, MMatrix())}) {
        fixture();
        inputData.items[0]->fields["weight"] = weight;
        rejection();
    }
    fixture();
    controls["aMotionVectorName"].string = "power";
    rejection();
    assert(copies == 0 && samples == 0);
    fixture();
    controls["aMotionPowerName"].string.assign(dommy::generation::kMaxGeneratedStringByteCount + 1U,
                                               'a');
    rejection();
    assert(copies == 0 && samples == 0);
    fixture();
    inputData.items[0]->fields["power"] = field<std::string>(2U, "old");
    inputData.items[0]->fields["vector"] = field<float>(2U, 7.0f);
    assert(compute() == MS::kSuccess);
    checkPosition(3.5f);
    assert(publishedData.items[0]->getAttr("power")->getAttrType() == DP_ATTR_FLOAT);
    assert(publishedData.items[0]->getAttr("vector")->getAttrType() == DP_ATTR_VEC3F);
    assert(inputData.items[0]->getAttr("power")->getAttrType() == DP_ATTR_STRING);
    fixture();
    inputData.items[0]->fields.erase("weight");
    assert(compute() == MS::kSuccess);
    checkPosition(2.0f);
    fixture();
    inputData.items[0]->fields["weight"] = field<double>(2U, 0.1);
    assert(compute() == MS::kSuccess);
    checkPosition(static_cast<float>(4.0 - 2.0 * static_cast<float>(0.1)));
    fixture();
    auto second = std::make_shared<DMeshInternalData>();
    second->points = {{8, 0, 0}};
    second->fields["weight"] = field<float>(1U, 0.5f);
    inputData.items.push_back(second);
    controls["aMotionPowerName"].string = "weight";
    assert(compute() == MS::kSuccess);
    checkPosition(3.5f);
    assert(publishedData.items[1]->points[0][0] == 7.0f);
    assert(inputData.items[1]->points[0][0] == 8.0f);
    fixture();
    rampValue = 0.25f;
    assert(compute() == MS::kSuccess);
    checkPosition(3.875f);
    fixture();
    controls["aSampling"].number = 1;
    controls["aWeight"].number = NAN;
    controls["aMotionVectorName"].string = "power";
    assert(compute() == MS::kSuccess);
    checkPosition(4.0f);
    assert(samples == 0);
    fixture();
    inputData.items.clear();
    controls["aWeight"].number = NAN;
    controls["aMotionVectorName"].string = "power";
    assert(compute() == MS::kSuccess);
    assert(samples == 0);
    fixture();
    inputData.items[0]->points.clear();
    inputData.items[0]->fields["weight"] = field<float>(0U, 0.25f);
    assert(compute() == MS::kSuccess);
    assert(publishedData.items[0]->points.empty());
    assert(publishedData.items[0]->getAttr("power")->attrSize() == 0U);
    fixture();
    controls["aIsExportMotionVector"].number = 0;
    controls["aMotionVectorName"].string = "power";
    assert(compute() == MS::kSuccess);
    checkPosition(3.5f);
    fixture();
    controls["aMotionPowerName"].string = "";
    controls["aMotionVectorName"].string = "";
    assert(compute() == MS::kSuccess);
    checkPosition(3.5f);
    assert(!publishedData.items[0]->getAttr("power"));
    for (int mode = 0; mode < 6; ++mode) {
        fixture();
        if (mode == 0)
            nullInput = true;
        if (mode == 1)
            nullCreated = true;
        if (mode == 2)
            nullBinding = true;
        if (mode == 3) {
            rejectAdd = true;
            controls["aIsExportMotionVector"].number = 0;
        }
        if (mode == 4)
            shortWrite = true;
        if (mode == 5)
            rejectPositions = true;
        rejection();
    }
    for (MS::MStatusCode error : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = error;
        for (std::size_t i = 0; i < baseline.size(); ++i) {
            fixture();
            failAt = static_cast<int>(i);
            const auto result = compute();
            assert(result != MS::kSuccess);
            if (baseline[i] == "input.inDM" || baseline[i] == "input.bind")
                assert(result == error);
            assert(inputData.items[0]->points[0][0] == 4.0f);
            if (baseline[i] != "output.clean" && baseline[i] != "block.clean")
                assert(publications == 0);
            fixture();
            assert(compute() == MS::kSuccess);
            checkPosition(3.5f);
        }
    }
    for (std::size_t i = 0; i < baseline.size(); ++i) {
        fixture();
        attributeThrowAt = static_cast<int>(i);
        assert(compute() != MS::kSuccess);
        assert(inputData.items[0]->points[0][0] == 4.0f);
        fixture();
        assert(compute() == MS::kSuccess);
        checkPosition(3.5f);
    }
    fixture();
    DMMotionDeform node;
    MDataBlock data;
    assert(node.compute(MPlug(MObject{"unknown"}), data) == MS::kUnknownParameter);
    assert(operations.empty());
    std::cout << "Complete motion compute: aliases, strict scalar weights, output names/types, "
                 "failures and retry passed\n";
}
