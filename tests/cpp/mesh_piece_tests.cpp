// Complete DMMakePiece control flow. Geometry is represented by scalar counts;
// real merge/append transactions are exercised in their separate suites.
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMeshMergePlan.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

static int calls = 0, failAt = -1, throwAt = -1, copies = 0, merges = 0;
static int creations = 0, publications = 0, cleaned = 0;
static std::vector<std::string> operations;
static MStatus api(const char* name) {
    operations.push_back(name);
    const int index = calls++;
    if (index == throwAt) throw std::runtime_error("injected host failure");
    return index == failAt ? MS::kFailure : MS::kSuccess;
}
struct MObject {
    std::string name;
    MObject() = default;
    MObject(const char* value) : name(value) {}
};
struct MPlug {
    MObject object;
    bool operator!=(const MObject& other) const {return object.name != other.name;}
};
struct MDataHandle {
    bool valid;
    MString name;
    MDataHandle(bool value = false, MString text = MString()) : valid(value), name(text) {}
    MString asString() const {assert(valid); return name;}
};
struct MDataBlock {
    MString name{"piece.*"};
    MDataHandle inputValue(MObject, MStatus* status) {
        *status = api("input.control");
        return {bool(*status), name};
    }
    MDataHandle outputValue(MObject, MStatus* status) {
        *status = api("output.handle");
        return {bool(*status), MString()};
    }
    MStatus setClean(const MPlug&) {
        const auto status = api("clean");
        if (status) ++cleaned;
        return status;
    }
};
using DPIndexList = std::vector<std::size_t>;
struct DPGroupData {
    using ConstPtr = std::shared_ptr<const DPGroupData>;
    DPIndexList indices;
    void getActiveIndexList(DPIndexList& output) const {output = indices;}
};
struct DMeshInternalData {
    using Ptr = std::shared_ptr<DMeshInternalData>;
    std::size_t points = 3U, faces = 1U, connections = 3U, uvs = 0U;
    std::vector<int> sourceIds;
    std::size_t numberOfPoints() const {return points;}
    int numFace() const {return int(faces);}
    std::size_t numberOfConnections() const {return connections;}
    std::size_t numberOfUVs() const {return uvs;}
    Ptr copyNewMesh() const {
        ++copies;
        if (!api("copy")) throw std::bad_alloc();
        return std::make_shared<DMeshInternalData>(*this);
    }
    static MStatus createMerged(const std::vector<const DMeshInternalData*>& sources, Ptr& result) {
        ++merges;
        const auto status = api("merge");
        if (!status) return status;
        auto candidate = std::make_shared<DMeshInternalData>();
        candidate->points = candidate->faces = candidate->connections = candidate->uvs = 0U;
        for (const auto source : sources) {
            candidate->points += source->points;
            candidate->faces += source->faces;
            candidate->connections += source->connections;
            candidate->uvs += source->uvs;
            candidate->sourceIds.insert(candidate->sourceIds.end(), source->sourceIds.begin(), source->sourceIds.end());
        }
        result.swap(candidate);
        return status;
    }
};
using DMItem = DMeshInternalData::Ptr;
using ConstDMItem = std::shared_ptr<const DMeshInternalData>;
struct DMeshData {
    static const int id = 1;
    using DMeshList = std::vector<DMItem>;
    DMeshList meshes;
    std::map<std::string, DPGroupData::ConstPtr> groups;
    std::size_t numberOfMesh() const {return meshes.size();}
    ConstDMItem getMeshPtrConst(std::size_t index) const {return meshes.at(index);}
    DPGroupData::ConstPtr getGrpPtr(const std::string& name) const {return groups.at(name);}
    bool insert(const DMeshList& values) {
        if (!api("insert.batch")) return false;
        meshes = values;
        return true;
    }
};
static DMeshData input, staging, output;
static std::vector<std::string> matchedNames;
static MStatus getValidInputDM(MObject, MDataBlock&, const DMeshData*& result) {
    const auto status = api("input.data");
    if (status) result = &input;
    return status;
}
static void getDMeshMatchedGroups(const DMeshData&, const std::string&, std::vector<std::string>& names) {
    // Group matching is a transport boundary, not a regex implementation.
    names = matchedNames;
}
namespace dommy { namespace maya {
template <typename T> MStatus createPluginDataObject(int, MObject&, T*& result) {
    ++creations;
    const auto status = api("plugin.create");
    if (status) {staging = DMeshData(); result = &staging;}
    return status;
}
static MStatus publishPluginDataObject(const MDataHandle& handle, MObject, const DMeshData& value) {
    assert(handle.valid);
    const auto status = api("publish");
    if (status) {output = value; ++publications;}
    return status;
}
}}
#include "mesh_piece.inc"

