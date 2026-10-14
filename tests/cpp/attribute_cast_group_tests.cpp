#include "common/utility/DAttributeCast.h"
#include "common/utility/DGroupPublication.h"
#include "common/utility/DGroupEvaluation.h"

#include <cassert>
#include <climits>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

struct PointOwner {
    std::size_t count = 0U;
    DPAttrList attributes;
    DPGrpList groups;
    std::size_t numberOfPoints() const { return count; }
    DPAttrIter beginAttr() { return attributes.begin(); }
    DPAttrIter endAttr() { return attributes.end(); }
    DPGrpIter beginGrp() { return groups.begin(); }
    DPGrpIter endGrp() { return groups.end(); }
    DPGroupData::ConstPtr getGrp(const std::string& name) const {
        return dommy::attribute::findAttribute(groups, name);
    }
    void addGrp(const std::string& name, const DPGroupData& group) {
        groups.emplace(name, group.copy());
    }
    bool replaceGrp(const std::string& name, const DPIdList& values) {
        return dommy::group::replaceGroupValues(groups, name, values, numberOfPoints());
    }
};

template <typename Source, typename Scalar>
void testTypedCast(int target)
{
    using namespace dommy::attribute;
    using Components = dommy::numeric::ValueComponents<Source>;
    using Destination = typename CastValue<Source, Scalar>::Type;
    using OutputComponents = dommy::numeric::ValueComponents<Destination>;
    Source value, defaultValue;
    for (unsigned int axis = 0U; axis < Components::count; ++axis) {
        Components::get(value, axis) = static_cast<typename Components::Scalar>(axis + 1.75);
        Components::get(defaultValue, axis) = static_cast<typename Components::Scalar>(axis + 4.25);
    }
    const auto source = std::make_shared<TypedDPAttributeData<Source> >(
        std::vector<Source>{value}, defaultValue);
    DPAttributeData::Ptr output;
    assert(buildAttributeCast(source, 1U, target, output));
    const auto typed = std::dynamic_pointer_cast<TypedDPAttributeData<Destination> >(output);
    assert(typed && typed->value().size() == 1U);
    for (unsigned int axis = 0U; axis < Components::count; ++axis) {
        assert(OutputComponents::get(typed->value()[0], axis) ==
               static_cast<Scalar>(Components::get(value, axis)));
        assert(OutputComponents::get(typed->getDefaultValue(), axis) ==
               static_cast<Scalar>(Components::get(defaultValue, axis)));
        assert(Components::get(source->value()[0], axis) == Components::get(value, axis));
        assert(Components::get(source->getDefaultValue(), axis) == Components::get(defaultValue, axis));
    }
    const auto empty = std::make_shared<TypedDPAttributeData<Source> >(
        std::vector<Source>(), defaultValue);
    assert(buildAttributeCast(empty, 0U, target, output));
    assert(output->attrSize() == 0U && output->getAttrType() == typed->getAttrType());
}

template <typename Source> void testCastTargets()
{
    testTypedCast<Source, int32_t>(0);
    testTypedCast<Source, float>(1);
    testTypedCast<Source, double>(2);
}

