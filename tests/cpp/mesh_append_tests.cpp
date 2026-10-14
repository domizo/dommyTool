// Extracted production transactions; scalar geometry counts avoid huge buffers.
// Metadata here models ownership/row semantics; real typed budgets are tested
// in mesh_merge_tests.cpp. This is not Maya geometry evaluation.
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DWireTopologyPlan.h"
#include <cassert>
#include <maya/MStatus.h>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

static std::size_t meshChecks = 0, meshCopies = 0, meshCtorCopies = 0;
static int fault = 0;
struct MGlobal { static void displayError(const char*) {} };
struct Metadata {
    std::vector<int> ids;
    int defaultValue = 7;
    void incrementDefaultValue(std::size_t n) { if (fault != 3) ids.insert(ids.end(), n, defaultValue); }
    void incrementIndexElement(std::size_t i) { const int v = ids.at(i); if (fault != 3) ids.push_back(v); }
    void expandOtherGrpFromIndex(std::size_t i) { incrementIndexElement(i); }
    const std::vector<int>& getGroupIds() const { return ids; }
};
using DPrimAttrList = std::map<std::string,std::shared_ptr<Metadata>>;
using DPrimGrpList = DPrimAttrList;
using ConstDPrimGrpListIter = DPrimGrpList::const_iterator;
using DPIndexList = std::vector<std::size_t>;
void CopyAttributeToAttribute(DPrimAttrList& dst,const DPrimAttrList& src) {
    if (fault == 2) throw std::bad_alloc();
    for (const auto& entry : src) dst[entry.first] = std::make_shared<Metadata>(*entry.second);
}
void CopyGroupToGroup(DPrimGrpList& dst,const DPrimGrpList& src) { CopyAttributeToAttribute(dst,src); }
void ExpandGroupListSize(DPrimGrpList& groups,std::size_t n) {
    for (auto& entry : groups) if (fault != 3) entry.second->ids.insert(entry.second->ids.end(),n,0);
}
namespace dommy { namespace attribute {
struct ValidationBudget { std::size_t workItems, stringBytes;
    explicit ValidationBudget(std::size_t n):workItems(n),stringBytes(0U){} };
bool validateAttributeList(const DPrimAttrList& attrs,std::size_t n,ValidationBudget& b) {
    for (const auto& entry : attrs)
        if (!entry.second || entry.second->ids.size() != n ||
            !generation::checkedSum(b.workItems,n,generation::kMaxWorkItemCount,b.workItems)) return false;
    return true;
}
bool validateAttributeExpansion(const DPrimAttrList& attrs,std::size_t n,std::size_t added,
                                const DPIndexList* indices,ValidationBudget& b) {
    if (!validateAttributeList(attrs,n,b) || (indices && indices->size()!=added)) return false;
    if (indices) for (auto i : *indices) if (i >= n) return false;
    std::size_t rows = 0;
    return generation::checkedProduct(attrs.size(),added,generation::kMaxWorkItemCount,rows) &&
        generation::checkedSum(b.workItems,rows,generation::kMaxWorkItemCount,b.workItems);
}
} }
// Metadata admission is a boundary double in this count-only suite. The
// complete shared planners and all three transactions use real typed metadata
// in mesh_merge_tests.cpp; do not infer payload-budget coverage from this stub.
namespace dommy { namespace mesh {
template <typename Iterator>
bool addMeshMetadataCost(Iterator, Iterator, attribute::ValidationBudget&) {return true;}
template <typename Owner>
bool validateRepeatedMeshMetadata(const Owner&, std::size_t, const MeshMergePlan&) {return true;}
} }
struct Connections { std::size_t count; std::size_t length() const { return count; } };
struct Mesh {
    std::size_t points, uvs = 0;
    int faces;
    Connections connections;
    bool valid = true;
    explicit Mesh(std::size_t p=3,int f=1):points(p),faces(f),connections{std::size_t(f)*3} {}
    Mesh(const Mesh& other):points(other.points),uvs(other.uvs),faces(other.faces),
        connections(other.connections),valid(other.valid) { ++meshCtorCopies; }
    bool hasValidMeshData() const {
        ++meshChecks;
        return valid && points>=3 && points<=dommy::generation::kMaxGeneratedPointCount &&
            faces>0 && std::size_t(faces)<=dommy::generation::kMaxGeneratedFaceCount &&
            connections.count<=dommy::generation::kMaxWorkItemCount;
    }
    std::size_t numberOfPoints() const { return points; }
    int numFace() const { return faces; }
    // Count-only paths must not request a by-value native topology array.
    const Connections& getConnectList() const = delete;
    std::size_t numberOfConnections() const { return connections.count; }
    std::size_t numberOfUVs() const { return uvs; }
    std::shared_ptr<Mesh> copyNewMesh() const {
        ++meshCopies;
        if (fault == 1) throw std::bad_alloc();
        auto result = std::make_shared<Mesh>(*this);
        if (fault == 5) result->valid = false;
        if (fault == 6) return {};
        return result;
    }
};
using DMeshInternalData = Mesh;
using DMItem = std::shared_ptr<Mesh>;
using ConstDMItem = std::shared_ptr<const Mesh>;
using DMeshList = std::vector<DMItem>;
using ConstDMeshList = std::vector<ConstDMItem>;
struct DMeshData {
    using MeshSourceList = std::vector<const DMeshInternalData*>;
    DMeshList meshItemArray;
    DPrimAttrList mAttributeList;
    DPrimGrpList mGroupList;
    std::size_t mBBox = 99;
    bool mBBoxDirty = true;
    bool getCheckedTotalVertex(std::size_t&) const;
    bool getCheckedTotalFace(std::size_t&) const;
    bool getCheckedTotalConnect(std::size_t&) const;
    bool hasValidMeshTopology() const;
    bool appendMeshes(const MeshSourceList&,const DPIndexList* = nullptr);
    MStatus copyFrom(const DMeshData&);
    bool insert(const DMeshInternalData&);
    bool insert(const DMItem&);
    bool insert(const ConstDMItem&);
    bool insert(const DMeshList&);
    bool expand(const DMItem&,std::size_t);
    bool expand(const DMeshList&,const DPIndexList&);
    bool mergeStorageAdmission(const DMeshData&) const;
    std::size_t numberOfMesh() const { return meshItemArray.size(); }
    DMeshList::const_iterator beginMeshes() const { return meshItemArray.begin(); }
    DMeshList::const_iterator endMeshes() const { return meshItemArray.end(); }
    void updateBBox() { if (fault == 4) throw std::bad_alloc(); mBBox = meshItemArray.size(); }
};
#include "mesh_append.inc"

