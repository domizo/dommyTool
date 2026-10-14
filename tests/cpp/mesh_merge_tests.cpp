#include "mesh_metadata_fixture.h"
#include "mesh_merge.inc"

static void normalMerge() {
    auto left = fixture(1), right = fixture(4);
    left.mAttributeList["left"] = attr<int>({8});
    right.mAttributeList["right"] = attr<int>({9});
    left.mGroupList["left"] = std::make_shared<DPGroupData>(DPIdList{false});
    right.mGroupList["right"] = std::make_shared<DPGroupData>(DPIdList{true});
    left.meshItemArray[0]->pointAttrs["a"] = attr<int>({1, 2, 3});
    right.meshItemArray[0]->pointAttrs["b"] = attr<int>({4, 5, 6});
    right.meshItemArray[0]->pointAttrs["p"] = attr<float>({4, 5, 6});
    left.meshItemArray[0]->groups["left"] = std::make_shared<DPGroupData>(DPIdList{true, false, false});
    right.meshItemArray[0]->faceGroups["right"] = std::make_shared<DPGroupData>(DPIdList{true});
    const auto oldLeft = left, oldRight = right;
    reset(); assert(left.mergeDMeshData(right));
    assert(meshCopies == 2 && boundsCalls == 1);
    assert(left.meshItemArray.size() == 2U && left.mBBox == 2U && !left.mBBoxDirty);
    assert(left.mAttributeList.size() == 1U && left.mAttributeList.count("v"));
    auto values = std::dynamic_pointer_cast<DPIntAttr>(left.mAttributeList.at("v"));
    assert(values->value() == std::vector<int>({1, 4}) && values->defaultValue() == -1);
    assert(left.mGroupList.size() == 1U && left.mGroupList.at("g")->getGroupIds() == DPIdList({true, true}));
    for (std::size_t i = 0; i < 2U; ++i) {
        const auto& mesh = left.meshItemArray[i];
        const auto& source = i == 0 ? oldLeft.meshItemArray[0] : oldRight.meshItemArray[0];
        assert(mesh != source && mesh->geometry == source->geometry && mesh->transform == source->transform);
        assert(mesh->pointAttrs.empty() && mesh->groups.size() == 1U && mesh->faceGroups.size() == 1U);
        assert(mesh->groups.at("g") != source->groups.at("g") &&
               mesh->groups.at("g")->getGroupIds() == source->groups.at("g")->getGroupIds());
        assert(mesh->edgeGroups.size() == 1U && mesh->uvs && mesh->uvIds == source->uvIds);
        assert(mesh->cornerAttrs.at("N") != source->cornerAttrs.at("N"));
        const auto& normals = std::dynamic_pointer_cast<DPVec3fAttr>(mesh->cornerAttrs.at("N"))->value();
        const auto& sourceNormals = std::dynamic_pointer_cast<DPVec3fAttr>(source->cornerAttrs.at("N"))->value();
        assert(normals.size() == sourceNormals.size());
        for (std::size_t row = 0; row < normals.size(); ++row)
            for (int axis = 0; axis < 3; ++axis) assert(normals[row][axis] == sourceNormals[row][axis]);
        assert(mesh->faceAttrs.at("f") != source->faceAttrs.at("f"));
    }
    unchanged(right, oldRight);
    assert(oldLeft.meshItemArray[0]->pointAttrs.size() == 2U && oldLeft.meshItemArray[0]->groups.size() == 2U);
    auto base = fixture(1), target = fixture(4);
    target.meshItemArray[0]->clearUVs(); reset(); assert(base.mergeDMeshData(target));
    assert(!base.meshItemArray[0]->hasUV() && !base.meshItemArray[1]->hasUV() && boundsCalls == 1);
}
static void admission() {
    auto left = fixture(), right = fixture(4);
    left.mAttributeList["v"] = attr<std::string>({std::string(64U, 'a')});
    right.mAttributeList["v"] = attr<std::string>({std::string(64U, 'b')});
    const auto old = left; assert(left.hasValidMeshTopology() && right.hasValidMeshTopology());
    reset(); assert(!left.mergeDMeshData(right)); unchanged(left, old);
    assert(meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    right.mAttributeList["v"] = attr<std::string>({std::string(50U, 'b')}, std::string(64U, 'd'));
    // Exact aggregate boundary: component names + primitive name/mask + rows.
    // The discarded right default is not part of the collection payload.
    reset(); assert(left.mergeDMeshData(right)); assert(meshCopies == 2 && boundsCalls == 1);
    auto mismatch = fixture(); auto incompatible = fixture(4);
    incompatible.mAttributeList["v"] = attr<float>({4});
    const auto before = mismatch; reset(); assert(!mismatch.mergeDMeshData(incompatible));
    unchanged(mismatch, before); assert(meshCopies == 0 && attributeCopies == 0);
    auto nested = fixture(), nestedRight = fixture(4);
    nested.mAttributeList["v"] = attr<DIntArray>({DIntArray(255U, 1)});
    nestedRight.mAttributeList["v"] = attr<DIntArray>({DIntArray(255U, 2)});
    reset(); assert(!nested.mergeDMeshData(nestedRight)); assert(meshCopies == 0 && attributeCopies == 0);
    auto fields = fixture(), otherFields = fixture(4);
    fields.mAttributeList.clear(); otherFields.mAttributeList.clear();
    for (const char* name : {"a", "b"}) {
        fields.mAttributeList[name] = attr<std::string>({std::string(32U, 'a')});
        otherFields.mAttributeList[name] = attr<std::string>({std::string(32U, 'b')});
    }
    reset(); assert(!fields.mergeDMeshData(otherFields)); assert(meshCopies == 0 && attributeCopies == 0);
    auto masks = fixture(), otherMasks = fixture(4);
    masks.mAttributeList.clear(); otherMasks.mAttributeList.clear();
    masks.mGroupList.clear(); otherMasks.mGroupList.clear();
    for (int i = 0; i < 64; ++i) {
        const auto name = std::to_string(i);
        masks.mGroupList[name] = std::make_shared<DPGroupData>(DPIdList{true});
        otherMasks.mGroupList[name] = std::make_shared<DPGroupData>(DPIdList{false});
    }
    assert(masks.hasValidMeshTopology() && otherMasks.hasValidMeshTopology());
    reset(); assert(!masks.mergeDMeshData(otherMasks)); assert(meshCopies == 0 && attributeCopies == 0);
    masks.mGroupList.erase("63"); otherMasks.mGroupList.erase("63");
    masks.mGroupList.erase("62"); otherMasks.mGroupList.erase("62");
    reset(); assert(masks.mergeDMeshData(otherMasks)); assert(meshCopies == 2 && boundsCalls == 1);
    auto invalid = fixture(); invalid.meshItemArray[0]->valid = false;
    reset(); assert(!mismatch.mergeDMeshData(invalid)); assert(meshCopies == 0);
}
static void aggregateComponents() {
    auto ordinary = fixture(); DMeshData copied;
    reset(); assert(copied.copyFrom(ordinary)); assert(meshCopies == 1 && boundsCalls == 1);
    assert(copied.meshItemArray[0] != ordinary.meshItemArray[0]);
    reset(); assert(copied.appendMeshes({ordinary.meshItemArray[0].get()}));
    assert(meshCopies == 1 && boundsCalls == 1 && copied.meshItemArray.size() == 2U);
    auto left = fixture(), right = fixture(4);
    for (auto* source : {&left, &right}) {
        source->meshItemArray[0]->pointAttrs.clear();
        source->meshItemArray[0]->pointAttrs["s"] =
            attr<std::string>({std::string(40U, 'a'), std::string(40U, 'b'), std::string(40U, 'c')});
        assert(source->hasValidMeshTopology());
    }
    auto destination = fixture(7); const auto old = destination;
    reset(); assert(!destination.copyMergedFrom(left, right)); unchanged(destination, old);
    assert(meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    DMeshData aggregate;
    aggregate.meshItemArray = {left.meshItemArray[0], right.meshItemArray[0]};
    assert(aggregate.hasValidMeshTopology());
    reset(); assert(!destination.copyFrom(aggregate)); unchanged(destination, old);
    assert(meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    const auto beforeAppend = left;
    reset(); assert(!left.appendMeshes({right.meshItemArray[0].get()})); unchanged(left, beforeAppend);
    assert(meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    auto defaults = fixture(), incoming = fixture(4);
    defaults.mAttributeList["v"] = attr<std::string>({std::string(35U, 'a')}, std::string(40U, 'd'));
    const auto defaultBefore = defaults;
    reset(); assert(!defaults.appendMeshes({incoming.meshItemArray[0].get()}));
    unchanged(defaults, defaultBefore); assert(meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    const DPIndexList indices{0U}; reset();
    assert(defaults.appendMeshes({incoming.meshItemArray[0].get()}, &indices));
    assert(meshCopies == 1 && boundsCalls == 1);
    assert(std::dynamic_pointer_cast<DPStringAttr>(defaults.mAttributeList.at("v"))->value() ==
        std::vector<std::string>({std::string(35U, 'a'), std::string(35U, 'a')}));
    assert(defaults.mGroupList.at("g")->getGroupIds() == DPIdList({true, true}));
    // A valid self-assignment allocates nothing, even for a collection above
    // the operation budget. No new payload is produced in that identity case.
    reset(); assert(aggregate.copyFrom(aggregate)); assert(meshCopies == 0 && boundsCalls == 0);
    inputData = left; targetData = right; publishedData = old;
    reset(); MDataBlock data;
    assert(!DMMerge().compute(MPlug{DMMerge::outDM}, data));
    assert(publications == 0 && cleanCalls == 0 && meshCopies == 0);
    unchanged(publishedData, old);
}
static void emptyAndAliases() {
    auto full = fixture(3); DMeshData empty;
    reset(); assert(empty.mergeDMeshData(full)); assert(meshCopies == 1 && boundsCalls == 1);
    assert(empty.hasUVs() && empty.mAttributeList.count("v") && empty.mGroupList.count("g"));
    assert(empty.meshItemArray[0]->pointAttrs.count("p") && empty.meshItemArray[0]->groups.count("g"));
    assert(empty.meshItemArray[0] != full.meshItemArray[0]);
    DMeshData nothing; const auto old = full; reset(); assert(full.mergeDMeshData(nothing));
    unchanged(full, old); assert(meshCopies == 0 && attributeCopies == 0 && boundsCalls == 0);
    DMeshData bothEmpty; reset(); assert(bothEmpty.mergeDMeshData(nothing)); assert(meshCopies == 0);
    auto self = fixture(2); const auto original = self.meshItemArray[0];
    reset(); assert(self.mergeDMeshData(self)); assert(meshCopies == 2 && boundsCalls == 1);
    assert(self.meshItemArray.size() == 2U && self.meshItemArray[0] != original && self.meshItemArray[1] != original);
    assert(self.meshItemArray[0] != self.meshItemArray[1]);
    // Keep the legacy collection-name union and first-present type rule even
    // when a shared field/group is absent from some primitives.
    auto sparse = fixture(1), sparseTarget = fixture(4);
    auto second = fixture(7).meshItemArray[0];
    second->pointAttrs.clear(); second->groups.clear();
    sparse.meshItemArray.push_back(second);
    sparse.mAttributeList["v"] = attr<int>({1, 7});
    sparse.mGroupList["g"] = std::make_shared<DPGroupData>(DPIdList{true, false});
    reset(); assert(sparse.mergeDMeshData(sparseTarget));
    assert(meshCopies == 3 && boundsCalls == 1 && sparse.meshItemArray.size() == 3U);
    assert(sparse.meshItemArray[0]->pointAttrs.count("p") && sparse.meshItemArray[1]->pointAttrs.empty());
    assert(sparse.meshItemArray[2]->pointAttrs.count("p") && sparse.meshItemArray[1]->groups.empty());
}
static void failures() {
    for (int mode : {1, 2, 3}) for (int index : {0, 1}) {
        auto left = fixture(), right = fixture(4); const auto old = left;
        reset(); copyFailure = mode; failCopyAt = index;
        assert(!left.mergeDMeshData(right)); unchanged(left, old);
    }
    for (bool bounds : {false, true}) {
        auto left = fixture(), right = fixture(4); const auto old = left;
        reset(); failBounds = bounds; failMetadata = !bounds;
        assert(!left.mergeDMeshData(right)); unchanged(left, old);
    }
    for (int mode : {1, 2, 3}) for (int index = 1; index <= 7; ++index) {
        for (bool group : {false, true}) {
            auto left = fixture(), right = fixture(4); const auto old = left;
            reset(); metadataFailure = mode;
            if (group) failGroupAt = index; else failAttributeAt = index;
            assert(!left.mergeDMeshData(right)); unchanged(left, old);
            assert(right.hasValidMeshTopology());
        }
    }
    reset();
}
static MStatus nodeCompute() {
    MDataBlock data; return DMMerge().compute(MPlug{DMMerge::outDM}, data);
}
static void nodeAndAssignment() {
    reset(); MDataBlock block;
    assert(DMMerge().compute(MPlug{MObject{"unknown"}}, block) == MS::kUnknownParameter && hostCalls == 0);
    inputData = fixture(1); targetData = fixture(4); publishedData = fixture(7);
    reset(); assert(nodeCompute());
    assert(meshCopies == 2 && boundsCalls == 1 && creations == 1 && publications == 1 && cleanCalls == 1);
    const int calls = hostCalls;
    const auto saved = publishedData;
    for (int call = 0; call < calls; ++call) for (bool throws : {false, true}) {
        reset(); publishedData = saved;
        if (throws) hostThrowAt = call; else hostFailAt = call;
        assert(!nodeCompute()); assert(cleanCalls == 0);
        // A failed setClean is after publication; earlier failures retain cache.
        if (call < calls - 1) {assert(publications == 0); unchanged(publishedData, saved);}
    }
    reset(); targetData.meshItemArray[0]->valid = false;
    assert(!nodeCompute() && publications == 0 && cleanCalls == 0 && meshCopies == 0);
    targetData = fixture(4);
    auto result = fixture(7); const auto old = result;
    reset(); assert(result.copyMergedFrom(inputData, targetData));
    assert(meshCopies == 2 && boundsCalls == 1 && result.meshItemArray.size() == 2U);
    const auto priorResult = result;
    reset(); failBounds = true; assert(!result.copyMergedFrom(inputData, targetData));
    unchanged(result, priorResult);
    // The destination may be either input, including the target input.
    reset(); assert(targetData.copyMergedFrom(inputData, targetData));
    assert(targetData.meshItemArray.size() == 2U && meshCopies == 2 && boundsCalls == 1);
    DMeshData empty;
    result = old; reset(); assert(result.copyMergedFrom(empty, inputData));
    assert(meshCopies == 1 && boundsCalls == 1 && result.meshItemArray[0]->pointAttrs.count("p"));
    result = old; reset(); assert(result.copyMergedFrom(inputData, empty));
    assert(meshCopies == 1 && boundsCalls == 1 && result.hasUVs());
    reset();
}
int main() {
    aggregateComponents(); admission(); normalMerge(); emptyAndAliases(); failures(); nodeAndAssignment();
    std::cout << "Complete mesh merge: typed attributes, groups, UVs/normals, budgets, ownership and rollback passed\n";
}