static void fixture(std::size_t count = 3U) {
    calls = copies = merges = creations = publications = cleaned = 0;
    failAt = throwAt = -1;
    operations.clear(); matchedNames.clear(); input = DMeshData(); staging = DMeshData();
    for (std::size_t i = 0U; i < count; ++i) {
        auto mesh = std::make_shared<DMeshInternalData>();
        mesh->sourceIds = {int(i)};
        input.meshes.push_back(mesh);
    }
}
static void group(const char* name, const DPIndexList& indices) {
    auto value = std::make_shared<DPGroupData>(); value->indices = indices;
    input.groups[name] = value; matchedNames.push_back(name);
}
static MStatus compute(MDataBlock& data) {return DMMakePiece().compute(MPlug{DMMakePiece::outDM}, data);}
int main() {
    MDataBlock data;
    fixture(); group("piece0", {0U,1U}); group("piece1", {1U});
    assert(compute(data) && publications == 1 && output.meshes.size() == 3U);
    assert(output.meshes[0]->sourceIds == std::vector<int>({0,1}));
    assert(output.meshes[1]->sourceIds == std::vector<int>({1}));
    assert(output.meshes[2]->sourceIds == std::vector<int>({2}));
    const auto previous = output.meshes;
    const auto trace = operations;
    const auto publication = std::find(trace.begin(), trace.end(), "publish") - trace.begin();
    for (std::size_t i = 0U; i < trace.size(); ++i) for (bool throws : {false,true}) {
        fixture(); group("piece0", {0U,1U}); group("piece1", {1U});
        output.meshes = previous;
        if (throws) throwAt = int(i); else failAt = int(i);
        assert(!compute(data));
        if (std::ptrdiff_t(i) <= publication)
            assert(publications == 0 && cleaned == 0 && output.meshes == previous);
        assert(input.meshes[0]->sourceIds == std::vector<int>({0}));
    }
    for (bool overlapping : {false,true}) {
        fixture(overlapping ? 1U : 2U);
        for (const auto& mesh : input.meshes) mesh->points = 1100000U;
        if (overlapping) {group("piece0", {0U}); group("piece1", {0U});}
        output.meshes = previous;
        assert(compute(data) == MS::kInvalidParameter);
        assert(merges == 0 && copies == 0 && creations == 0 && publications == 0 && cleaned == 0);
        assert(output.meshes == previous);
    }
    fixture(1U); input.meshes[0]->points = 1048575U;
    group("piece0", {0U}); group("piece1", {0U});
    assert(compute(data) == MS::kInvalidParameter && merges == 0 && copies == 0);
    input.meshes[0]->points = 1048574U;
    assert(compute(data) && publications == 1 && output.meshes.size() == 2U);
    fixture(1U); group("piece0", {1U}); output.meshes = previous;
    assert(!compute(data) && output.meshes == previous && merges == 0 && creations == 0);
    fixture(); data.name = "";
    assert(compute(data) && output.meshes.empty());
    fixture(0U); data.name = "piece.*";
    assert(compute(data) && output.meshes.empty());
    std::cout << "MakePiece admits aggregate/overlapping output before merges or copies; order, exact capacity, retry and failures pass\n";
}
