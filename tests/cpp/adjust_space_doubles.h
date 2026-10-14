#pragma once

// Real numeric algorithms, typed attributes and aggregate payload checks;
// explicit Maya/custom-data owner boundaries, not native ownership emulation.
#include "common/utility/DPointPayloadBudget.h"
#include "common/utility/DWorkBudget.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "geometry/DGeometryTypes.h"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using ConstDPointIter = DPointList::const_iterator;
int failAt = -1, throwAt = -1, callCount = 0;
int copies = 0, publications = 0, cleans = 0, creations = 0, bulkReads = 0, writes = 0;
MS::MStatusCode injectedStatus = MS::kFailure;
std::vector<std::string> trace;
std::vector<bool> statusSites;
bool missingInput = false, nullOutput = false, shortRemap = false;
double radius = 1.0, retained = 100.0;
int iterations = 1;
bool uniform = true;

MStatus operation(const std::string& name, bool statusSite = true) {
    trace.push_back(name);
    statusSites.push_back(statusSite);
    const int at = callCount++;
    if (at == throwAt) throw std::runtime_error("injected boundary exception");
    return at == failAt ? injectedStatus : MS::kSuccess;
}
void voidOperation(const char* name) { (void)operation(name, false); }

struct Group {
    std::vector<bool> ids;
    const std::vector<bool>& getGroupIds() const { return ids; }
};
struct DPData {
    static const int id = 1;
    DPointList points;
    DPAttrList attributes;
    std::map<std::string, std::shared_ptr<Group>> groups;
    std::size_t countOverride = 0U;
    bool valid = true;

    std::size_t numberOfPoints() const { return countOverride ? countOverride : points.size(); }
    bool hasValidPointTopology() const {
        if (!valid) return false;
        for (const auto& point : points)
            if (!dommy::attribute::isValidValue(point)) return false;
        return true;
    }
    auto beginAttr() const -> decltype(attributes.begin()) { return attributes.begin(); }
    auto endAttr() const -> decltype(attributes.end()) { return attributes.end(); }
    auto beginGrp() const -> decltype(groups.begin()) { return groups.begin(); }
    auto endGrp() const -> decltype(groups.end()) { return groups.end(); }
    ConstDPointIter beginPt() const { voidOperation("points.read"); ++bulkReads; return points.begin(); }
    ConstDPointIter endPt() const { return points.end(); }
    void setDPData(const DPData& source) {
        voidOperation("data.copy");
        *this = source;
        for (auto& field : attributes) field.second = field.second->copy();
        for (auto& group : groups) group.second = std::make_shared<Group>(*group.second);
        ++copies;
    }
    bool remapPointFromIds(const DPIndexList& ids) {
        if (!operation("data.remap")) return false;
        DPointList selected;
        for (auto id : ids) selected.push_back(points.at(id));
        for (auto& field : attributes) assert(field.second->remapFromIds(ids));
        for (auto& group : groups) {
            std::vector<bool> selectedIds;
            for (auto id : ids) selectedIds.push_back(group.second->ids.at(id));
            group.second->ids = selectedIds;
        }
        points = selected;
        if (shortRemap && !points.empty()) points.pop_back();
        return true;
    }
    void setPosition(const DPoint& point, std::size_t index) {
        voidOperation("data.position");
        assert(dommy::attribute::isValidValue(point));
        points.at(index) = point;
        ++writes;
    }
    void updateBBox() { voidOperation("data.bounds"); }
};
DPData inputData, stagedData, publishedData;
struct MObject { std::string name; };
struct MPlug {
    MObject attribute;
    explicit MPlug(const MObject& attr) : attribute(attr) {}
    bool operator!=(const MObject& attr) const { return attribute.name != attr.name; }
};
struct MDataHandle {
    std::string name;
    bool valid;
    MDataHandle(const std::string& label, bool ok) : name(label), valid(ok) {}
    double asDouble() const { assert(valid); return name == "aNeighborRadius" ? radius : retained; }
    int asInt() const { assert(valid); return iterations; }
    bool asBool() const { assert(valid); return uniform; }
    MStatus set(const MObject&) {
        assert(valid);
        const auto status = operation("output.set");
        if (status) { publishedData = stagedData; ++publications; }
        return status;
    }
    void setClean() { assert(valid); voidOperation("output.clean"); ++cleans; }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& attr, MStatus* status) {
        *status = operation("read." + attr.name);
        return {attr.name, bool(*status)};
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = operation("output.handle");
        return {"output", bool(*status)};
    }
    MStatus setClean(const MPlug&) { return operation("block.clean"); }
};
void EchoWarning(const char*, const char*) {}
namespace dommy { namespace maya {
template<class T> MStatus getPluginDataFromHandle(const MDataHandle& handle,int,const T*& out) {
    assert(handle.valid);out=nullptr;const auto status=operation("input.data");
    if(status&&!missingInput)out=&inputData;return status;
}
template<class T> MStatus createPluginDataObject(int, MObject& object, T*& output) {
    const auto status = operation("data.create");
    output = status && !nullOutput ? &stagedData : nullptr;
    if (status) { object.name = "output"; ++creations; }
    return status;
}
}}