void testAttributeCast()
{
    using namespace dommy::attribute;
    testCastTargets<int32_t>();
    testCastTargets<float>();
    testCastTargets<double>();
    testCastTargets<Imath::V2i>();
    testCastTargets<Imath::V2f>();
    testCastTargets<Imath::V2d>();
    testCastTargets<Imath::V3i>();
    testCastTargets<Imath::V3f>();
    testCastTargets<Imath::V3d>();

    PointOwner owner;
    owner.count = 4U;
    owner.attributes["number"] = std::make_shared<DPDoubleAttr>(
        std::vector<double>{double(INT_MAX), double(INT_MIN), 16777217.0, -1.75}, 16777217.0);
    const auto original = owner.attributes.at("number");
    owner.attributes["unrelated"] = std::make_shared<DPFloatAttr>(std::vector<float>(4U, 7.0f));
    const auto unrelated = owner.attributes.at("unrelated");
    assert(castPointAttribute(owner, "number", 0));
    const auto integers = std::dynamic_pointer_cast<DPIntAttr>(owner.attributes.at("number"));
    assert(integers && integers->value() == std::vector<int32_t>({INT_MAX, INT_MIN, 16777217, -1}));
    assert(integers->getDefaultValue() == 16777217);
    assert(original->getAttrType() == DP_ATTR_DOUBLE);
    assert(owner.attributes.at("unrelated") == unrelated);

    owner.count = 1U;
    owner.attributes["precise"] = std::make_shared<DPVec2iAttr>(
        std::vector<Imath::V2i>{Imath::V2i(16777217, INT_MAX)});
    assert(castPointAttribute(owner, "precise", 2));
    const auto vec2 = std::dynamic_pointer_cast<DPVec2dAttr>(owner.attributes.at("precise"));
    assert(vec2 && vec2->value()[0][0] == 16777217.0 && vec2->value()[0][1] == double(INT_MAX));
    owner.attributes["precise"] = std::make_shared<DPVec3iAttr>(
        std::vector<Imath::V3i>{Imath::V3i(16777217, INT_MAX, INT_MIN)});
    assert(castPointAttribute(owner, "precise", 2));
    const auto vec3 = std::dynamic_pointer_cast<DPVec3dAttr>(owner.attributes.at("precise"));
    assert(vec3 && vec3->value()[0][0] == 16777217.0 &&
        vec3->value()[0][1] == double(INT_MAX) && vec3->value()[0][2] == double(INT_MIN));

    const DPAttributeData::Ptr failures[] = {
        std::make_shared<DPDoubleAttr>(std::vector<double>{double(INT_MAX) + 1.0}),
        std::make_shared<DPDoubleAttr>(std::vector<double>{double(INT_MIN) - 1.0}),
        std::make_shared<DPDoubleAttr>(std::vector<double>{std::numeric_limits<double>::quiet_NaN()}),
        std::make_shared<DPDoubleAttr>(std::vector<double>{std::numeric_limits<double>::infinity()}),
        std::make_shared<DPFloatAttr>(std::vector<float>{2147483648.0f}),
        std::make_shared<DPVec2dAttr>(std::vector<Imath::V2d>{Imath::V2d(1, 3e9)}),
        std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>{Imath::V3d(1, 2, 3e9)}),
        std::make_shared<DPDoubleAttr>(std::vector<double>{1.0}, 3e9),
        std::make_shared<DPBoolAttr>(std::vector<bool>{true}),
        std::make_shared<DPStringAttr>(std::vector<std::string>{"value"}),
        std::make_shared<DPUInt32Attr>(std::vector<uint32_t>{1U}),
        std::make_shared<DPInt64Attr>(std::vector<int64_t>{1})
    };
    DPAttributeData::Ptr output = vec3;
    for (const auto& source : failures) {
        owner.attributes["bad"] = source;
        assert(!castPointAttribute(owner, "bad", 0));
        assert(owner.attributes.at("bad") == source);
        assert(!buildAttributeCast(source, 1U, 0, output));
        assert(output == vec3);
    }
    const auto tooWide = std::make_shared<DPDoubleAttr>(std::vector<double>{1e300});
    assert(!buildAttributeCast(tooWide, 1U, 1, output) && output == vec3);
    assert(buildAttributeCast(tooWide, 1U, 2, output));
    assert(std::dynamic_pointer_cast<DPDoubleAttr>(output)->value()[0] == 1e300);
    assert(!buildAttributeCast(tooWide, 0U, 2, output));
    assert(!buildAttributeCast(tooWide, 1U, -1, output));
    assert(!buildAttributeCast(tooWide, 1U, 3, output));
    assert(!buildAttributeCast(DPAttributeData::ConstPtr(), 0U, 0, output));
    assert(!buildAttributeCast(tooWide, std::numeric_limits<std::size_t>::max(), 2, output));
    assert(!castPointAttribute(owner, "missing", 0));
    assert(!castPointAttribute(owner, "", 0));
    assert(!castPointAttribute(owner, std::string("bad\0key", 7U), 0));

    // A late failure must preserve the complete original field.
    owner.count = 2U;
    const auto laterInvalid = std::make_shared<DPDoubleAttr>(std::vector<double>{1.0, 3e9});
    owner.attributes["late"] = laterInvalid;
    assert(!castPointAttribute(owner, "late", 0));
    assert(owner.attributes.at("late") == laterInvalid && laterInvalid->value()[0] == 1.0);
    owner.count = 0U;
    owner.attributes["empty with spaces"] = std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>());
    assert(castPointAttribute(owner, "empty with spaces", 0));
    assert(owner.attributes.at("empty with spaces")->getAttrType() == DP_ATTR_VEC3I);
    assert(!castPointAttribute(owner, "empty with spaces", 3));
}

