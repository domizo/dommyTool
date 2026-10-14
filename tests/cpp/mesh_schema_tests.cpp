#include "mesh_metadata_fixture.h"

// Same lexical ordering as std::less<string>, instrumented only in extracted
// production indexes. This is an algorithmic guard, not a native speed test.
static std::size_t schemaComparisons = 0U;
struct CountingLess {
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        ++schemaComparisons; return lhs < rhs;
    }
};
#include "mesh_schema.inc"

static void stableOrderAndTypes() {
    auto data = twoMeshes();
    data.meshItemArray[0]->pointAttrs.clear(); data.meshItemArray[1]->pointAttrs.clear();
    data.meshItemArray[0]->pointAttrs["a"] = attr<int>({1, 2, 3});
    data.meshItemArray[0]->pointAttrs["z"] = attr<int>({1, 2, 3});
    data.meshItemArray[1]->pointAttrs["b"] = attr<int>({4, 5, 6});
    data.meshItemArray[1]->pointAttrs["z"] = attr<float>({4, 5, 6});
    assert(data.getPointAttributeList() == DStringList({"a", "z", "b"}));
    for (auto& mesh : data.meshItemArray) mesh->groups.clear();
    data.meshItemArray[0]->groups["a"] = std::make_shared<DPGroupData>(DPIdList{true, false, false});
    data.meshItemArray[0]->groups["z"] = std::make_shared<DPGroupData>(DPIdList{true, false, false});
    data.meshItemArray[1]->groups["b"] = std::make_shared<DPGroupData>(DPIdList{true, false, false});
    data.meshItemArray[1]->groups["z"] = std::make_shared<DPGroupData>(DPIdList{true, false, false});
    DStringList expectedGroups;
    for (const auto& mesh : data.meshItemArray)
        IntersectDStringList(expectedGroups, mesh->getAllCompGroupName(D_COMP_POINT));
    assert(data.getAllCompGroupName(D_COMP_POINT) == expectedGroups);
    auto target = fixture(7); target.meshItemArray[0]->pointAttrs.clear();
    target.meshItemArray[0]->pointAttrs["z"] = attr<int>({7, 8, 9});
    target.meshItemArray[0]->groups["z"] = std::make_shared<DPGroupData>(DPIdList{false, true, false});
    reset(); assert(data.mergeDMeshData(target));
    assert(data.meshItemArray[0]->pointAttrs.size() == 1 && data.meshItemArray[1]->pointAttrs.size() == 1);
    assert(data.meshItemArray[0]->pointAttrs.at("z")->getAttrType() == DP_ATTR_INT);
    assert(data.meshItemArray[1]->pointAttrs.at("z")->getAttrType() == DP_ATTR_FLOAT);
    assert(data.meshItemArray[2]->pointAttrs.at("z")->getAttrType() == DP_ATTR_INT);
    assert(data.meshItemArray[0]->groups.size() == 1 && data.meshItemArray[1]->groups.size() == 1);
    auto invalid = fixture(); invalid.meshItemArray.push_back({});
    assert(invalid.getPointAttributeList().empty());
    assert(invalid.getAllCompGroupName(D_COMP_POINT) == DStringList({"g"}));
}
static void indexGrowth() {
    for (int fields : {100, 200, 1000}) {
        auto left = fixture(), right = fixture(4);
        left.meshItemArray[0]->pointAttrs.clear(); right.meshItemArray[0]->pointAttrs.clear();
        for (int i = 0; i < fields; ++i) {
            const auto name = "field_" + std::to_string(i);
            left.meshItemArray[0]->pointAttrs[name] = attr<int>({1, 2, 3});
            right.meshItemArray[0]->pointAttrs[name] = attr<int>({4, 5, 6});
        }
        reset(); schemaComparisons = linearSearchCalls = 0;
        assert(left.copyMergedFrom(left, right));
        assert(meshCopies == 2 && boundsCalls == 1 && left.meshItemArray[0]->pointAttrs.size() == std::size_t(fields));
        std::cout << "Indexed schema " << fields << ": " << schemaComparisons << " lexical comparisons\n";
        const auto logarithmicBound = std::size_t(16 * fields * (1 + std::ceil(std::log2(fields))));
        assert(linearSearchCalls == 0 && schemaComparisons < logarithmicBound);
    }
}
int main() {
    stableOrderAndTypes(); indexGrowth();
    std::cout << "Mesh schemas retain first-observed order, sparse fields and first-present types\n";
}
