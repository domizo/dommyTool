#pragma once

#include "mesh_emitter_doubles.h"
#include "common/utility/DDeformerMath.h"
#include "common/utility/DTimeSamplingBudget.h"
#include "common/utility/DAttributeLookup.h"
#include <maya/MTime.h>

struct HistoryPoints : std::vector<MPoint> {
    using std::vector<MPoint>::vector;
    std::size_t reported = std::numeric_limits<std::size_t>::max();
    HistoryPoints() = default;
    HistoryPoints(const HistoryPoints& other) : std::vector<MPoint>(other), reported(other.reported) {
        if (!api("positions.copy")) throw std::runtime_error("position copy failed");
    }
    HistoryPoints(HistoryPoints&&) = default;
    HistoryPoints& operator=(const HistoryPoints&) = default;
    std::size_t size() const {
        return reported == std::numeric_limits<std::size_t>::max() ? std::vector<MPoint>::size() : reported;
    }
};
struct HistoryMesh {
    HistoryPoints points;
    bool valid = true;
    bool hasValidMeshData() const { return valid; }
    const HistoryPoints* getPositionPtrConst() const { return &points; }
    std::size_t numberOfPoints() const { return points.size(); }
};
struct HistoryData {
    static constexpr MFnData::Type id = MFnData::kAny;
    std::vector<std::shared_ptr<HistoryMesh>> items;
    std::size_t reported = std::numeric_limits<std::size_t>::max();
    bool valid = true;
    std::size_t numberOfMesh() const {
        return reported == std::numeric_limits<std::size_t>::max() ? items.size() : reported;
    }
    bool hasValidMeshTopology() const { return valid; }
    auto beginMeshes() const -> decltype(items.begin()) { return items.begin(); }
    auto endMeshes() const -> decltype(items.end()) { return items.end(); }
    bool getCheckedTotalVertex(std::size_t& count) const {
        count = 0;
        for (const auto& item : items)
            if (!item || !dommy::generation::checkedSum(count, item->numberOfPoints(),
                    dommy::generation::kMaxGeneratedPointCount, count)) return false;
        return true;
    }
};
struct HistoryTopology {
    MIntArray counts{3}, connections{0,1,2}, uvCounts{3}, uvConnections{0,1,2};
    MFloatArray u{0,1,0}, v{0,0,1};
    int connectionCount = -1, uvCount = -1;
};
std::map<std::string, HistoryTopology> topologies;
std::map<double, HistoryData> meshSnapshots;
std::map<double, Input> snapshots;
MTime activeTime;
std::vector<MTime> queriedTimes;
unsigned selectedGeometry = 0;
MObject input{"input"}, inputGeom{"inputGeom"}, outputGeom{"outputGeom"};

struct MDGContext { MTime time; explicit MDGContext(MTime t) : time(t) {} };
struct MDGContextGuard {
    MTime previous;
    explicit MDGContextGuard(const MDGContext& context) : previous(activeTime) { activeTime = context.time; }
    ~MDGContextGuard() { activeTime = previous; }
};
struct HistoryHandle : MDataHandle {
    HistoryHandle(MDataHandle base = {}) : MDataHandle(base) {}
    MObject asMesh() const { return data(); }
    HistoryHandle child(MObject) const { check(); return *this; }
};
struct HistoryArrayHandle {
    Input* input;
    bool valid;
    MStatus jumpToElement(unsigned index) {
        assert(valid); selectedGeometry = index; return api("array.jump");
    }
    HistoryHandle inputValue(MStatus* status) const {
        assert(valid); *status = api("array.input");
        HistoryHandle result; result.input = input; result.valid = bool(*status); return result;
    }
};
struct HistoryBlock : MDataBlock {
    Input* sample;
    explicit HistoryBlock(Input* value = nullptr) : sample(value) {}
    HistoryHandle inputValue(MObject attr, MStatus* status) { return MDataBlock::inputValue(attr,status); }
    HistoryArrayHandle inputArrayValue(MObject, MStatus* status) {
        *status = api("input.array"); return {sample,bool(*status)};
    }
};
struct HistoryPlug : MPlug {
    using MPlug::MPlug;
    bool isNull() const { return attribute.empty(); }
};
struct HistoryMeshFn : MFnMesh {
    using MFnMesh::MFnMesh;
    int numFaceVertices(MStatus* status) const {
        *status = api("mesh.connectionCount"); const auto& t = topologies.at(object.name);
        return t.connectionCount < 0 ? int(t.connections.size()) : t.connectionCount;
    }
    int numUVs(MStatus* status) const {
        *status = api("mesh.uvCount"); const auto& t = topologies.at(object.name);
        return t.uvCount < 0 ? int(t.u.size()) : t.uvCount;
    }
    template<class ArrayType>
    void resultArray(const ArrayType& source, ArrayType& result, int ordinal) const {
        result = source;
        if (ordinal == shortAt && !result.empty()) { result.pop_back(); shortApplied = true; }
    }
    MStatus getVertices(MIntArray& counts, MIntArray& connections) const {
        const int ordinal = callCount; const auto s = api("mesh.vertices");
        if (s) {resultArray(topologies.at(object.name).counts,counts,ordinal);
                resultArray(topologies.at(object.name).connections,connections,ordinal);}
        return s;
    }
    MStatus getUVs(MFloatArray& u, MFloatArray& v) const {
        const int ordinal = callCount; const auto s = api("mesh.uvs");
        if (s) {resultArray(topologies.at(object.name).u,u,ordinal);resultArray(topologies.at(object.name).v,v,ordinal);}
        return s;
    }
    MStatus getAssignedUVs(MIntArray& counts, MIntArray& connections) const {
        const int ordinal = callCount; const auto s = api("mesh.uvAssignments");
        if (s) {resultArray(topologies.at(object.name).uvCounts,counts,ordinal);
                resultArray(topologies.at(object.name).uvConnections,connections,ordinal);}
        return s;
    }
};
namespace dommy { namespace maya {
struct ScopedPlugDataHandle {
    HistoryHandle value;
    HistoryHandle& handle() { return value; }
    MStatus read(const HistoryPlug&) {
        const auto status = api("plug.read"); queriedTimes.push_back(activeTime);
        if (status) {value.input = &snapshots.at(activeTime.value());value.valid = true;}
        return status;
    }
};
} }
const HistoryData* getInputDMfromHandle(HistoryHandle handle) {
    handle.check();
    if (!api("meshData.read")) return nullptr;
    return static_cast<const HistoryData*>(handle.input->plugin);
}
namespace dommy { namespace maya {
template<class Data>
MStatus getPluginDataFromHandle(const HistoryHandle& handle, MFnData::Type, const Data*& output) {
    handle.check();
    const auto status = api("meshData.read");
    output = status ? static_cast<const Data*>(handle.input->plugin) : nullptr;
    return status;
}
} }

#define DPointList HistoryPoints
#define DMeshData HistoryData
#define MDataHandle HistoryHandle
#define MDataBlock HistoryBlock
#define MArrayDataHandle HistoryArrayHandle
#define MPlug HistoryPlug
#define MFnMesh HistoryMeshFn
#include "motion_history.inc"
#undef MFnMesh
#undef MPlug
#undef MArrayDataHandle
#undef MDataBlock
#undef MDataHandle
#undef DMeshData
#undef DPointList