void testGroupPublication()
{
    using namespace dommy::group;
    PointOwner owner;
    owner.count = 2U;
    owner.attributes["hit"] = std::make_shared<DPFloatAttr>(std::vector<float>{7.0f, 8.0f});
    const auto ordinary = owner.attributes.at("hit");
    const auto sourceMask = std::make_shared<DPGroupData>(DPIdList{false, false});
    owner.groups["hit"] = sourceMask;
    owner.groups["unrelated"] = sourceMask;
    assert(owner.replaceGrp("hit", DPIdList({true, false})));
    assert(owner.attributes.at("hit") == ordinary);
    assert(owner.groups.at("hit")->getGroupIds() == DPIdList({true, false}));
    assert(owner.groups.at("unrelated") == sourceMask && !sourceMask->getGroupIds()[0]);
    assert(owner.replaceGrp("hit", DPIdList({false, true})));
    assert(owner.groups.at("hit")->getGroupIds() == DPIdList({false, true}));
    assert(owner.replaceGrp("new with spaces", DPIdList({true, true})));
    const auto previous = owner.groups.at("hit");
    assert(!owner.replaceGrp("hit", DPIdList(1U, true)));
    assert(owner.groups.at("hit") == previous);
    assert(!owner.replaceGrp("", DPIdList(2U, true)));
    assert(!owner.replaceGrp(std::string("hit\0key", 7U), DPIdList(2U, true)));
    assert(!replaceGroupValues(owner.groups, "hit", DPIdList(),
        std::numeric_limits<std::size_t>::max()));

    // Both DCurve and DMesh use this exact partition adapter, including empty
    // primitives. All masks are preflighted before touching an earlier owner.
    std::vector<std::shared_ptr<PointOwner> > parts;
    for (std::size_t count : {2U, 0U, 1U}) {
        const auto part = std::make_shared<PointOwner>();
        part->count = count;
        part->attributes["hit"] = std::make_shared<DPFloatAttr>(std::vector<float>(count, 7.0f));
        assert(part->replaceGrp("hit", DPIdList(count, false)));
        parts.push_back(part);
    }
    const std::vector<DPIdList> masks{DPIdList{true, false}, DPIdList(), DPIdList{true}};
    assert(replacePointGroupPartitions(parts.begin(), parts.end(), "hit", masks));
    for (std::size_t i = 0U; i < parts.size(); ++i) {
        assert(parts[i]->groups.at("hit")->getGroupIds() == masks[i]);
        assert(parts[i]->attributes.at("hit")->attrSize() == parts[i]->count);
    }
    const auto first = parts[0]->groups.at("hit");
    std::vector<DPIdList> badMasks = masks;
    badMasks.back().clear();
    assert(!replacePointGroupPartitions(parts.begin(), parts.end(), "hit", badMasks));
    assert(parts[0]->groups.at("hit") == first);
    badMasks = masks; badMasks.pop_back();
    assert(!replacePointGroupPartitions(parts.begin(), parts.end(), "hit", badMasks));
    badMasks = masks; badMasks.emplace_back();
    assert(!replacePointGroupPartitions(parts.begin(), parts.end(), "hit", badMasks));
    assert(!replacePointGroupPartitions(parts.begin(), parts.end(), "", masks));
    parts.back().reset();
    assert(!replacePointGroupPartitions(parts.begin(), parts.end(), "hit", masks));
    assert(parts[0]->groups.at("hit") == first);
    parts.clear();
    assert(replacePointGroupPartitions(parts.begin(), parts.end(), "hit", std::vector<DPIdList>()));
}