DMeshData fixture(std::size_t points = 3, int faces = 1) {
    fault = 0; meshChecks = meshCopies = meshCtorCopies = 0;
    DMeshData result;
    result.meshItemArray.push_back(std::make_shared<Mesh>(points,faces));
    auto attr = std::make_shared<Metadata>(); attr->ids = {42};
    auto group = std::make_shared<Metadata>(); group->ids = {1};
    result.mAttributeList["value"] = attr; result.mGroupList["selected"] = group;
    return result;
}
void unchanged(const DMeshData& value, const DMItem& item) {
    assert(value.meshItemArray.size()==1 && value.meshItemArray[0]==item);
    assert(value.mAttributeList.at("value")->ids==std::vector<int>({42}));
    assert(value.mGroupList.at("selected")->ids==std::vector<int>({1}));
    assert(value.mBBox==99 && value.mBBoxDirty);
}
std::size_t batch(std::size_t n) {
    auto value = fixture(); const auto original = value.meshItemArray[0];
    DMeshList incoming(n,original); DPIndexList indices(n,0);
    assert(value.expand(incoming,indices));
    const auto checks = meshChecks;
    assert(value.hasValidMeshTopology() && value.meshItemArray.size()==n+1 && meshCopies==n);
    assert(value.mAttributeList.at("value")->ids==std::vector<int>(n+1,42));
    assert(value.mGroupList.at("selected")->ids==std::vector<int>(n+1,1));
    for (std::size_t i=1;i<=n;++i) assert(value.meshItemArray[i]!=original);
    return checks;
}
void storageAdmission() {
    using namespace dommy::mesh;
    // Scalar counts exercise the real transaction without oversized buffers.
    for (int operation = 0; operation < 3; ++operation) {
        auto value = fixture(1100000U);
        const auto original = value.meshItemArray[0];
        MeshMergePlan plan;
        assert(appendMeshMergePlan(1100000U, 1U, 3U, 0U, plan));
        assert(validMeshOperationStorage(plan));
        assert(appendMeshMergePlan(1100000U, 1U, 3U, 0U, plan));
        assert(!validMeshOperationStorage(plan));
        const bool accepted = operation == 0 ? value.appendMeshes(DMeshData::MeshSourceList{original.get()}) :
            operation == 1 ? value.expand(original, 0U) : value.insert(DMeshList{original});
        assert(!accepted && meshCopies == 0U);
        unchanged(value, original);
    }
    DMeshData empty;
    auto excessive = std::make_shared<Mesh>(2200000U);
    meshCopies = 0U;
    assert(!empty.insert(DMeshList{excessive}) && meshCopies == 0U && empty.meshItemArray.empty());

    // Exact 256 MiB admission and a single-point excess retain failure atomicity.
    auto boundary = fixture(1048574U);
    const auto original = boundary.meshItemArray[0];
    MeshMergePlan plan;
    assert(appendMeshMergePlan(1048574U, 1U, 3U, 0U, plan));
    assert(appendMeshMergePlan(1048574U, 1U, 3U, 0U, plan));
    std::size_t bytes = 0U;
    assert(meshOperationStorageBytes(plan, bytes) && bytes == kMaxMeshOperationBytes);
    auto tooLarge = std::make_shared<Mesh>(1048575U);
    assert(!boundary.insert(DMeshList{tooLarge}) && meshCopies == 0U);
    unchanged(boundary, original);
    assert(boundary.insert(DMeshList{original}) && meshCopies == 1U);
    assert(boundary.meshItemArray.size() == 2U && boundary.meshItemArray[1] != original);
    std::cout << "Mesh append rejects byte excess before copies; exact capacity and retry pass\n";
}

