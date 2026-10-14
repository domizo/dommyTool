#pragma once

// Maya/DG, mesh ownership and nearest-search boundaries only. The complete
// node bodies, checked arithmetic and real typed-attribute budgets are used.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DSpatialSearchPlan.h"
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>

namespace {
int throwAt = -1, copies = 0, additions = 0, publications = 0, nameBuilds = 0;
bool rejectNameBuild = false;
bool invalidRead = false, nullInput = false, nullTarget = false, nullOutput = false;
bool badCopy = false, badFinal = false, badNearest = false, shortCenters = false;
MStatus api(const std::string& name) {
    const int index = callCount;
    const MStatus status = operation(name);
    if (index == throwAt) throw std::runtime_error("injected host exception");
    return status;
}
void voidApi(const std::string& name) {
    if (!api(name)) throw std::runtime_error("injected void boundary failure");
}
void resetCompute() {
    resetCalls();
    throwAt = -1;
    copies = additions = publications = 0;
    nameBuilds = 0;
    rejectNameBuild = false;
    invalidRead = nullInput = nullTarget = nullOutput = false;
    badCopy = badFinal = badNearest = shortCenters = false;
    injectedStatus = MS::kFailure;
}
} // namespace

struct GroupStringArray;
struct GroupString : MString {
    using MString::MString;
    GroupString(const MString& value) : MString(value) {}
    std::size_t length() const { return std::strlen(asChar()); }
    GroupString operator+(int value) const {
        ++nameBuilds;
        if (rejectNameBuild) throw std::bad_alloc();
        return GroupString((std::string(asChar()) + std::to_string(value)).c_str());
    }
    MStatus split(char separator, GroupStringArray& output) const;
};
struct GroupStringArray {
    std::vector<GroupString> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    const GroupString& operator[](unsigned int index) const { return values.at(index); }
};
MStatus GroupString::split(char separator, GroupStringArray& output) const {
    const MStatus status = api("string.split");
    if (!status) {
        // Even a partial result from a failed split must never be consumed.
        output.values.push_back(GroupString("remove"));
        return status;
    }
    std::istringstream stream(asChar());
    std::string token;
    while (std::getline(stream, token, separator))
        if (!token.empty()) output.values.emplace_back(token.c_str());
    return status;
}
#define MString GroupString
using MStringArray = GroupStringArray;
using double2 = double[2];
struct Input {
    double number = 0.0;
    double2 range = {0.0, 1.0};
    MString string;
};
struct MDataHandle {
    bool valid = false;
    const Input* input = nullptr;
    std::string attributeName;
    void check() const {
        invalidRead = invalidRead || !valid;
        assert(valid);
    }
    bool asBool() const { check(); return input->number != 0.0; }
    double asDouble() const { check(); return input->number; }
    int asInt() const { check(); return static_cast<int>(input->number); }
    const double2& asDouble2() const { check(); return input->range; }
    MString asString() const { check(); return input->string; }
    void setClean() { check(); }
};
struct MPlug {
    MObject attribute;
    explicit MPlug(const MObject& value) : attribute(value) {}
    MPlug(const MObject&, const MObject& value) : attribute(value) {}
    bool operator!=(const MObject& value) const { return attribute.name != value.name; }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    MDataHandle inputValue(const MObject& attr, MStatus* status) {
        *status = api("input " + attr.name);
        MDataHandle result;
        result.valid = bool(*status);
        result.input = &inputs[attr.name];
        result.attributeName = attr.name;
        return result;
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = api("output.handle");
        MDataHandle result;
        result.valid = bool(*status);
        return result;
    }
    MStatus setClean(const MPlug&) { return api("output.clean"); }
};
using DPoint = std::array<double, 3>;
using DPointList = std::vector<DPoint>;
using ConstDPointIter = DPointList::const_iterator;
using IDs = std::vector<std::size_t>;
using DScalarList = std::vector<double>;
enum DComponentType { D_COMP_POINT, D_COMP_EDGE, D_COMP_FACE };
struct GroupMask {
    DPIdList ids;
    explicit GroupMask(const DPIdList& values) : ids(values) {}
    const DPIdList& getGroupIds() const { return ids; }
};
using Groups = std::map<std::string, std::shared_ptr<GroupMask>>;
struct GroupMesh {
    DPoint center{{0, 0, 0}};
    bool valid = true;
    Groups points, faces, edges;
    std::size_t pointCount = 2U;
    DPAttrList attributes;
    DPointList positions = DPointList(2U);
    bool rejectAttribute = false;
    std::size_t numberOfPoints() const { return pointCount; }
    const DPointList* getPositionPtrConst() const { return &positions; }
    bool isExistAttr(const std::string& name) const { return attributes.count(name); }
    void removeAttr(const std::string& name) { voidApi("attribute.remove"); attributes.erase(name); }
    DPAttrIter beginAttr() { return attributes.begin(); }
    DPAttrIter endAttr() { return attributes.end(); }
    DPAttributeData::Ptr getAttr(const std::string& name) const {
        const auto found = attributes.find(name);
        return found == attributes.end() ? nullptr : found->second;
    }
    void addAttr(const std::string& name, const DPAttributeData& value) {
        voidApi("attribute.add");
        if (!rejectAttribute) attributes[name] = value.copy();
    }
    bool hasValidMeshData() const { return valid; }
    bool hasValidCurveTopology() const { return valid; }
    DPoint getCenter() const { return center; }
    bool isExistGrp(const std::string& name) const { return points.count(name); }
    void removeGrp(const std::string& name) { voidApi("point.remove"); points.erase(name); }
    Groups& groups(DComponentType type) { return type == D_COMP_FACE ? faces : edges; }
    const Groups& groups(DComponentType type) const { return type == D_COMP_FACE ? faces : edges; }
    bool isExistCompGrp(DComponentType type, const std::string& name) const { return groups(type).count(name); }
    bool removeCompGrp(DComponentType type, const std::string& name) {
        if (!api("component.remove")) return false;
        groups(type).erase(name);
        return true;
    }
};
struct DMeshData {
    static const int id = 21;
    using ConstDMeshIter = std::vector<std::shared_ptr<GroupMesh>>::const_iterator;
    using DMeshIter = ConstDMeshIter;
    std::vector<std::shared_ptr<GroupMesh>> meshes;
    Groups groups;
    DPAttrList attributes;
    std::size_t reportedCount = 0U;
    bool valid = true, staged = false;
    void updateBBox() { voidApi("bbox.update"); }
    std::size_t numberOfMesh() const { return reportedCount ? reportedCount : meshes.size(); }
    ConstDMeshIter beginMeshes() const { return meshes.begin(); }
    ConstDMeshIter endMeshes() const { return meshes.end() - (shortCenters && !meshes.empty() ? 1 : 0); }
    ConstDMeshIter beginPrims() const { return meshes.begin(); }
    ConstDMeshIter endPrims() const { return meshes.end(); }
    bool getCheckedTotalVertex(std::size_t& count) const {
        count = 0U;
        for (const auto& mesh : meshes)
            if (!mesh || !dommy::generation::checkedSum(count, mesh->numberOfPoints(),
                    dommy::generation::kMaxGeneratedPointCount, count)) return false;
        return true;
    }
    Groups::const_iterator beginGrp() const { return groups.begin(); }
    Groups::const_iterator endGrp() const { return groups.end(); }
    ConstDPAttrIter beginAttr() const { return attributes.begin(); }
    ConstDPAttrIter endAttr() const { return attributes.end(); }
    bool isExistGrp(const std::string& name) const { return groups.count(name); }
    bool hasValidMeshTopology() const {
        if (!valid || (staged && badFinal)) return false;
        for (const auto& mesh : meshes) if (!mesh || !mesh->valid) return false;
        dommy::attribute::ValidationBudget budget(numberOfMesh());
        if (!dommy::attribute::validateAttributeList(attributes, numberOfMesh(), budget)) return false;
        for (const auto& group : groups)
            if (!group.second || group.second->ids.size() != numberOfMesh() ||
                !dommy::generation::checkedSum(budget.workItems, numberOfMesh(),
                    dommy::generation::kMaxWorkItemCount, budget.workItems)) return false;
        return true;
    }
    MStatus copyFrom(const DMeshData& source) {
        const MStatus status = api("mesh.copy");
        if (!status) return status;
        ++copies;
        *this = source;
        staged = true;
        for (auto& mesh : meshes) mesh = std::make_shared<GroupMesh>(*mesh);
        return badCopy ? MStatus(MS::kFailure) : MStatus(MS::kSuccess);
    }
    void removeGrp(const std::string& name) { voidApi("primitive.remove"); groups.erase(name); }
    void addGrp(const std::string& name, const DPIdList& ids) {
        voidApi("primitive.add");
        ++additions;
        groups.emplace(name, std::make_shared<GroupMask>(ids));
    }
};
struct DPData {
    static const int id = 22;
    DPointList points;
    DPAttrList attributes;
    bool valid = true;
    bool staged = false;
    std::size_t reportedCount = 0U;
    std::size_t numberOfPoints() const { return reportedCount ? reportedCount : points.size(); }
    ConstDPointIter beginPt() const { return points.begin(); }
    ConstDPointIter endPt() const { return points.end(); }
    const DPointList* getPositionPtrConst() const { return &points; }
    void setDPData(const DPData& source) {
        voidApi("point.copy"); ++copies; *this = source; staged = true;
        if (badCopy) throw std::runtime_error("point copy failure");
    }
    bool hasValidPointTopology() const {
        dommy::attribute::ValidationBudget budget(numberOfPoints());
        return valid && !(staged && badFinal) &&
            dommy::attribute::validateAttributeList(attributes, numberOfPoints(), budget);
    }
    bool isExistAttr(const std::string& name) const { return attributes.count(name); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto found = attributes.find(name);
        return found == attributes.end() ? nullptr : found->second;
    }
    void removeAttr(const std::string& name) { voidApi("attribute.remove"); attributes.erase(name); }
    void addAttr(const std::string& name, const DPAttributeData& value) {
        voidApi("attribute.add"); attributes[name] = value.copy();
    }
};
static DMeshData groupInput, groupOutput;
static DPData targetInput;
static DPData pointInput, pointOutput;
static IDs nearestSelections;
static DScalarList distances;
struct MFnPluginData { MObject object() const { return {"output"}; } };
namespace dommy { namespace maya {
template <typename T> MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const T*& value);
template <> inline MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DMeshData*& value) {
    handle.check();
    const MStatus status = api("mesh.bind");
    if (status) value = nullInput ? nullptr : &groupInput;
    return status;
}
template <> inline MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DPData*& value) {
    handle.check();
    const MStatus status = api("target.bind");
    if (status) value = handle.attributeName == "inDP"
        ? (nullInput ? nullptr : &pointInput) : (nullTarget ? nullptr : &targetInput);
    return status;
}
inline MStatus createPluginData(MFnPluginData&, int, DMeshData*& value) {
    const MStatus status = api("output.create");
    if (status) value = nullOutput ? nullptr : &groupOutput;
    return status;
}
template <typename T> MStatus publishTypedPluginDataObject(MDataHandle& handle, const MObject&, int) {
    handle.check();
    const MStatus status = api("output.publish");
    if (status) ++publications;
    return status;
}
} }
template <typename T> struct PointCloud {
    struct Point { T x, y, z; };
    std::vector<Point> pts;
};
namespace nanoflann {
struct KDTreeSingleIndexAdaptorParams { explicit KDTreeSingleIndexAdaptorParams(int) {} };
}
struct DPkdtree {
    DPkdtree(int, const PointCloud<double>&, nanoflann::KDTreeSingleIndexAdaptorParams) {}
    void buildIndex() { voidApi("nearest.build"); }
};
namespace tbb {
template <typename T> struct blocked_range {
    T first, last;
    blocked_range(T first, T last, T) : first(first), last(last) {}
};
template <typename Range, typename Function> void parallel_for(const Range& range, Function function) {
    function(range);
}
}
struct PointSearchKDTreeNearestWithDistanceOp {
    IDs& ids;
    DScalarList& values;
    PointSearchKDTreeNearestWithDistanceOp(DPkdtree&, const DPointList&, IDs& ids, DScalarList& values)
        : ids(ids), values(values) {}
    void operator()(const tbb::blocked_range<std::size_t>& range) {
        voidApi("nearest.query");
        for (std::size_t i = range.first; i < range.last; ++i) {
            ids[i] = badNearest ? targetInput.numberOfPoints() : nearestSelections.at(i);
            values[i] = distances.at(i);
        }
    }
};