void testGroupCombination()
{
    using namespace dommy::group;
    const DPIdList previous{false, false, true, true};
    const DPIdList generated{false, true, false, true};
    for (int action = 0; action < 4; ++action) {
        for (bool invert : {false, true}) {
            DPIdList expected(4U, false);
            for (std::size_t i = 0U; i < 4U; ++i) {
                const bool value = invert ? !generated[i] : generated[i];
                expected[i] = action == 0 ? value : action == 1 ? value || previous[i] :
                    action == 2 ? value && !previous[i] : value && previous[i];
            }
            DPIdList result;
            assert(combineGroupSelection(&previous, generated, action, invert, result));
            assert(result == expected);
            // Both operands may alias the destination without changing the result.
            result = previous;
            assert(combineGroupSelection(&result, generated, action, invert, result));
            assert(result == expected);
            result = generated;
            assert(combineGroupSelection(&previous, result, action, invert, result));
            assert(result == expected);

            PointOwner owner;
            owner.count = 4U;
            const auto source = std::make_shared<DPGroupData>(previous);
            owner.groups["selected"] = source;
            owner.groups["unrelated"] = source;
            owner.attributes["selected"] = std::make_shared<DPIntAttr>(std::vector<int32_t>(4U, 7));
            const auto attribute = owner.attributes.at("selected");
            assert(publishCombinedGroup(owner, "selected", 4U, generated, action, invert));
            assert(owner.groups.at("selected")->getGroupIds() == expected);
            assert(source->getGroupIds() == previous && owner.groups.at("unrelated") == source);
            assert(owner.attributes.at("selected") == attribute);

            DPGrpList componentGroups;
            componentGroups["selected"] = source;
            assert(publishCombinedComponentGroup(&componentGroups, "selected", 4U, generated, action, invert));
            assert(componentGroups.at("selected")->getGroupIds() == expected);
            assert(source->getGroupIds() == previous);

            DPIdList noPrevious = generated;
            if (invert) noPrevious.flip();
            assert(combineGroupSelection(nullptr, generated, action, invert, result));
            assert(result == noPrevious);
            assert(publishCombinedGroup(owner, "new group", 4U, generated, action, invert));
            assert(owner.groups.at("new group")->getGroupIds() == noPrevious);
        }
    }
    PointOwner owner;
    owner.count = 4U;
    assert(owner.replaceGrp("selected", previous));
    const auto original = owner.groups.at("selected");
    for (int action : {-1, 4}) {
        DPIdList result = previous;
        assert(!combineGroupSelection(&previous, generated, action, false, result));
        assert(result == previous);
        assert(!publishCombinedGroup(owner, "selected", 4U, generated, action, false));
        assert(owner.groups.at("selected") == original);
    }
    DPIdList mismatch(3U, true), result = generated;
    assert(!combineGroupSelection(&mismatch, generated, 0, false, result));
    assert(result == generated);
    assert(!publishCombinedGroup(owner, "selected", 3U, generated, 0, false));
    assert(!publishCombinedGroup(owner, "", 4U, generated, 0, false));
    assert(!publishCombinedComponentGroup(nullptr, "selected", 4U, generated, 0, false));
    assert(!publishCombinedComponentGroup(&owner.groups, "selected", 3U, mismatch, 0, false));
    assert(owner.groups.at("selected") == original);
    PointOwner empty;
    assert(publishCombinedGroup(empty, "empty", 0U, DPIdList(), 3, true));
    assert(empty.groups.at("empty")->getGroupIds().empty());
}

struct TestCurve {
    bool failClosest = false, failDirection = false;
    double parameter = 2.0;
    Imath::V3d closest{1.0, 2.0, 3.0}, direction{0.0, 1.0, 0.0};
    int closestCalls = 0, tangentCalls = 0, normalCalls = 0;
    Imath::V3d closestPoint(const Imath::V3d&, double* result, double, int, MStatus* status) {
        ++closestCalls;
        *status = failClosest ? MS::kFailure : MS::kSuccess;
        if (!failClosest) *result = parameter; // A failed query leaves it unwritten.
        return closest;
    }
    Imath::V3d tangent(double value, int, MStatus* status) {
        ++tangentCalls;
        assert(value == parameter);
        *status = failDirection ? MS::kFailure : MS::kSuccess;
        return direction;
    }
    Imath::V3d normal(double value, int space, MStatus* status) {
        ++normalCalls;
        return tangent(value, space, status);
    }
};