void copyAdmission() {
    auto excessive = fixture(2200000U);
    auto target = fixture(); const auto original = target.meshItemArray.front();
    assert(excessive.hasValidMeshTopology());
    assert(target.copyFrom(excessive) == MS::kInvalidParameter && meshCopies == 0U);
    unchanged(target, original);
    assert(excessive.copyFrom(excessive) == MS::kSuccess && meshCopies == 0U);
    auto invalid = fixture(); invalid.meshItemArray.front()->valid = false;
    assert(target.copyFrom(invalid) == MS::kInvalidParameter && meshCopies == 0U);
    unchanged(target, original);
    auto valid = fixture();
    for (int failure : {1,2,4,5,6}) {
        fault = failure;
        assert(target.copyFrom(valid) == MS::kFailure);
        unchanged(target, original);
    }
    fault = 0; meshCopies = meshCtorCopies = 0U;
    assert(target.copyFrom(valid) == MS::kSuccess && meshCopies == 1U && meshCtorCopies == 1U);
    assert(target.meshItemArray.front() != valid.meshItemArray.front());
    assert(target.mAttributeList.at("value") != valid.mAttributeList.at("value"));
    assert(target.mGroupList.at("selected") != valid.mGroupList.at("selected"));
    assert(target.mBBox == 1U && !target.mBBoxDirty);
    valid.meshItemArray.front()->valid = false;
    assert(target.hasValidMeshTopology());
    DMeshData empty;
    assert(target.copyFrom(empty) == MS::kSuccess && target.meshItemArray.empty());
    assert(target.mAttributeList.empty() && target.mGroupList.empty() && target.mBBox == 0U);

    auto boundary = fixture(2097150U);
    assert(target.copyFrom(boundary) == MS::kSuccess && meshCopies == 1U);
    auto excess = fixture(2097151U);
    const auto retained = target.meshItemArray.front();
    assert(target.copyFrom(excess) == MS::kInvalidParameter && meshCopies == 0U);
    assert(target.meshItemArray.front() == retained && target.mBBox == 1U && !target.mBBoxDirty);
    std::cout << "Mesh copy: capacity, self/empty/invalid input, atomic failures and independent ownership pass\n";
}

