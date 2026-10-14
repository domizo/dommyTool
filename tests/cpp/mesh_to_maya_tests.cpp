// Complete production compute and checked input reader; recording host/geometry
// boundaries. Native Maya mesh construction is not exercised here.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <memory>

static int throwAt = -1, publications = 0;
static bool connected = true, wrongType = false, nullData = false, nullMesh = false;
static MStatus api(const std::string& name) {
    if (callCount == throwAt) throw std::runtime_error("injected exception");
    return operation(name);
}
struct StringArray;
struct String : MString {
    using MString::MString;
    MStatus split(char, StringArray&) const;
};
struct StringArray : std::vector<String> {unsigned int length() const {return size();}};
MStatus String::split(char separator, StringArray& result) const {
    const auto status = api("split");
    if (!status) return status;
    std::istringstream stream(asChar()); std::string word;
    while (std::getline(stream, word, separator)) if (!word.empty()) result.emplace_back(word.c_str());
    return status;
}
#define MString String
using MStringArray = StringArray;
struct MPlug {
    MObject object;
    explicit MPlug(const MObject& value) : object(value) {}
    MPlug(const MObject&, const MObject& value) : object(value) {}
    bool operator!=(const MObject& value) const {return object.name != value.name;}
    bool isConnected(MStatus* status) const {*status = api("connected"); return connected;}
};
struct MDataHandle {
    bool valid = false; short mode = 0; MString groups;
    short asShort() const {assert(valid); return mode;}
    MString asString() const {assert(valid); return groups;}
};
struct MDataBlock {
    short mode = 0; MString groups{"chosen missing chosen"};
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = api("input " + attribute.name);
        MDataHandle result; result.valid = bool(*status); result.mode = mode; result.groups = groups;
        return result;
    }
};
using DPIdList = std::vector<bool>;
struct DPGroupData {
    using ConstPtr = std::shared_ptr<const DPGroupData>;
    DPIdList ids;
    const DPIdList& getGroupIds() const {return ids;}
};
struct DMeshInternalData {
    using Ptr = std::shared_ptr<DMeshInternalData>;
    int value = 0;
    static MStatus createMerged(const std::vector<const DMeshInternalData*>& sources, Ptr& result) {
        const auto status = api("merge"); if (!status) return status;
        result = std::make_shared<DMeshInternalData>();
        for (const auto source : sources) result->value += source->value;
        return status;
    }
};
struct DMeshData {
    static const int id = 10;
    using ConstDMeshIter = std::vector<DMeshInternalData::Ptr>::const_iterator;
    std::vector<DMeshInternalData::Ptr> meshes;
    std::map<std::string, std::shared_ptr<DPGroupData>> groups;
    bool valid = true, badTotals = false;
    std::size_t reportedCount = 0;
    bool hasValidMeshTopology() const {return valid;}
    std::size_t numberOfMesh() const {return reportedCount ? reportedCount : meshes.size();}
    ConstDMeshIter beginMeshes() const {return meshes.begin();}
    ConstDMeshIter endMeshes() const {return meshes.end();}
    bool getCheckedTotalVertex(std::size_t& count) const {count = meshes.size() * 3; return !badTotals;}
    bool getCheckedTotalFace(std::size_t& count) const {count = meshes.size(); return !badTotals;}
    bool getCheckedTotalConnect(std::size_t& count) const {count = meshes.size() * 3; return !badTotals;}
    bool isExistGrp(const std::string& name) const {return groups.count(name);}
    DPGroupData::ConstPtr getGrp(const std::string& name) const {return groups.at(name);}
};
static DMeshData inputData;
static std::vector<MObject> published;
namespace dommy { namespace maya {
MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DMeshData*& output) {
    assert(handle.valid); output = nullptr;
    const auto status = api("plugin data");
    if (!status || wrongType) return status ? MStatus(MS::kFailure) : status;
    if (!nullData) output = &inputData;
    return MS::kSuccess;
}
MStatus setObjectArrayOutput(MDataBlock&, const MObject&, const MPlug&, const std::vector<MObject>& values) {
    const auto status = api("publish");
    if (status) {published = values; ++publications;}
    return status;
}
}}
enum class DMeshPointSpace {Transformed};
MStatus generateMMeshFromDmeshWithUV(const DMeshInternalData& source, MObject& result, DMeshPointSpace) {
    const auto status = api("convert");
    if (status && !nullMesh) result = MObject{std::to_string(source.value)};
    return status;
}
void mergeUnionIdListOp(const DPIdList* ids, DPIdList& result) {
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = result[i] || (*ids)[i];
}
struct DMToMesh {
    static MObject inDM, aConvertType, aGroup, outMesh;
    MObject thisMObject() const {return MObject{"node"};}
    MStatus compute(const MPlug&, MDataBlock&);
};
MObject DMToMesh::inDM{"inDM"}, DMToMesh::aConvertType{"type"}, DMToMesh::aGroup{"group"}, DMToMesh::outMesh{"out"};
#include "mesh_to_maya.inc"
static void reset() {
    resetCalls(); throwAt = -1; publications = 0;
    connected = true; wrongType = nullData = nullMesh = false;
    inputData = DMeshData(); published = {MObject{"previous"}};
    for (int i = 1; i <= 3; ++i) {
        auto mesh = std::make_shared<DMeshInternalData>(); mesh->value = i; inputData.meshes.push_back(mesh);
    }
    auto group = std::make_shared<DPGroupData>(); group->ids = {true, false, true}; inputData.groups["chosen"] = group;
}
static MStatus compute(short mode) {
    MDataBlock data; data.mode = mode;
    return DMToMesh().compute(MPlug(DMToMesh::outMesh), data);
}
int main() {
    int injections = 0;
    for (short mode = 0; mode < 3; ++mode) {
        reset(); assert(compute(mode));
        assert(published.size() == (mode == 0 ? 1U : mode == 1 ? 3U : 2U));
        assert(published.front().name == (mode == 0 ? "6" : "1"));
        if (mode == 2) assert(published.back().name == "3");
        const auto trace = operations;
        for (std::size_t i = 0; i < trace.size(); ++i) for (int kind = 0; kind < 2; ++kind) {
            reset(); if (kind) throwAt = int(i); else failAt = int(i);
            assert(!compute(mode) && publications == 0 && published.front().name == "previous"); ++injections;
        }
        for (int bad = 0; bad < 4; ++bad) {
            reset();
            if (bad == 0) wrongType = true;
            if (bad == 1) nullData = true;
            if (bad == 2) inputData.valid = false;
            if (bad == 3) nullMesh = true;
            assert(!compute(mode) && publications == 0);
        }
        reset(); connected = false; assert(compute(mode) && published.empty());
        assert(callCount == 2); // No input or controls read for a disconnected input.
        reset(); inputData.meshes.clear(); inputData.groups.clear();
        assert(compute(mode) && published.empty());
    }
    reset(); assert(!compute(3) && publications == 0);
    reset(); inputData.badTotals = true; assert(!compute(0) && publications == 0);
    reset(); inputData.groups["chosen"]->ids.pop_back(); assert(!compute(2) && publications == 0);
    reset(); inputData.reportedCount = std::size_t(std::numeric_limits<unsigned int>::max()) + 1U;
    assert(!compute(1) && publications == 0);
    reset(); MDataBlock data;
    assert(DMToMesh().compute(MPlug(MObject{"other"}), data) == MS::kUnknownParameter && callCount == 0);
    std::cout << "DMToMesh: all 3 modes, empty/disconnected/invalid inputs; " << injections << " status/exception injections passed\n";
}
