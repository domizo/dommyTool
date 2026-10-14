#pragma once
// Only Maya/custom-data boundaries are doubled. Compute and array publication
// bodies are extracted unchanged from production.
#include "array_utility_doubles.h"
#include "common/utility/DMeshMergePlan.h"
#include <memory>

using DPIdList = std::vector<bool>;
enum DComponentType { D_COMP_POINT, D_COMP_EDGE, D_COMP_FACE, D_COMP_COUNT };
struct GroupIndexMask {
    using ConstPtr = std::shared_ptr<const GroupIndexMask>;
    DPIdList ids;
    const DPIdList& getGroupIds() const { return ids; }
};
struct GroupIndexMesh {
    std::map<DComponentType, std::shared_ptr<GroupIndexMask>> groups;
    std::size_t points = 4U, connections = 4U;
    int faces = 1, edges = 4;
    std::size_t numberOfPoints() const { return points; }
    int numFace() const { return faces; }
    std::size_t numberOfConnections() const { return connections; }
    bool isExistCompGrp(DComponentType type, const std::string&) const { return groups.count(type); }
    GroupIndexMask::ConstPtr getCompGrp(DComponentType type, const std::string&) const {
        return groups.at(type);
    }
};
struct GroupIndexData {
    std::vector<std::shared_ptr<GroupIndexMesh>> meshes;
    std::size_t numberOfMesh() const { return meshes.size(); }
    auto beginPrims() const -> decltype(meshes.begin()) { return meshes.begin(); }
    auto endPrims() const -> decltype(meshes.end()) { return meshes.end(); }
};
struct GroupIndexString : ArrayString {
    GroupIndexString(const ArrayString& value) : ArrayString(value) {}
    std::size_t length() const { return std::strlen(asChar()); }
};
#undef MString
#define MString GroupIndexString
struct GroupIndexBlock : MDataBlock {
    using MDataBlock::setClean;
    MStatus setClean(const MObject& attribute) { return MDataBlock::setClean(MPlug(attribute)); }
};
#define MDataBlock GroupIndexBlock
#define DMeshData GroupIndexData
#define DPGroupData GroupIndexMask
static GroupIndexData groupInput;
static bool invalidInput = false, nullNative = false, nullArray = false;
static const GroupIndexMesh* nativeSource = nullptr;
inline MStatus getValidInputDM(const MObject&, MDataBlock&, const GroupIndexData*& result) {
    const MStatus status = api("mesh.input");
    if (!status) return status;
    if (invalidInput) return MS::kInvalidParameter;
    result = &groupInput;
    return MS::kSuccess;
}
inline MStatus generateMMeshFromDmesh(const GroupIndexMesh& mesh, MObject& object) {
    const MStatus status = api("mesh.convert");
    if (status) {
        nativeSource = &mesh;
        object = nullNative ? MObject::kNullObj : MObject{"mesh"};
    }
    return status;
}
struct GroupIndexFnMesh {
    GroupIndexFnMesh(const MObject&, MStatus* status) { *status = api("mesh.bind"); }
    int numEdges(MStatus* status) {
        *status = api("mesh.edges");
        return nativeSource->edges;
    }
};
#define MFnMesh GroupIndexFnMesh
template <> inline const char* arrayName<int>() { return "index"; }
using MIntArray = Array<int>;
static std::vector<int> outputIndices;
struct MFnIntArrayData {
    MObject create(const MIntArray& indices, MStatus* status) {
        *status = api("index.create");
        if (*status) outputIndices = indices.values;
        return *status && !nullArray ? MObject{"indices"} : MObject::kNullObj;
    }
};
struct DMGroupToIndex : AttributeNode {
    static MObject inDM, aGroupName, outDataArray;
    MStatus compute(const MPlug&, MDataBlock&);
    ArrayString name() const { return {"DMGroupToIndex"}; }
};
MObject DMGroupToIndex::inDM{"inDM"};
MObject DMGroupToIndex::aGroupName{"GroupName"};
MObject DMGroupToIndex::outDataArray{"outDataArray"};