void referenceInsertion() {
    auto value = fixture(); const auto original = value.meshItemArray.front();
    Mesh excessive(2200000U);
    assert(!value.insert(excessive) && meshCtorCopies == 0U && meshCopies == 0U);
    unchanged(value, original);
    Mesh small;
    for (int failure = 1; failure <= 6; ++failure) {
        fault = failure;
        assert(!value.insert(small)); unchanged(value, original);
    }
    fault = 0; meshCopies = meshCtorCopies = 0U;
    assert(value.insert(*original) && meshCopies == 1U && meshCtorCopies == 1U);
    assert(value.meshItemArray.back().get() != original.get());
    assert(value.mAttributeList.at("value")->ids == std::vector<int>({42,7}));
    DMeshData owned;
    {
        Mesh local;
        meshCopies = meshCtorCopies = 0U;
        assert(owned.insert(local) && meshCopies == 1U && meshCtorCopies == 1U);
        local.valid = false;
    }
    assert(owned.hasValidMeshTopology());
    assert(!owned.insert(DMItem()) && !owned.insert(ConstDMItem()));
    auto pointer = std::make_shared<Mesh>();
    assert(owned.insert(pointer));
    ConstDMItem constPointer = pointer;
    assert(owned.insert(constPointer));
    pointer->valid = false;
    assert(owned.hasValidMeshTopology());
    std::cout << "Reference/pointer insert: no eager copy, one owned clone, scoped input lifetime and retry pass\n";
}