void testGroupEvaluation()
{
    using namespace dommy::group;
    DPIdList mask{true, false, false};
    const DPIdList original = mask;
    std::size_t calls = 0U;
    assert(!evaluateSelection(3U, mask, [&](std::size_t i, bool& selected) {
        ++calls; selected = true; return i != 2U;
    }));
    assert(calls == 3U && mask == original);
    assert(evaluateSelection(3U, mask, [](std::size_t i, bool& selected) {
        selected = i == 2U; return true;
    }));
    assert(mask == DPIdList({true, false, true}));
    assert(!evaluateSelection(2U, mask, [](std::size_t, bool&) { assert(false); return true; }));
    mask = original;
    assert(!selectPattern("1 invalid", 3U, mask) && mask == original);
    assert(selectPattern("1-3", 3U, mask) && mask == DPIdList(3U, true));
    DPIdList empty;
    assert(selectPattern("0;1-9", 0U, empty));
    assert(!selectPattern("0--2", 0U, empty));
    assert(evaluateSelection(0U, empty, [](std::size_t, bool&) { assert(false); return true; }));

    const Imath::V3d up(0.0, 1.0, 0.0), zero(0.0);
    bool selected = false;
    assert(normalSelected(Imath::V3d(0.0, 1e300, 0.0), up, 0.1, selected) && selected);
    assert(normalSelected(Imath::V3d(0.0, 1e-300, 0.0), up, 0.1, selected) && selected);
    assert(normalSelected(zero, up, 0.1, selected) && !selected);
    assert(!normalSelected(up, zero, 0.1, selected));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    assert(!normalSelected(Imath::V3d(nan, 0.0, 0.0), up, 0.1, selected));
    assert(!normalSelected(up, up, inf, selected));

    const Imath::V3d unchanged(7.0, 8.0, 9.0);
    Imath::V3d direction = unchanged;
    TestCurve curve;
    curve.failClosest = true;
    assert(!curveDirectionAtClosestPoint(curve, up, false, 1e-6, 0, direction));
    assert(direction[0] == unchanged[0] && direction[1] == unchanged[1] && direction[2] == unchanged[2] && curve.tangentCalls == 0);
    curve.failClosest = false;
    curve.parameter = nan;
    assert(!curveDirectionAtClosestPoint(curve, up, false, 1e-6, 0, direction));
    assert(direction[0] == unchanged[0] && direction[1] == unchanged[1] && direction[2] == unchanged[2] && curve.tangentCalls == 0);
    curve.parameter = 2.0;
    curve.closest[0] = inf;
    assert(!curveDirectionAtClosestPoint(curve, up, false, 1e-6, 0, direction));
    assert(direction[0] == unchanged[0] && direction[1] == unchanged[1] && direction[2] == unchanged[2] && curve.tangentCalls == 0);
    curve.closest = up;
    curve.failDirection = true;
    assert(!curveDirectionAtClosestPoint(curve, up, true, 1e-6, 0, direction));
    assert(direction[0] == unchanged[0] && direction[1] == unchanged[1] && direction[2] == unchanged[2] && curve.normalCalls == 1);
    curve.failDirection = false;
    curve.direction[0] = inf;
    assert(!curveDirectionAtClosestPoint(curve, up, true, 1e-6, 0, direction));
    assert(direction[0] == unchanged[0] && direction[1] == unchanged[1] && direction[2] == unchanged[2]);
    curve.direction = up;
    assert(curveDirectionAtClosestPoint(curve, up, false, 1e-6, 0, direction));
    assert(direction[0] == up[0] && direction[1] == up[1] && direction[2] == up[2]);
    assert(curveDirectionAtClosestPoint(curve, up, true, 1e-6, 0, direction));
    assert(direction[0] == up[0] && direction[1] == up[1] && direction[2] == up[2]);
    const int previousCalls = curve.closestCalls;
    assert(!curveDirectionAtClosestPoint(curve, Imath::V3d(nan, 0.0, 0.0), false, 1e-6, 0, direction));
    assert(!curveDirectionAtClosestPoint(curve, up, false, 0.0, 0, direction));
    assert(curve.closestCalls == previousCalls);
}

} // namespace

void runAttributeCastGroupTests()
{
    testAttributeCast();
    testGroupPublication();
    testGroupCombination();
    testGroupEvaluation();
}