void rangeAndNodeAdmission() {
    using namespace dommy::mesh;
    auto left = fixture(1100000U), right = fixture(1100000U);
    assert(copyTransformAdmission(&left, 1U) == MS::kSuccess);
    assert(copyTransformAdmission(&left, 0U) == MS::kSuccess);
    assert(copyTransformAdmission(&left, 2U) == MS::kInvalidParameter);
    assert(mergeNodeAdmission(&left, &right) == MS::kInvalidParameter);
    assert(!left.mergeStorageAdmission(right) && meshCopies == 0U);
    auto small = fixture();
    assert(copyTransformAdmission(&small, 100U) == MS::kSuccess);
    assert(mergeNodeAdmission(&left, &small) == MS::kSuccess);
    assert(left.mergeStorageAdmission(small));
    MeshMergePlan plan;
    assert(appendMeshRangePlan(small.beginMeshes(), small.endMeshes(), plan));
    const auto retained = plan;
    DMeshData::MeshSourceList bad{small.meshItemArray.front().get(), nullptr};
    assert(!appendMeshRangePlan(bad.begin(), bad.end(), plan));
    assert(plan.meshes == retained.meshes && plan.points == retained.points && plan.workItems == retained.workItems);
    Mesh huge(dommy::generation::kMaxGeneratedPointCount + 1U);
    bad = {small.meshItemArray.front().get(), &huge};
    assert(!appendMeshRangePlan(bad.begin(), bad.end(), plan));
    assert(plan.meshes == retained.meshes && plan.points == retained.points && plan.workItems == retained.workItems);
    DMeshData empty;
    assert(copyTransformAdmission(&empty, 0U) == MS::kSuccess);
    assert(mergeNodeAdmission(&empty, &small) == MS::kSuccess);
    std::cout << "Copy/merge node count-only admission and failure-atomic range accumulation pass\n";
}
int main() {
    copyAdmission();
    referenceInsertion();
    rangeAndNodeAdmission();
    storageAdmission();
    for (int mode=0;mode<4;++mode) {
        auto value=fixture(mode==0?6000000:3,mode==1?1100000:1);
        const auto original=value.meshItemArray[0];
        if (mode==2) original->connections.count=30000000;
        if (mode==3) original->uvs=30000000;
        assert(!value.expand(original,0)); unchanged(value,original); assert(meshCopies==0);
    }
    for (int mode=0;mode<3;++mode) {
        DMeshData value;
        auto primitive=std::make_shared<Mesh>(mode==0?6000000:3, mode==1?1100000:1);
        if(mode==2) primitive->connections.count=60000000;
        value.meshItemArray={primitive,primitive};
        assert(!value.hasValidMeshTopology());
    }
    for (int failure=1;failure<=6;++failure) {
        auto value=fixture(); const auto original=value.meshItemArray[0]; fault=failure;
        assert(!value.expand(original,0)); unchanged(value,original);
    }
    auto value=fixture(); const auto original=value.meshItemArray[0];
    assert(!value.expand(original,1)); assert(!value.expand(DMeshList{original},{}));
    assert(!value.expand(DMeshList{original},DPIndexList{std::size_t(-1)}));
    assert(!value.expand(DMItem(),0)); unchanged(value,original); assert(meshCopies==0);
    assert(value.expand(DMeshList{},DPIndexList{})); unchanged(value,original);
    assert(value.appendMeshes(DMeshData::MeshSourceList{original.get()}));
    assert(value.mAttributeList.at("value")->ids==std::vector<int>({42,7}));
    assert(value.mGroupList.at("selected")->ids==std::vector<int>({1,0}));
    auto oldAttribute=value.mAttributeList.at("value");
    assert(value.expand(DMeshList{original,original,original},DPIndexList{1,0,1}));
    assert(value.mAttributeList.at("value")->ids==std::vector<int>({42,7,7,42,7}));
    assert(value.mGroupList.at("selected")->ids==std::vector<int>({1,0,0,1,0}));
    assert(oldAttribute->ids==std::vector<int>({42,7}));
    assert(value.mBBox==5 && !value.mBBoxDirty);
    const auto small=batch(1000), large=batch(2000);
    assert(large <= small*2 && large < 11000);
    for (std::size_t n : {100U, 200U, 1000U}) {
        DMeshData output;
        DMeshList incoming(n, std::make_shared<Mesh>());
        fault = 0; meshChecks = meshCopies = 0;
        assert(output.insert(incoming));
        assert(meshChecks == 2U*n && meshCopies == n);
        const auto insertionChecks = meshChecks;
        meshChecks = 0;
        assert(output.hasValidMeshTopology() && meshChecks == n);
        std::cout << "Batch " << n << ": " << insertionChecks << " primitive validations\n";
        incoming.back()->valid = false;
        meshChecks = 0;
        assert(output.hasValidMeshTopology()); // Output owns independent primitive copies.
        output.meshItemArray.back()->valid = false;
        assert(!output.hasValidMeshTopology());
    }

    using namespace dommy::mesh;
    MeshMergePlan input, repeated;
    assert(appendMeshMergePlan(3,1,3,0,input));
    assert(repeatMeshMergePlan(input,100,repeated) && repeated.points==300 && repeated.meshes==100);
    assert(!repeatMeshMergePlan(input,1000001,repeated) && repeated.points==300);
    assert(!repeatMeshMergePlan(input,0,repeated) && repeated.points==300);
    input.points=6000000;
    assert(!repeatMeshMergePlan(input,2,repeated));
    input.points=3; input.workItems=60000000;
    assert(!repeatMeshMergePlan(input,2,repeated));

    WireTopologyPlan topology;
    assert(checkedWireTopologyPlan(2,3,0,0,1,1,100,100,1000,topology));
    assert(hasValidWireTopology(topology));
    assert(topology.faceVertexIndices==std::vector<int>({0,1,4,3,1,2,5,4,2,0,3,5}));
    assert(topology.uvIndices==std::vector<int>({0,1,5,4,1,2,6,5,2,3,7,6}));
    assert(topology.uCoordinates==std::vector<float>({0,1.0f/3,2.0f/3,1,0,1.0f/3,2.0f/3,1}));
    const auto indices=topology.faceVertexIndices;
    assert(!checkedWireTopologyPlan(2,10000000,0,0,1,1,10000000,2000000,100000000,topology));
    assert(topology.faceVertexIndices==indices);
    assert(!checkedWireTopologyPlan(2,3,1e300,0,1,1,100,100,1000,topology));
    assert(topology.faceVertexIndices==indices);
    assert(checkedWireTopologyCounts(1000,1000,10000000,2000000,100000000,topology));
    assert(topology.faceVertexIndices.empty() && !validWireGenerationBudget(topology,1));
    assert(!validWireGenerationBudget(topology,0));
    std::cout << "Mesh append transactions, metadata order, linear batch growth and scalar budgets passed\n";
}
