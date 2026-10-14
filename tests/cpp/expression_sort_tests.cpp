#include "common/utility/DAttributePublication.h"
#include "common/utility/DAttributeSort.h"
#include "common/utility/DExpressionResult.h"
#include "common/utility/DColorPublication.h"
#include "common/utility/DColorBounds.h"
#include "common/utility/DExpressionAttribute.h"
#include "common/utility/DSeExprCall.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DPointMean.h"
#include "common/utility/DScaledProduct.h"
#include "common/utility/DPrimitiveExplode.h"
#include "common/utility/DMeshDeformationInfo.h"
#include "common/utility/DMayaCheckedArray.h"

#include <cassert>
#include <cfenv>
#include <climits>
#include <limits>
#include <stdexcept>

namespace {

template <typename Value> bool sameValue(const Value& a, const Value& b) {
    using Components = dommy::numeric::ValueComponents<Value>;
    for (unsigned int axis = 0; axis < Components::count; ++axis)
        if (Components::get(a, axis) != Components::get(b, axis))
            return false;
    return true;
}

struct Owner {
    DPAttrList fields;
    std::vector<Imath::V3f> points;
    bool ignoreWrite = false, throwWrite = false;
    std::size_t numberOfPoints() const {
        return points.size();
    }
    DPAttrIter beginAttr() {
        return fields.begin();
    }
    DPAttrIter endAttr() {
        return fields.end();
    }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        return dommy::attribute::findAttribute(fields, name);
    }
    void addAttr(const std::string& name, const DPAttributeData& value) {
        if (throwWrite)
            throw std::runtime_error("injected write failure");
        if (ignoreWrite)
            return;
        const auto previous = getAttr(name);
        if (previous && previous->typeName() != value.typeName())
            return;
        fields[name] = value.copy();
    }
    void removeAttr(const std::string& name) {
        fields.erase(name);
    }
};

enum class BuildFailure { None, Allocation, ShortAllocation, Copy, ShortCopy };
BuildFailure buildFailure = BuildFailure::None;
struct BuildArray {
    std::vector<int> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    MStatus setLength(unsigned int count) {
        if (buildFailure == BuildFailure::Allocation) return MS::kFailure;
        values.resize(buildFailure == BuildFailure::ShortAllocation && count ? count - 1 : count);
        return MS::kSuccess;
    }
    MStatus copy(const BuildArray& other) {
        if (buildFailure == BuildFailure::Copy) return MS::kFailure;
        values = other.values;
        if (buildFailure == BuildFailure::ShortCopy && !values.empty()) values.pop_back();
        return MS::kSuccess;
    }
    int& operator[](unsigned int i) { return values.at(i); }
};

void testStagedMayaArrayBuild() {
    using dommy::maya::buildCheckedArray;
    BuildArray output;
    unsigned int reads = 0U;
    auto fill = [&](unsigned int i, int& value) -> MStatus { ++reads; value = int(i); return MS::kSuccess; };
    for (BuildFailure failure : {BuildFailure::Allocation, BuildFailure::ShortAllocation,
                                  BuildFailure::Copy, BuildFailure::ShortCopy}) {
        output.values = {17}; reads = 0U; buildFailure = failure;
        assert(buildCheckedArray(3U, fill, output) == MS::kFailure);
        if (failure == BuildFailure::Allocation || failure == BuildFailure::ShortAllocation)
            assert(reads == 0U);
        if (failure != BuildFailure::ShortCopy) assert(output.values == std::vector<int>({17}));
    }
    buildFailure = BuildFailure::None; output.values = {17};
    assert(buildCheckedArray(3U, [](unsigned int i, int& value) -> MStatus {
        value = int(i); return i == 2U ? MS::kInvalidParameter : MS::kSuccess;
    }, output) == MS::kInvalidParameter && output.values == std::vector<int>({17}));
    bool threw = false;
    try {
        (void)buildCheckedArray(3U, [](unsigned int, int&) -> MStatus {
            throw std::runtime_error("injected element read failure");
        }, output);
    } catch (const std::runtime_error&) { threw = true; }
    assert(threw && output.values == std::vector<int>({17}));
    assert(buildCheckedArray(dommy::generation::kMaxGeneratedPointCount + 1U, fill, output) == MS::kInvalidParameter);
    assert(buildCheckedArray(3U, fill, output) == MS::kSuccess && output.values == std::vector<int>({0,1,2}));
    assert(buildCheckedArray(0U, fill, output) == MS::kSuccess && output.values.empty());
}

struct ExplodePrimitive {
    std::vector<Imath::V3f> points = {Imath::V3f(1, 0, 0)};
    MMatrix matrix;
    std::size_t numberOfPoints() const { return points.size(); }
    const std::vector<Imath::V3f>* getPositionPtrConst() const { return &points; }
    MMatrix getTransformMatrix() const { return matrix; }
    Imath::V3d getCenterDouble() const {
        std::array<double, 3> local, world;
        if (!dommy::numeric::checkedPointMean(points, local) ||
            !dommy::numeric::checkedTransformPoint(local, matrix, world))
            throw std::invalid_argument("invalid test center");
        return Imath::V3d(world[0], world[1], world[2]);
    }
};
struct ExplodeGroup {
    std::vector<bool> mask;
    const std::vector<bool>& getGroupIds() const { return mask; }
};
struct ExplodeOwner : Owner {
    using Primitives = std::vector<std::shared_ptr<ExplodePrimitive>>;
    using Groups = std::map<std::string, std::shared_ptr<ExplodeGroup>>;
    Primitives primitives;
    Groups groups;
    Primitives::const_iterator beginPrims() const { return primitives.begin(); }
    Primitives::const_iterator endPrims() const { return primitives.end(); }
    Groups::const_iterator beginGrp() const { return groups.begin(); }
    Groups::const_iterator endGrp() const { return groups.end(); }
};

void testPrimitiveExplode() {
    using namespace dommy::geometry;
    double offset = 17;
    assert(scaledExplodeOffset(1e308, -1e308, 0.5, offset) && offset == 1e308);
    assert(scaledExplodeOffset(1e308, -1e308, 0.0, offset) && offset == 0.0);
    offset = 17;
    assert(!scaledExplodeOffset(1e308, -1e308, 2.0, offset) && offset == 17);
    ExplodeOwner owner;
    const auto primitive = std::make_shared<ExplodePrimitive>();
    owner.primitives.push_back(primitive);
    ExplodeSettings settings;
    std::vector<MMatrix> plan;
    primitive->matrix[0][0] = 2.0;
    assert(buildExplodeTransforms(owner, settings, plan));
    std::array<double, 3> result;
    assert(dommy::numeric::checkedTransformPoint(primitive->points[0], plan[0], result));
    assert(result[0] == 4.0 && primitive->matrix[3][0] == 0.0 && primitive->points[0][0] == 1.0f);
    // Rotation, reflection, shear, nonuniform scale, and singular transforms.
    settings.vectorType = 1; settings.vectorAttribute = "move";
    owner.fields["move"] = std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>{Imath::V3d(3,4,5)});
    for (double scale : {0.0, -2.0, 3.0}) {
        primitive->matrix = MMatrix();
        primitive->matrix[0][0] = 0.0; primitive->matrix[0][1] = scale;
        primitive->matrix[1][0] = -1.0; primitive->matrix[1][2] = 0.5;
        primitive->matrix[3][2] = 7.0;
        std::array<double, 3> before;
        assert(dommy::numeric::checkedTransformPoint(primitive->points[0], primitive->matrix, before));
        assert(buildExplodeTransforms(owner, settings, plan));
        assert(dommy::numeric::checkedTransformPoint(primitive->points[0], plan[0], result));
        for (unsigned int axis = 0U; axis < 3U; ++axis) assert(result[axis] == before[axis] + 3.0 + axis);
    }
    settings = ExplodeSettings(); settings.center[0] = 16777216.0;
    primitive->points[0] = Imath::V3f(0,0,0); primitive->matrix = MMatrix();
    primitive->matrix[3][0] = 16777217.0;
    const auto group = std::make_shared<ExplodeGroup>(); group->mask = {true};
    owner.groups["piece.a"] = group; settings.groupPattern = "piece.*";
    assert(buildExplodeTransforms(owner, settings, plan) && plan[0][3][0] == 16777218.0);
    settings.groupPattern.clear();
    assert(buildExplodeTransforms(owner, settings, plan) && plan[0][3][0] == 16777218.0);
    const double previous = plan[0][3][0];
    settings.centerAttribute = "center";
    for (const auto& bad : std::vector<DPAttributeData::Ptr>{
            std::make_shared<DPFloatAttr>(std::vector<float>{1}),
            std::make_shared<DPVec2fAttr>(std::vector<Imath::V2f>{Imath::V2f(1,2)}),
            std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>()),
            std::make_shared<DPVec3dAttr>(std::vector<Imath::V3d>{Imath::V3d(NAN,0,0)})}) {
        owner.fields["center"] = bad;
        assert(!buildExplodeTransforms(owner, settings, plan) && plan[0][3][0] == previous);
    }
    owner.fields.erase("center"); // Missing optional center still uses geometry.
    assert(buildExplodeTransforms(owner, settings, plan));
    owner.fields["center"] = std::make_shared<DPVec3iAttr>(std::vector<Imath::V3i>{Imath::V3i(16777218,0,0)});
    assert(buildExplodeTransforms(owner, settings, plan) && plan[0][3][0] == 16777219.0);
    owner.fields.erase("center");
    settings.groupPattern = "[";
    assert(!buildExplodeTransforms(owner, settings, plan));
    settings.groupPattern = "piece.*"; group->mask.clear();
    assert(!buildExplodeTransforms(owner, settings, plan));
    settings.groupPattern.clear(); settings.scale = std::numeric_limits<double>::infinity();
    assert(!buildExplodeTransforms(owner, settings, plan));
    settings.scale = 1.0; settings.vectorType = 2;
    assert(!buildExplodeTransforms(owner, settings, plan));
    settings.vectorType = 1; settings.vectorAttribute = "missing";
    assert(buildExplodeTransforms(owner, settings, plan) && plan[0][3][0] == primitive->matrix[3][0]);
    owner.fields["missing"] = std::make_shared<DPStringAttr>(std::vector<std::string>{"bad"});
    assert(!buildExplodeTransforms(owner, settings, plan));
    owner.fields.clear();
    const auto later = std::make_shared<ExplodePrimitive>();
    owner.primitives.push_back(later);
    later->matrix[0][0] = std::numeric_limits<double>::quiet_NaN();
    const std::size_t previousSize = plan.size();
    assert(!buildExplodeTransforms(owner, settings, plan) && plan.size() == previousSize);
    later->matrix = MMatrix(); later->points[0][0] = std::numeric_limits<float>::infinity();
    assert(!buildExplodeTransforms(owner, settings, plan) && plan.size() == previousSize);
    owner.primitives.back().reset();
    assert(!buildExplodeTransforms(owner, settings, plan) && plan.size() == previousSize);
    owner.fields.clear(); owner.groups.clear(); owner.primitives.clear();
    assert(buildExplodeTransforms(owner, settings, plan) && plan.empty());

    std::vector<Imath::V3d> geometric(4U, Imath::V3d(1e308,0,0)), centers(geometric);
    assert(replaceGroupCenter(geometric, std::vector<bool>(4U, true), centers));
    assert(centers[0][0] == 1e308 && centers[3][0] == 1e308);
    geometric = {Imath::V3d(1e308,0,0), Imath::V3d(1,0,0), Imath::V3d(-1e308,0,0)};
    centers = geometric;
    assert(replaceGroupCenter(geometric, std::vector<bool>(3U, true), centers));
    assert(std::abs(centers[0][0] - 1.0 / 3.0) < 1e-14);
    geometric = {Imath::V3d(0,0,0), Imath::V3d(2,0,0), Imath::V3d(10,0,0)};
    centers = geometric;
    assert(replaceGroupCenter(geometric, std::vector<bool>{true,true,false}, centers));
    assert(replaceGroupCenter(geometric, std::vector<bool>{false,true,true}, centers));
    assert(centers[0][0] == 1 && centers[1][0] == 6 && centers[2][0] == 6);
    assert(replaceGroupCenter(geometric, std::vector<bool>(3U, false), centers) && centers[1][0] == 6);
}

void testWorldCenterNodeChain() {
    using namespace dommy::numeric;
    for (bool baked : {false, true}) {
        ExplodeOwner owner;
        const auto primitive = std::make_shared<ExplodePrimitive>();
        primitive->points = {Imath::V3f(0,0,0), Imath::V3f(2,0,0)};
        owner.primitives.push_back(primitive);
        MMatrix translation, transformed;
        translation[3][0] = 10.0;
        assert(composeWorldPivotTransform(primitive->matrix, primitive->getCenterDouble(),
                                          translation, transformed));
        if (baked) {
            assert(checkedTransformPoints(primitive->points, transformed, primitive->points));
        } else {
            primitive->matrix = transformed;
        }
        // DMCharactor retains Vec3f storage, but obtains the world center.
        const Imath::V3d worldCenter = primitive->getCenterDouble();
        assert(worldCenter[0] == 11.0);
        Imath::V3f storedCenter;
        for (unsigned int axis = 0U; axis < 3U; ++axis)
            assert(checkedNumericCast(worldCenter[axis], storedCenter[axis]));
        owner.fields["center"] = std::make_shared<DPFloatAttr>(std::vector<float>{99});
        assert(dommy::attribute::commitStagedAttribute(owner, "center",
            DPAttributeData::Ptr(new DPVec3fAttr(std::vector<Imath::V3f>{storedCenter})), 1U));
        dommy::geometry::ExplodeSettings settings;
        settings.centerAttribute = "center";
        std::vector<MMatrix> exploded;
        assert(dommy::geometry::buildExplodeTransforms(owner, settings, exploded));
        for (unsigned int i = 0U; i < 2U; ++i) {
            std::array<double, 3> result;
            assert(checkedTransformPoint(primitive->points[i], exploded[0], result));
            assert(result[0] == 21.0 + 2.0 * i && result[1] == 0.0 && result[2] == 0.0);
        }
    }
}

void testWorldPivotTransform() {
    using namespace dommy::numeric;
    ExplodeOwner owner;
    auto primitive = std::make_shared<ExplodePrimitive>();
    primitive->points = {Imath::V3f(0,0,0), Imath::V3f(2,0,0)};
    owner.primitives.push_back(primitive);
    std::vector<MMatrix> exploded;
    assert(dommy::geometry::buildExplodeTransforms(owner, dommy::geometry::ExplodeSettings(), exploded));
    primitive->matrix = exploded[0];
    MMatrix scale, transformed;
    scale[0][0] = scale[1][1] = scale[2][2] = 2.0;
    assert(composeWorldPivotTransform(primitive->matrix, primitive->getCenterDouble(), scale, transformed));
    std::array<double, 3> result;
    for (unsigned int i = 0U; i < 2U; ++i) {
        assert(checkedTransformPoint(primitive->points[i], transformed, result));
        assert(result[0] == 4.0 * i && result[1] == 0.0 && result[2] == 0.0);
    }
    // Nonidentity input: compare the composed matrix with world-point arithmetic.
    MMatrix operation;
    operation[0][0] = 0.0; operation[0][1] = 2.0;
    operation[1][0] = -3.0; operation[1][1] = 0.0;
    operation[3][0] = 5.0; operation[3][2] = -7.0;
    for (double value : {0.0, -2.0, 3.0}) {
        MMatrix input;
        input[0][0] = 0.0; input[0][1] = value;
        input[1][0] = -1.0; input[1][2] = 0.5;
        input[3][0] = 16777217.0; input[3][2] = 4.0;
        const std::array<double, 3> center = {{16777218.0, 2.0, 3.0}};
        assert(composeWorldPivotTransform(input, center, operation, transformed));
        for (const auto& point : {Imath::V3f(0,0,0), Imath::V3f(2,3,4)}) {
            std::array<double, 3> world, expected;
            assert(checkedTransformPoint(point, input, world));
            for (unsigned int axis = 0U; axis < 3U; ++axis) world[axis] -= center[axis];
            assert(checkedTransformPoint(world, operation, expected));
            assert(checkedTransformPoint(point, transformed, result));
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                assert(result[axis] == expected[axis] + center[axis]);
        }
    }
    const MMatrix before = transformed;
    MMatrix bad;
    const std::array<double, 3> center = {{0,0,0}};
    bad[0][0] = NAN;
    assert(!composeWorldPivotTransform(bad, center, operation, transformed));
    bad = MMatrix(); bad[0][3] = 1.0;
    assert(!composeWorldPivotTransform(bad, center, operation, transformed));
    bad = MMatrix(); bad[0][0] = std::numeric_limits<double>::max();
    assert(!composeWorldPivotTransform(bad, center, scale, transformed));
    for (unsigned int i = 0U; i < 4U; ++i)
        for (unsigned int j = 0U; j < 4U; ++j) assert(transformed[i][j] == before[i][j]);
}

void testWideExponentPointMean() {
    using dommy::numeric::checkedPointMean;
    using Point = std::array<double, 3>;
    const double maximum = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    Point result = {{17,18,19}};
    auto mean = [&](const std::vector<double>& values) -> double {
        std::vector<Point> points;
        for (double value : values) points.push_back(Point{{value, value, value}});
        assert(checkedPointMean(points, result));
        assert(result[0] == result[1] && result[1] == result[2]);
        return result[0];
    };
    assert(mean({1e308,1e-100,-1e308}) == 1e-100 / 3.0);
    assert(mean({maximum,maximum,1e-100,-maximum,-maximum}) == 1e-100 / 5.0);
    assert(mean({maximum,maximum,maximum}) == maximum);
    assert(mean({-maximum,-maximum,-maximum}) == -maximum);
    assert(mean({maximum,-maximum}) == 0.0);
    assert(mean({tiny,tiny,tiny}) == tiny);
    assert(mean({tiny,0}) == 0.0); // Half-subnormal tie to even zero.
    assert(mean({tiny,2 * tiny}) == 2 * tiny);
    assert(mean({-tiny,-2 * tiny}) == -2 * tiny);
    assert(mean({1.0,std::nextafter(1.0,2.0)}) == 1.0);
    assert(mean({std::nextafter(1.0,2.0),std::nextafter(std::nextafter(1.0,2.0),2.0)}) ==
        std::nextafter(std::nextafter(1.0,2.0),2.0));
    for (int exponent = -1074; exponent <= 1023; exponent += 17) {
        const double residual = std::ldexp(1.0, exponent);
        std::vector<double> values = {-maximum, residual, maximum};
        std::sort(values.begin(), values.end());
        do { assert(mean(values) == residual / 3.0); }
        while (std::next_permutation(values.begin(), values.end()));
    }
    assert(mean(std::vector<double>(65537U, maximum)) == maximum);
    assert(mean(std::vector<double>(65537U, tiny)) == tiny);
    result = {{17,18,19}};
    assert(!checkedPointMean(std::vector<Point>(), result));
    assert(!checkedPointMean(std::vector<Point>{Point{{1,2,3}}, Point{{NAN,2,3}}}, result));
    assert(!checkedPointMean(std::vector<Point>{Point{{INFINITY,2,3}}}, result));
    assert((result == Point{{17,18,19}}));
    std::vector<double> scalar = {17};
    assert(dommy::attribute::convertScalarAttribute(
        std::make_shared<DPInt64Attr>(std::vector<int64_t>{INT64_C(9007199254740992)}), 1U, scalar));
    assert(scalar[0] == 9007199254740992.0);
    assert(!dommy::attribute::convertScalarAttribute(
        std::make_shared<DPStringAttr>(std::vector<std::string>{"wrong"}), 1U, scalar));
    assert(!dommy::attribute::convertScalarAttribute(
        std::make_shared<DPDoubleAttr>(std::vector<double>{NAN}), 1U, scalar));
    assert(scalar[0] == 9007199254740992.0);
}

void testGroupPatternBoundary() {
    using namespace dommy::group;
    ExplodeOwner owner;
    for (const auto& name : {"piece.1", "piece.2", "xpiece.1", "aa", "ab"})
        owner.groups[name] = std::make_shared<ExplodeGroup>();
    std::vector<std::string> names = {"previous"};
    assert(appendMatchedGroupNames(owner, R"(piece\.[12])", names));
    assert(names == std::vector<std::string>({"previous", "piece.1", "piece.2"}));
    names.clear();
    assert(appendMatchedGroupNames(owner, R"((a)\1)", names));
    assert(names == std::vector<std::string>({"aa"})); // Capture/backreference compatibility.
    names.clear();
    assert(appendMatchedGroupNames(owner, "(?=ab)ab", names));
    assert(names == std::vector<std::string>({"ab"})); // Lookahead compatibility.
    names = {"previous"};
    for (const auto& pattern : std::vector<std::string>{"[", std::string("a\0b", 3), std::string(4097U, 'a')}) {
        assert(!appendMatchedGroupNames(owner, pattern, names));
        assert(names == std::vector<std::string>({"previous"}));
    }
    PatternLimits limits;
    limits.groups = 2U;
    assert(!appendMatchedGroupNames(owner, ".*", names, limits));
    limits = PatternLimits(); limits.inputWork = 10U;
    unsigned int callbacks = 0U;
    assert(!forEachMatchingGroup(owner.beginGrp(), owner.endGrp(), ".*",
        [&](ExplodeOwner::Groups::const_iterator) { ++callbacks; return true; }, limits));
    assert(callbacks == 0U); // Validate every name/budget before any callback.
    limits = PatternLimits(); limits.nameBytes = 3U;
    assert(!appendMatchedGroupNames(owner, ".*", names, limits));
    assert(names == std::vector<std::string>({"previous"}));
    owner.groups[std::string("z\0bad", 5)] = std::make_shared<ExplodeGroup>();
    assert(!appendMatchedGroupNames(owner, ".*", names));
    owner.groups.erase(std::string("z\0bad", 5));
    assert(!forEachMatchingGroup(owner.beginGrp(), owner.endGrp(), ".*",
        [](ExplodeOwner::Groups::const_iterator) { return false; }));
    bool threw = false;
    try {
        (void)forEachMatchingGroup(owner.beginGrp(), owner.endGrp(), ".*",
            [](ExplodeOwner::Groups::const_iterator) -> bool { throw std::bad_alloc(); });
    } catch (const std::bad_alloc&) { threw = true; }
    assert(threw); // The compute boundary must receive allocation failures.
    owner.groups.clear();
    assert(!appendMatchedGroupNames(owner, "[", names));
    assert(appendMatchedGroupNames(owner, "", names));
    assert(names == std::vector<std::string>({"previous"}));
    owner.groups[""] = std::make_shared<ExplodeGroup>();
    assert(appendMatchedGroupNames(owner, "", names));
    assert(names == std::vector<std::string>({"previous", ""}));
}

void testMeshDeformationInfo() {
    using namespace dommy::mesh;
    const std::vector<Imath::V3f> points = {Imath::V3f(0,0,0), Imath::V3f(1,0,0), Imath::V3f(0,1,0)};
    MMatrix identity, translated;
    DeformationInfo info;
    assert(buildDeformationInfo(points, identity, points, identity, 1.0, true, info));
    assert(info.distances == std::vector<double>(3U, 0.0));
    for (const auto& value : info.vectors) assert(sameValue(value, Imath::V3f(0.0f)));
    translated[3][0] = 10.0;
    assert(buildDeformationInfo(points, identity, points, translated, 2.0, false, info));
    assert(info.distances == std::vector<double>(3U, 1.0));
    for (const auto& value : info.vectors) assert(sameValue(value, Imath::V3f(20,0,0)));
    const auto matrixResult = info;
    const std::vector<Imath::V3f> baked = {Imath::V3f(10,0,0), Imath::V3f(11,0,0), Imath::V3f(10,1,0)};
    assert(buildDeformationInfo(points, identity, baked, identity, 2.0, false, info));
    assert(info.distances == matrixResult.distances);
    for (std::size_t i = 0U; i < info.vectors.size(); ++i)
        assert(sameValue(info.vectors[i], matrixResult.vectors[i]));
    assert(buildDeformationInfo(points, translated, baked, identity, 1.0, true, info));
    assert(info.distances == std::vector<double>(3U, 0.0));
    MMatrix scale;
    scale[0][0] = 2.0; scale[1][1] = 3.0;
    assert(buildDeformationInfo(points, identity, points, scale, 1.0, false, info));
    assert(info.distances == std::vector<double>({0.0,0.5,1.0}));
    translated[3][0] = 1e100;
    assert(buildDeformationInfo(points, identity, points, translated, 1.0, true, info));
    assert(sameValue(info.vectors[0], Imath::V3f(1,0,0)));
    const auto previous = info.distances;
    assert(!buildDeformationInfo(points, identity, points, translated, 1.0, false, info));
    assert(info.distances == previous);
    assert(!buildDeformationInfo(points, identity, points, identity, NAN, false, info));
    assert(!buildDeformationInfo(points, identity, std::vector<Imath::V3f>(), identity, 1.0, false, info));
    translated[3][0] = NAN;
    assert(!buildDeformationInfo(points, identity, points, translated, 1.0, true, info));
    std::vector<Imath::V3f> empty;
    assert(buildDeformationInfo(empty, identity, empty, identity, 1.0, true, info) && info.distances.empty());

    Imath::V3f area(17.0f);
    assert(vertexAreaChange({4,9,0}, {1,18,0}, std::vector<int>{0,1,2}, 3U, 2.0, area));
    assert(sameValue(area, Imath::V3f(6,18,0)));
    assert(vertexAreaChange({0}, {0}, std::vector<int>{0}, 1U, 1.0, area));
    assert(sameValue(area, Imath::V3f(0.0f)));
    assert(vertexAreaChange({0,0}, {1e308,1e308}, std::vector<int>{0,1}, 2U, 1e-300, area));
    assert(std::fabs(area[1] - 1e8) < 16.0);
    area = Imath::V3f(17.0f);
    assert(!vertexAreaChange({1}, {0}, std::vector<int>{-1}, 1U, 1.0, area));
    assert(!vertexAreaChange({1}, {0}, std::vector<int>{1}, 1U, 1.0, area));
    assert(!vertexAreaChange({NAN}, {0}, std::vector<int>{0}, 1U, 1.0, area));
    assert(!vertexAreaChange({1}, {0}, std::vector<int>{0}, 1U, 1e100, area));
    assert(sameValue(area, Imath::V3f(17.0f)));

    Owner owner;
    owner.fields["defDist"] = std::make_shared<DPStringAttr>(std::vector<std::string>{"old"});
    assert(buildDeformationInfo(points, identity, points, identity, 1.0, true, info));
    std::vector<Imath::V3f> areas(3U, Imath::V3f(0.0f));
    assert(publishDeformationInfo(owner, info, areas, 3U));
    const auto stored = owner.fields["defDist"];
    assert(stored->getAttrType() == DP_ATTR_DOUBLE && stored->attrSize() == 3U);
    areas.back()[0] = NAN;
    assert(!publishDeformationInfo(owner, info, areas, 3U) && owner.fields["defDist"] == stored);
}

void testExactIntegerBlends() {
    using dommy::attribute::checkedLinearBlend;
    const int64_t minimum = std::numeric_limits<int64_t>::min();
    const int64_t maximum = std::numeric_limits<int64_t>::max();
    int64_t result = 17;
    assert(checkedLinearBlend(maximum, minimum, 0.0, result) && result == maximum);
    assert(checkedLinearBlend(maximum, minimum, 1.0, result) && result == minimum);
    assert(checkedLinearBlend(minimum, maximum, 0.5, result) && result == 0);
    assert(checkedLinearBlend(maximum, minimum, 0.5, result) && result == 0);
    assert(checkedLinearBlend(maximum - 1, maximum, 0.5, result) && result == maximum - 1);
    assert(checkedLinearBlend(minimum, minimum + 1, 0.5, result) && result == minimum + 1);
    const int64_t adjacent = INT64_C(9007199254740993);
    assert(checkedLinearBlend(adjacent, adjacent + 2, 0.5, result) && result == adjacent + 1);
    assert(checkedLinearBlend(adjacent, minimum, 0.0, result) && result == adjacent);
    const double tiny = std::numeric_limits<double>::denorm_min();
    assert(checkedLinearBlend(int64_t(5), int64_t(0), tiny, result) && result == 4);
    assert(checkedLinearBlend(int64_t(-5), int64_t(0), tiny, result) && result == -4);
    assert(checkedLinearBlend(maximum, maximum - 1, 1.5, result) && result == maximum - 2);
    assert(checkedLinearBlend(minimum, minimum + 1, 1.5, result) && result == minimum + 2);
    assert(checkedLinearBlend(maximum, maximum, 1e300, result) && result == maximum);
    result = 17;
    assert(!checkedLinearBlend(maximum, maximum - 1, -1.0, result) && result == 17);
    assert(!checkedLinearBlend(minimum, minimum + 1, -1.0, result) && result == 17);
    assert(!checkedLinearBlend(minimum, maximum, 1e300, result) && result == 17);
    assert(!checkedLinearBlend(minimum, maximum, std::numeric_limits<double>::infinity(), result));
    assert(!checkedLinearBlend(minimum, maximum, std::numeric_limits<double>::quiet_NaN(), result));
    assert(result == 17);

    const uint64_t umax = std::numeric_limits<uint64_t>::max();
    uint64_t unsignedResult = 17;
    assert(checkedLinearBlend(umax, uint64_t(0), 0.0, unsignedResult) && unsignedResult == umax);
    assert(checkedLinearBlend(uint64_t(0), umax, 1.0, unsignedResult) && unsignedResult == umax);
    assert(checkedLinearBlend(uint64_t(0), umax, 0.5, unsignedResult) && unsignedResult == umax / 2);
    assert(checkedLinearBlend(umax, uint64_t(0), tiny, unsignedResult) && unsignedResult == umax - 1);
    assert(checkedLinearBlend(uint64_t(0), umax, -tiny, unsignedResult) && unsignedResult == 0);
    unsignedResult = 17;
    assert(!checkedLinearBlend(umax, umax - 1, -1.0, unsignedResult) && unsignedResult == 17);
    assert(!checkedLinearBlend(uint64_t(0), umax, -1.0, unsignedResult) && unsignedResult == 17);

    // An integer oracle covers both directions, fractional cancellation, zero,
    // and extrapolation without repeating the implementation's floating math.
    for (int a = -8; a <= 8; ++a)
        for (int b = -8; b <= 8; ++b)
            for (int numerator = -12; numerator <= 12; ++numerator) {
                const int expected = (4 * a + (b - a) * numerator) / 4;
                int actual = 17;
                assert(checkedLinearBlend(a, b, numerator / 4.0, actual) && actual == expected);
            }
}

template <typename Value> void testFloatingBlendEndpoints() {
    using dommy::attribute::checkedLinearBlend;
    const Value maximum = std::numeric_limits<Value>::max();
    Value result = 17;
    assert(checkedLinearBlend(maximum, Value(1), 0.0, result) && result == maximum);
    assert(checkedLinearBlend(maximum, Value(1), 1.0, result) && result == 1);
    assert(checkedLinearBlend(maximum, -maximum, 0.5, result) && result == 0);
    assert(checkedLinearBlend(-maximum, maximum, 0.5, result) && result == 0);
    assert(checkedLinearBlend(maximum, maximum, 1e300, result) && result == maximum);
    assert(checkedLinearBlend(Value(2), Value(4), 1.5, result) && result == 5);
    result = 17;
    assert(!checkedLinearBlend(maximum, -maximum, -1.0, result) && result == 17);
    assert(!checkedLinearBlend(maximum, std::numeric_limits<Value>::infinity(), 0.0, result));
    assert(!checkedLinearBlend(maximum, Value(1), std::numeric_limits<double>::quiet_NaN(), result));
    assert(result == 17);
}

void testCommonWorldPointPrecision() {
    using namespace dommy::numeric;
    std::vector<Imath::V3f> points = {Imath::V3f(0, 0, 0), Imath::V3f(2, 2, 2)};
    MMatrix matrix;
    matrix[3][0] = 16777217.0;
    dommy::math::Bounds3d bounds;
    assert(appendTransformedBounds(points, matrix, bounds));
    assert(bounds.minimum(0) == 16777217.0 && bounds.maximum(0) == 16777219.0);
    assert(bounds.maximum(0) - bounds.minimum(0) == 2.0);
    std::vector<Imath::V3d> doubles;
    assert(checkedTransformPoints(points, matrix, doubles));
    assert(doubles[0][0] == 16777217.0 && doubles[1][0] == 16777219.0);
    std::vector<Imath::V3f> floats(1, Imath::V3f(7, 8, 9));
    matrix[3][0] = 2.0 * std::numeric_limits<float>::max();
    assert(!checkedTransformPoints(points, matrix, floats));
    assert(floats.size() == 1 && sameValue(floats[0], Imath::V3f(7, 8, 9)));
    dommy::math::Bounds3d largeBounds;
    assert(appendTransformedBounds(points, matrix, largeBounds));
    assert(largeBounds.minimum(0) == matrix[3][0]);
    matrix = MMatrix();
    points.back()[2] = std::numeric_limits<float>::infinity();
    assert(!appendTransformedBounds(points, matrix, bounds));
    assert(bounds.count() == 2 && bounds.minimum(0) == 16777217.0 && bounds.maximum(0) == 16777219.0);
    assert(!checkedTransformPoints(points, matrix, doubles));
    assert(doubles.size() == 2 && doubles[0][0] == 16777217.0 && doubles[1][0] == 16777219.0);
    assert(sameValue(points[0], Imath::V3f(0, 0, 0)));

    // Keep the arithmetic vertex mean, not the midpoint of the bounding box.
    points = {Imath::V3f(0, 0, 0), Imath::V3f(0, 0, 0), Imath::V3f(3, 0, 0)};
    matrix[3][0] = 16777217.0;
    std::array<double, 3> local, world;
    assert(checkedPointMean(points, local) && local[0] == 1.0);
    assert(checkedTransformPoint(local, matrix, world) && world[0] == 16777218.0);
    matrix[3][0] = 1e308;
    dommy::math::Bounds3d hugeWorldBounds;
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(appendTransformedBounds(points, matrix, hugeWorldBounds));
    assert(hugeWorldBounds.count() == 3U && hugeWorldBounds.minimum(0) == 1e308 &&
           hugeWorldBounds.maximum(0) == 1e308);
    assert(checkedPointMean(points, local) && local[0] == 1.0);
    assert(checkedTransformPoint(local, matrix, world) && world[0] == 1e308);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
    points.clear();
    assert(checkedTransformPoints(points, matrix, doubles) && doubles.empty());
    assert(appendTransformedBounds(points, matrix, bounds) && bounds.count() == 2);
}

void testExtremeAffineProductSums() {
    using namespace dommy::numeric;
    const double maximum = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    double result = 91.0;
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(checkedProductSum(std::array<double, 3>{{maximum, maximum, 1.0}},
        std::array<double, 3>{{maximum, -maximum, 1.0}}, result) && result == 1.0);
    assert(checkedProductSum(std::array<double, 3>{{maximum, 1.0, maximum}},
        std::array<double, 3>{{maximum, 1.0, -maximum}}, result) && result == 1.0);
    assert(checkedProductSum(std::array<double, 3>{{1e16, 1.0, -1e16}},
        std::array<double, 3>{{1.0, 1.0, 1.0}}, result) && result == 1.0);
    // A nonzero final tail must not hide an earlier lost low-order term.
    assert(checkedProductSum(std::array<double, 4>{{1e16, 1.0, -1e16, 10000.0}},
        std::array<double, 4>{{1.0, 1.0, 1.0, 1.0}}, result) && result == 10001.0);
    assert(checkedProductSum(std::array<double, 2>{{tiny, tiny}},
        std::array<double, 2>{{0.5, 0.5}}, result) && result == tiny);
    // A tiny tail above the subnormal midpoint must survive cancellation and
    // round once, not first to 53 bits and then to a subnormal (double rounding).
    assert(checkedProductSum(std::array<double, 2>{{tiny, tiny}},
        std::array<double, 2>{{0.5, tiny}}, result) && result == tiny);
    assert(checkedProductSum(std::array<double, 1>{{tiny}},
        std::array<double, 1>{{0.5}}, result) && result == 0.0);
    const double epsilon = std::numeric_limits<double>::epsilon();
    assert(checkedProductSum(std::array<double, 4>{{maximum, maximum, 1.0, epsilon}},
        std::array<double, 4>{{maximum, -maximum, 1.0, 0.5}}, result) && result == 1.0);
    assert(checkedProductSum(std::array<double, 4>{{maximum, maximum, 1.0 + epsilon, epsilon}},
        std::array<double, 4>{{maximum, -maximum, 1.0, 0.5}}, result) && result == 1.0 + 2.0 * epsilon);
    result = 91.0;
    assert(!checkedProductSum(std::array<double, 1>{{maximum}},
        std::array<double, 1>{{maximum}}, result) && result == 91.0);
    assert(!checkedProductSum(std::array<double, 1>{{0.0}},
        std::array<double, 1>{{std::numeric_limits<double>::infinity()}}, result) && result == 91.0);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);

    MMatrix matrix;
    matrix[0][0] = 1.0; matrix[1][0] = 1.0; matrix[2][0] = 1.0; matrix[3][0] = 10000.0;
    std::array<double, 3> cancelled;
    assert(checkedTransformPoint(std::array<float, 3>{{std::ldexp(1.0f, 54), 1.0f,
        -std::ldexp(1.0f, 54)}}, matrix, cancelled) && cancelled[0] == 10001.0);
    matrix = MMatrix();
    matrix[0][0] = 1e308; matrix[1][0] = -1e308; matrix[3][0] = 1.0;
    const std::array<float, 3> point{{2, 2, 0}};
    std::array<float, 3> floats{{91, 92, 93}};
    std::array<double, 3> doubles{{91, 92, 93}};
    assert(checkedTransformPoint(point, matrix, floats) && floats[0] == 1.0f);
    assert(checkedTransformPoint(point, matrix, doubles) && doubles[0] == 1.0);
    assert(checkedTransformDirection(point, matrix, doubles) && doubles[0] == 0.0);
    matrix[0][0] = maximum; matrix[1][0] = 1.0; matrix[2][0] = -maximum;
    const std::array<double, 3> extreme{{maximum, 1.0, maximum}};
    assert(checkedTransformDirection(extreme, matrix, doubles) && doubles[0] == 1.0);
    matrix = MMatrix();
    matrix[2][2] = 1e308;
    floats = {{91, 92, 93}};
    assert(!checkedTransformPoint(std::array<float, 3>{{1, 2, 3}}, matrix, floats));
    assert((floats == std::array<float, 3>{{91, 92, 93}}));
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);

    MMatrix primitive, operation, composed;
    primitive[0][0] = maximum; primitive[0][1] = 1.0; primitive[0][2] = maximum;
    operation[0][0] = maximum; operation[1][0] = 1.0; operation[2][0] = -maximum;
    const std::array<double, 3> zero{{0, 0, 0}};
    assert(composeWorldPivotTransform(primitive, zero, operation, composed));
    assert(composed[0][0] == 1.0 && composed[3][3] == 1.0);
    primitive = MMatrix(); operation = MMatrix();
    primitive[3][0] = 1e308; operation[0][0] = 0.25;
    assert(composeWorldPivotTransform(primitive, std::array<double, 3>{{-1e308, 0, 0}}, operation, composed));
    assert(composed[3][0] == -5e307);
    const MMatrix before = composed;
    primitive[0][0] = maximum; operation[0][0] = maximum;
    assert(!composeWorldPivotTransform(primitive, zero, operation, composed));
    for (unsigned int row = 0U; row < 4U; ++row)
        for (unsigned int column = 0U; column < 4U; ++column)
            assert(composed[row][column] == before[row][column]);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

void testNormalAndScaledProductExtremes() {
    using namespace dommy::numeric;
    const double maximum = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    std::array<double, 3> output{{91, 92, 93}};
    std::feclearexcept(FE_ALL_EXCEPT);
    for (double scale : {1e-300, 1e-40, 1.0, 1e20, 1e300, tiny, maximum}) {
        MMatrix matrix;
        matrix[0][0] = matrix[1][1] = matrix[2][2] = scale;
        assert(checkedTransformNormal(std::array<float, 3>{{0, 0, 1}}, matrix, output));
        assert(output[0] == 0.0 && output[1] == 0.0 && output[2] == 1.0);
    }
    MMatrix matrix;
    matrix[0][0] = matrix[1][1] = 1e-100;
    assert(checkedTransformNormal(std::array<float, 3>{{0, 0, 1}}, matrix, output) && output[2] == 1.0);
    matrix[0][0] = matrix[1][1] = tiny;
    assert(checkedTransformNormal(std::array<double, 3>{{1, 1, 0}}, matrix, output));
    assert(std::fabs(output[0] - std::sqrt(0.5)) < 1e-15 && output[0] == output[1]);
    matrix[0][0] = matrix[1][1] = matrix[2][2] = maximum;
    assert(checkedTransformNormal(std::array<double, 3>{{maximum, maximum, 0}}, matrix, output));
    assert(std::fabs(output[0] - std::sqrt(0.5)) < 1e-15 && output[0] == output[1]);
    matrix[0][0] = -tiny; matrix[1][1] = maximum; matrix[2][2] = 1.0;
    assert(checkedTransformNormal(std::array<double, 3>{{1, 1, 1}}, matrix, output));
    assert(output[0] == -1.0 && output[1] == 0.0 && output[2] == tiny);
    // Exact singularity must be distinguished from an underflowed determinant.
    matrix = MMatrix();
    for (int column = 0; column < 3; ++column) matrix[1][column] = matrix[0][column];
    output = {{91, 92, 93}};
    assert(!checkedTransformNormal(std::array<double, 3>{{1, 0, 0}}, matrix, output));
    assert((output == std::array<double, 3>{{91, 92, 93}}));
    assert(checkedTransformNormal(std::array<double, 3>{{0, 0, 0}}, matrix, output));
    assert((output == std::array<double, 3>{{0, 0, 0}}));
    matrix = MMatrix(); output = {{91, 92, 93}};
    assert(!checkedTransformNormal(std::array<double, 3>{{0, 0, std::numeric_limits<double>::quiet_NaN()}}, matrix, output));
    assert((output == std::array<double, 3>{{91, 92, 93}}));
    matrix[0][3] = 1.0;
    assert(!checkedTransformNormal(std::array<double, 3>{{0, 0, 0}}, matrix, output));
    assert((output == std::array<double, 3>{{91, 92, 93}}));
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);

    double result = 91.0;
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(scaledProduct(std::array<double, 3>{{1e308, 2.0, 0.25}}, result) && result == 5e307);
    assert(scaledProduct(std::array<double, 4>{{1e-308, 1e-308, 1e308, 1e308}}, result));
    assert(std::fabs(result - 1.0) < 1e-14);
    assert(scaledProduct(std::array<double, 2>{{tiny, 1.0}}, result) && result == tiny);
    assert(scaledProduct(std::array<double, 2>{{tiny, 0.5}}, result) && result == 0.0);
    assert(scaledProduct(std::array<double, 2>{{-tiny, 1e-308}}, result) && result == 0.0 && std::signbit(result));
    assert(scaledProduct(std::array<double, 1>{{maximum}}, result) && result == maximum);
    result = 91.0;
    assert(!scaledProduct(std::array<double, 2>{{maximum, 2.0}}, result) && result == 91.0);
    assert(!scaledProduct(std::array<double, 2>{{0.0, std::numeric_limits<double>::infinity()}}, result) && result == 91.0);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

void testBlendPublication() {
    using dommy::attribute::publishBlendedAttribute;
    const auto a = std::make_shared<DPDoubleAttr>(std::vector<double>{2.0}, 4.0);
    const auto b = std::make_shared<DPDoubleAttr>(std::vector<double>{11.0});
    const DPDoubleAttr result(std::vector<double>{6.5}, 4.0);
    for (const std::string& name : {std::string("a"), std::string("b"), std::string("new")}) {
        for (bool remove : {false, true}) {
            Owner owner;
            owner.fields["a"] = a;
            owner.fields["b"] = b;
            owner.fields["unrelated"] = a;
            assert(publishBlendedAttribute(owner, "a", "b", name, result, remove));
            const auto written = std::dynamic_pointer_cast<const DPDoubleAttr>(owner.getAttr(name));
            assert(written && written->value()[0] == 6.5 && written->getDefaultValue() == 4.0);
            assert(bool(owner.getAttr("a")) == (!remove || name == "a"));
            assert(bool(owner.getAttr("b")) == (!remove || name == "b"));
            assert(owner.getAttr("unrelated") == a);
            assert(a->value()[0] == 2.0 && b->value()[0] == 11.0);
        }
    }
    Owner owner;
    owner.fields["a"] = a;
    owner.fields["b"] = b;
    const auto wrong = std::make_shared<DPVec3fAttr>(std::vector<Imath::V3f>{Imath::V3f(7, 8, 9)});
    owner.fields["out"] = wrong;
    assert(!publishBlendedAttribute(owner, "a", "b", "out", result, true));
    assert(owner.getAttr("a") == a && owner.getAttr("b") == b && owner.getAttr("out") == wrong);
    assert(!publishBlendedAttribute(owner, "a", "b", "", result, true));
    assert(!publishBlendedAttribute(owner, "a", "missing", "new", result, true));
    const DPDoubleAttr badSize(std::vector<double>{1, 2});
    assert(!publishBlendedAttribute(owner, "a", "b", "new", badSize, true));
    owner.ignoreWrite = true;
    assert(!publishBlendedAttribute(owner, "a", "b", "new", result, true));
    assert(owner.getAttr("a") == a && owner.getAttr("b") == b);
    owner.ignoreWrite = false;
    owner.throwWrite = true;
    bool threw = false;
    try {
        publishBlendedAttribute(owner, "a", "b", "new", result, true);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw && owner.getAttr("a") == a && owner.getAttr("b") == b);
}

// Minimal AST double for the exact call-boundary adapter used by all three
// SeExpr custom helpers. It deliberately gives each call its own name.
struct Call {
    std::string name;
    int arguments = 1;
    bool stringArgument = true, childValid = true;
    int prepared = 0, errors = 0;
    explicit Call(const std::string& value) : name(value) {}
    int numChildren() const {
        return arguments;
    }
    bool isStrArg(int) const {
        return stringArgument;
    }
    std::string getStrArg(int) const {
        return name;
    }
    Call* child(int) {
        return this;
    }
    bool prep(bool) {
        ++prepared;
        return childValid;
    }
    void addError(const std::string&) {
        ++errors;
    }
};

void testCallIsolation() {
    using namespace dommy::expression;
    for (int count : {1, 2}) {
        Call a("a"), b("b");
        a.arguments = b.arguments = count;
        assert(prepareNamedCall(&a, count, false));
        assert(prepareNamedCall(&b, count, false));
        std::string name;
        assert(readCallName(&a, count, name) && name == "a");
        assert(readCallName(&b, count, name) && name == "b");
        assert(readCallName(&a, count, name) && name == "a");
        assert(a.prepared == count - 1 && b.prepared == count - 1);
        b.childValid = false;
        if (count == 2)
            assert(!prepareNamedCall(&b, count, true));
    }
    Call bad("");
    assert(!prepareNamedCall(&bad, 1, false) && bad.errors == 1);
    bad.name = std::string("a\0b", 3);
    assert(!prepareNamedCall(&bad, 1, false));
    bad.name = "valid name";
    bad.stringArgument = false;
    assert(!prepareNamedCall(&bad, 1, false));
    bad.stringArgument = true;
    assert(!prepareNamedCall(&bad, 2, false));
    assert(prepareNamedCall(&bad, 1, false));
    std::string previous = "unchanged";
    assert(!readCallName(static_cast<const Call*>(nullptr), 1, previous));
    assert(previous == "unchanged");
}

struct ReadingExpression {
    Owner& owner;
    std::size_t index = 0U;
    bool failLast = false;
    explicit ReadingExpression(Owner& value) : owner(value) {}
    void setId(std::size_t value) {
        index = value;
    }
    void setPos(const Imath::V3f&) {}
    Imath::V3d evaluate() const {
        const auto source = std::dynamic_pointer_cast<const DPDoubleAttr>(owner.getAttr("value"));
        assert(source);
        return Imath::V3d(source->value()[index] + 1.0, 2.0, 3.0);
    }
    bool evaluationSucceeded() const {
        return !failLast || index + 1U != owner.points.size();
    }
};

void testExpressionResults() {
    using namespace dommy::expression;
    Owner owner;
    owner.points.resize(2U, Imath::V3f(0, 0, 0));
    const auto original = std::make_shared<DPDoubleAttr>(std::vector<double>{16777216.0, 1e100});
    owner.fields["value"] = original;
    owner.fields["unrelated"] = original;
    ReadingExpression expression(owner);
    std::vector<Imath::V3d> values;
    assert(evaluateResults(expression, owner.points, values));
    assert(owner.getAttr("value") == original); // old same-name field remains readable.
    assert(values[0][0] == 16777217.0 && values[1][0] == 1e100);
    assert(publishResult(owner, "value", 1, values));
    const auto output = std::dynamic_pointer_cast<const DPDoubleAttr>(owner.getAttr("value"));
    assert(output && output->value()[0] == 16777217.0 && output->value()[1] == 1e100);
    assert(owner.getAttr("unrelated") == original && original->value()[0] == 16777216.0);
    assert(!publishResult(owner, "value", 3, values)); // float destination is still checked.
    assert(owner.getAttr("value") == output);
    std::vector<Imath::V3d> previous{Imath::V3d(9, 8, 7)};
    expression.failLast = true;
    assert(!evaluateResults(expression, owner.points, previous));
    assert(previous.size() == 1U && previous[0][0] == 9.0);
    expression.failLast = false;
    owner.points[1][0] = std::numeric_limits<float>::quiet_NaN();
    assert(!evaluateResults(expression, owner.points, previous));
    owner.points[1][0] = 0.0f;
    values.assign(2U, Imath::V3d(1.25, 2.5, 3.75));
    assert(publishResult(owner, "value", 2, values));
    const auto vec2 = std::dynamic_pointer_cast<const DPVec2fAttr>(owner.getAttr("value"));
    assert(vec2 && vec2->value()[0][0] == 1.25f && vec2->value()[0][1] == 2.5f);
    assert(publishResult(owner, "new", 3, values));
    assert(owner.getAttr("new")->getAttrType() == DP_ATTR_VEC3F);
    DPAttributeData::Ptr candidate = original;
    values[1][0] = std::numeric_limits<double>::infinity();
    assert(!buildResult(values, 1, candidate) && candidate == original);
    assert(!buildResult(values, 0, candidate));
    assert(!buildResult(values, 4, candidate));
    assert(!publishResult(owner, "", 1, values));
    values.clear();
    assert(!publishResult(owner, "value", 1, values) && owner.getAttr("value") == vec2);
    Owner empty;
    for (int dimension : {1, 2, 3}) {
        assert(publishResult(empty, "empty", dimension, values));
        assert(empty.getAttr("empty")->attrSize() == 0U);
    }
}

struct CheckedExpression {
    Imath::V3d value = Imath::V3d(1, 2, 3);
    bool succeeded = true;
    double frame = -1.0, seconds = -1.0;
    Imath::V3d evaluate() {
        return value;
    }
    bool evaluationSucceeded() const {
        return succeeded;
    }
    void setFrame(double v) {
        frame = v;
    }
    void setTime(double v) {
        seconds = v;
    }
};

struct LocalScale {
    Imath::V3d val;
    bool vector;
    bool isVec() {
        return vector;
    }
};

struct TimeValue {
    enum Unit { Frame, Seconds };
    double seconds, framesPerSecond;
    double as(Unit unit) const {
        return unit == Seconds ? seconds : seconds * framesPerSecond;
    }
};

void testEvaluationBoundaries() {
    using namespace dommy::expression;
    CheckedExpression expression;
    Imath::V3d result(9, 8, 7);
    assert(evaluateChecked(expression, result) && sameValue(result, Imath::V3d(1, 2, 3)));
    expression.succeeded = false;
    expression.value = Imath::V3d(0, 0, 0); // Custom helpers return finite zero on failure.
    assert(!evaluateChecked(expression, result) && sameValue(result, Imath::V3d(1, 2, 3)));
    expression.succeeded = true;
    for (unsigned int axis = 0; axis < 3; ++axis) {
        expression.value = Imath::V3d(1, 2, 3);
        expression.value[axis] = std::numeric_limits<double>::infinity();
        assert(!evaluateChecked(expression, result) && sameValue(result, Imath::V3d(1, 2, 3)));
        expression.value[axis] = std::numeric_limits<double>::quiet_NaN();
        assert(!evaluateChecked(expression, result));
    }
    LocalScale local{Imath::V3d(2, 0, 0), false};
    assert(readScale(local, result) && sameValue(result, Imath::V3d(2, 2, 2)));
    local.val = Imath::V3d(-2, 3, 0);
    local.vector = true;
    assert(readScale(local, result) &&
           sameValue(result, local.val)); // Negative/zero scales remain supported.
    const Imath::V3d previous = result;
    local.val[1] = std::numeric_limits<double>::quiet_NaN();
    assert(!readScale(local, result) && sameValue(result, previous));
    local.vector = false;
    assert(readScale(local, result) && sameValue(result, Imath::V3d(-2, -2, -2)));
    for (double rate : {24.0, 30.0, 60.0}) {
        assert(setTime(expression, TimeValue{2.0, rate}, TimeValue::Frame, TimeValue::Seconds));
        assert(expression.frame == 2.0 * rate && expression.seconds == 2.0);
    }
    assert(!setTime(expression, TimeValue{std::numeric_limits<double>::infinity(), 24},
                    TimeValue::Frame, TimeValue::Seconds));
    assert(expression.frame == 120 && expression.seconds == 2);
}

void testColorPublication() {
    Owner owner;
    owner.points.resize(2U, Imath::V3f(0, 0, 0));
    const auto uv =
        std::make_shared<DPVec2fAttr>(std::vector<Imath::V2f>{Imath::V2f(0, 1), Imath::V2f(1, 0)});
    owner.fields["color"] = uv;
    owner.fields["unrelated"] = uv;
    std::vector<Imath::V3f> colors;
    for (const auto& coordinate : uv->value())
        colors.emplace_back(coordinate[0], coordinate[1],
                            0); // Read before replacing the same name.
    assert(owner.getAttr("color") == uv);
    assert(dommy::color::publishValues(owner, "color", colors, 2));
    const auto written = std::dynamic_pointer_cast<const DPVec3fAttr>(owner.getAttr("color"));
    assert(written && sameValue(written->value()[1], Imath::V3f(1, 0, 0)));
    assert(owner.getAttr("unrelated") == uv && sameValue(uv->value()[0], Imath::V2f(0, 1)));
    assert(!dommy::color::publishValues(owner, "color", colors, 1));
    assert(!dommy::color::publishValues(owner, "", colors, 2));
    assert(owner.getAttr("color") == written);
    colors[1][2] = std::numeric_limits<float>::quiet_NaN();
    assert(!dommy::color::publishValues(owner, "color", colors, 2));
    assert(owner.getAttr("color") == written);
    const DPVec3fAttr invalidDefault(written->value(),
                                     Imath::V3f(0, 0, std::numeric_limits<float>::infinity()));
    assert(!dommy::color::publishAttribute(owner, "color", invalidDefault, 2));
    owner.ignoreWrite = true;
    assert(!dommy::color::publishValues(owner, "new", written->value(), 2));
    owner.ignoreWrite = false;
    owner.throwWrite = true;
    bool threw = false;
    try {
        dommy::color::publishValues(owner, "new", written->value(), 2);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw && owner.getAttr("color") == written && !owner.getAttr("new"));
    owner.throwWrite = false;
    const DPVec3dAttr doubles(std::vector<Imath::V3d>(2, Imath::V3d(1, 2, 3)));
    assert(dommy::color::publishAttribute(owner, "color", doubles, 2));
    assert(owner.getAttr("color")->getAttrType() == DP_ATTR_VEC3D);
    Owner empty;
    assert(dommy::color::publishValues(empty, "color", std::vector<Imath::V3f>(), 0));
}

void testNewExpressionAttributes() {
    using dommy::expression::createPointAttribute;
    Owner owner;
    owner.points.resize(3U, Imath::V3f(0, 0, 0));
    assert(createPointAttribute(owner, "scalar", 1U, 5.0f));
    const auto scalar = std::dynamic_pointer_cast<const DPFloatAttr>(owner.getAttr("scalar"));
    assert(scalar && scalar->value() == std::vector<float>({0, 5, 0}));
    assert(scalar->getDefaultValue() == 0.0f);
    assert(createPointAttribute(owner, "vector", 2U, Imath::V3f(1, 2, 3)));
    const auto vector = std::dynamic_pointer_cast<const DPVec3fAttr>(owner.getAttr("vector"));
    assert(vector && sameValue(vector->value()[0], Imath::V3f(0, 0, 0)));
    assert(sameValue(vector->value()[1], Imath::V3f(0, 0, 0)));
    assert(sameValue(vector->value()[2], Imath::V3f(1, 2, 3)));
    assert(sameValue(vector->getDefaultValue(), Imath::V3f(0, 0, 0)));
    assert(!createPointAttribute(owner, "scalar", 0U, 99.0f));
    assert(owner.getAttr("scalar") == scalar);
    assert(!createPointAttribute(owner, "invalid", 3U, 1.0f));
    assert(!createPointAttribute(owner, "", 0U, 1.0f));
    assert(!createPointAttribute(owner, "invalid", 0U, std::numeric_limits<float>::infinity()));
    assert(!createPointAttribute(owner, "invalid", 0U,
                                 Imath::V3f(1, std::numeric_limits<float>::quiet_NaN(), 3)));
    assert(!owner.getAttr("invalid"));
    owner.ignoreWrite = true;
    assert(!createPointAttribute(owner, "ignored", 0U, 1.0f));
    owner.ignoreWrite = false;
    owner.throwWrite = true;
    bool threw = false;
    try {
        createPointAttribute(owner, "throws", 0U, 1.0f);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw && !owner.getAttr("throws"));
    Owner empty;
    assert(!createPointAttribute(empty, "empty", 0U, 1.0f));
}

void testExactExpressionIntegerRead() {
    for (const int64_t input : {INT64_C(0), INT64_C(9007199254740991),
                               INT64_C(9007199254740992), INT64_C(9007199254740994),
                               INT64_C(-9007199254740994), INT64_MIN}) {
        double value = 0;
        assert(dommy::numeric::checkedScalarCast(input, value, true));
        int64_t restored = 0;
        assert(dommy::numeric::checkedScalarCast(value, restored, true) && restored == input);
    }
    for (const int64_t input : {INT64_C(9007199254740993), INT64_C(-9007199254740993),
                               INT64_MAX, INT64_MIN + INT64_C(1)}) {
        double value = 123.0;
        assert(!dommy::numeric::checkedScalarCast(input, value, true));
        assert(value == 123.0);
    }
}

void testBoundingColors() {
    using namespace dommy::color;
    const float limit = std::numeric_limits<float>::max();
    const std::vector<Imath::V3f> points{
        Imath::V3f(-limit, 0, 7), Imath::V3f(0, limit / 2, 7), Imath::V3f(limit, limit, 7)};
    Imath::V3d minimum(9, 8, 7), maximum(6, 5, 4);
    assert(pointBounds(points, minimum, maximum));
    assert(minimum[0] == -static_cast<double>(limit) && maximum[0] == limit);
    std::vector<Imath::V3f> result;
    assert(boundingColors(points, minimum, maximum, result));
    assert(sameValue(result[0], Imath::V3f(0, 0, 0)));
    assert(sameValue(result[1], Imath::V3f(0.5f, 0.5f, 0)));
    assert(sameValue(result[2], Imath::V3f(1, 1, 0)));
    for (std::size_t i = 0; i < points.size(); ++i) {
        Imath::V3f single(9, 8, 7);
        assert(normalizeBoundingColor(points[i], minimum, maximum, single));
        assert(sameValue(single, result[i])); // DP parallel and DM/DC array paths agree.
    }
    Imath::V3f color(9, 8, 7);
    assert(normalizeBoundingColor(Imath::V3d(1, 4, 7), Imath::V3d(0, 2, 7),
                                   Imath::V3d(2, 6, 7), color));
    assert(sameValue(color, Imath::V3f(0.5f, 0.5f, 0)));
    const double tiny = std::numeric_limits<float>::denorm_min();
    assert(normalizeBoundingColor(Imath::V3d(tiny, 0, 0), Imath::V3d(0, 0, 0),
                                   Imath::V3d(2 * tiny, 0, 0), color));
    assert(color[0] == 0.5f); // Nonzero tiny spans must not be treated as degenerate.
    assert(normalizeBoundingColor(Imath::V3d(-1, 2, 0), Imath::V3d(0, 0, 0),
                                   Imath::V3d(1, 1, 0), color));
    assert(sameValue(color, Imath::V3f(-1, 2, 0))); // Preserve extrapolation.
    const Imath::V3f previous = color;
    assert(!normalizeBoundingColor(Imath::V3d(0, 0, 0), maximum, minimum, color));
    assert(sameValue(color, previous));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    assert(!normalizeBoundingColor(Imath::V3d(0, nan, 0), minimum, maximum, color));
    assert(!normalizeBoundingColor(Imath::V3d(0, 0, 0), Imath::V3d(nan, 0, 0), maximum, color));
    assert(!normalizeBoundingColor(Imath::V3d(1, 0, 0), Imath::V3d(0, 0, 0),
                                    Imath::V3d(tiny, 0, 0), color)); // Float output overflow.
    auto invalid = points;
    invalid.back()[1] = std::numeric_limits<float>::infinity();
    assert(!boundingColors(invalid, minimum, maximum, result));
    assert(result.size() == 3U && sameValue(result[1], Imath::V3f(0.5f, 0.5f, 0)));
    assert(!pointBounds(invalid, minimum, maximum) && maximum[1] == limit);
    const std::vector<Imath::V3f> empty;
    assert(pointBounds(empty, minimum, maximum));
    assert(sameValue(minimum, Imath::V3d(0, 0, 0)) && sameValue(maximum, minimum));
    assert(boundingColors(empty, minimum, maximum, result) && result.empty());
}

struct ColorPrimitive {
    std::vector<Imath::V3f> points{
        Imath::V3f(0, 0, 0), Imath::V3f(1, 1, 1), Imath::V3f(2, 2, 2)};
    MMatrix matrix;
    const std::vector<Imath::V3f>* getPositionPtrConst() const { return &points; }
    MMatrix getTransformMatrix() const { return matrix; }
};

void testTransformedBoundingColors() {
    using namespace dommy::color;
    const auto primitive = std::make_shared<ColorPrimitive>();
    std::vector<std::shared_ptr<ColorPrimitive>> primitives{primitive};
    Imath::V3d minimum(0, 0, 0), maximum(0, 0, 0);
    std::vector<Imath::V3f> colors;
    const auto evaluate = [&]() {
        return transformedPointBounds(primitives.cbegin(), primitives.cend(), minimum, maximum) &&
               transformedBoundingColors(primitive->points, primitive->matrix,
                                           minimum, maximum, colors);
    };
    assert(evaluate());
    assert(sameValue(colors[0], Imath::V3f(0, 0, 0)));
    assert(sameValue(colors[1], Imath::V3f(0.5f, 0.5f, 0.5f)));
    assert(sameValue(colors[2], Imath::V3f(1, 1, 1)));

    primitive->matrix[3][0] = 10;
    assert(evaluate() && minimum[0] == 10 && maximum[0] == 12);
    assert(colors[0][0] == 0 && colors[1][0] == 0.5f && colors[2][0] == 1);
    primitive->matrix[3][0] = 16777217; // Do not round transformed positions to float.
    assert(evaluate() && minimum[0] == 16777217 && maximum[0] == 16777219);
    assert(colors[1][0] == 0.5f);

    primitive->matrix = MMatrix();
    primitive->matrix[0][0] = 0;
    primitive->matrix[0][1] = 1;
    primitive->matrix[1][0] = -1;
    primitive->matrix[1][1] = 0;
    assert(evaluate()); // 90-degree rotation; red reverses while green does not.
    assert(sameValue(colors[0], Imath::V3f(1, 0, 0)));
    assert(sameValue(colors[2], Imath::V3f(0, 1, 1)));
    primitive->matrix = MMatrix();
    primitive->matrix[0][0] = -2;
    primitive->matrix[1][1] = 3;
    primitive->matrix[2][2] = 0;
    assert(evaluate()); // Negative/nonuniform/zero scales.
    assert(sameValue(colors[0], Imath::V3f(1, 0, 0)));
    assert(sameValue(colors[1], Imath::V3f(0.5f, 0.5f, 0)));
    assert(sameValue(colors[2], Imath::V3f(0, 1, 0)));
    primitive->matrix[0][0] = 1e40; // Valid double world coordinates beyond float range.
    assert(evaluate() && maximum[0] == 2e40 && colors[1][0] == 0.5f);

    primitive->points[0][0] = -1; primitive->points[1][0] = 0; primitive->points[2][0] = 1;
    primitive->matrix[0][0] = 1e308;
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(evaluate() && minimum[0] == -1e308 && maximum[0] == 1e308);
    assert(colors[0][0] == 0 && colors[1][0] == 0.5f && colors[2][0] == 1);
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
    primitive->points[0][0] = 0; primitive->points[1][0] = 1; primitive->points[2][0] = 2;

    primitive->matrix = MMatrix();
    primitive->matrix[3][0] = 10;
    const auto second = std::make_shared<ColorPrimitive>();
    second->matrix[3][0] = 14;
    primitives.push_back(second);
    assert(evaluate() && minimum[0] == 10 && maximum[0] == 16);
    assert(std::abs(colors[2][0] - 1.0f / 3.0f) < 1e-6f);
    std::vector<Imath::V3f> secondColors;
    assert(transformedBoundingColors(second->points, second->matrix, minimum, maximum, secondColors));
    assert(std::abs(secondColors[0][0] - 2.0f / 3.0f) < 1e-6f && secondColors[2][0] == 1);
    assert(sameValue(primitive->points[0], Imath::V3f(0, 0, 0)) &&
           sameValue(primitive->points[2], Imath::V3f(2, 2, 2)) && primitive->matrix[3][0] == 10);

    const Imath::V3d previousMin = minimum, previousMax = maximum;
    const auto previousColors = secondColors;
    second->points.back()[2] = std::numeric_limits<float>::infinity();
    assert(!transformedPointBounds(primitives.cbegin(), primitives.cend(), minimum, maximum));
    assert(sameValue(minimum, previousMin) && sameValue(maximum, previousMax));
    assert(!transformedBoundingColors(second->points, second->matrix, minimum, maximum, secondColors));
    assert(secondColors.size() == previousColors.size() && sameValue(secondColors[0], previousColors[0]));
    second->points.back()[2] = 2;
    second->matrix[0][0] = std::numeric_limits<double>::max();
    assert(!transformedPointBounds(primitives.cbegin(), primitives.cend(), minimum, maximum));
    assert(!transformedBoundingColors(second->points, second->matrix, minimum, maximum, secondColors));
    second->matrix[0][0] = std::numeric_limits<double>::quiet_NaN();
    assert(!transformedPointBounds(primitives.cbegin(), primitives.cend(), minimum, maximum));
    primitives.back().reset();
    assert(!transformedPointBounds(primitives.cbegin(), primitives.cend(), minimum, maximum));
    primitives.clear();
    assert(transformedPointBounds(primitives.cbegin(), primitives.cend(), minimum, maximum));
    assert(sameValue(minimum, Imath::V3d(0, 0, 0)) && sameValue(maximum, minimum));
    assert(transformedBoundingColors(std::vector<Imath::V3f>(), MMatrix(), minimum, maximum, colors));
    assert(colors.empty());
}

template <typename Value> void testNumericSortType() {
    using Components = dommy::numeric::ValueComponents<Value>;
    std::vector<Value> values(4U);
    const int keys[] = {3, 1, 3, 2};
    for (unsigned int axis = 0U; axis < Components::count; ++axis) {
        for (std::size_t i = 0U; i < values.size(); ++i)
            Components::get(values[i], axis) = static_cast<typename Components::Scalar>(keys[i]);
        const auto source = std::make_shared<TypedDPAttributeData<Value>>(values);
        std::vector<std::size_t> indices(4U);
        assert(dommy::attribute::sortAttributeIndices(source, axis, true, indices));
        assert(indices == std::vector<std::size_t>({0, 2, 3, 1}));
        assert(dommy::attribute::sortAttributeIndices(source, axis, false, indices));
        assert(indices == std::vector<std::size_t>({1, 3, 0, 2}));
    }
}

void testAttributeSort() {
    using namespace dommy::attribute;
    testNumericSortType<int32_t>();
    testNumericSortType<uint32_t>();
    testNumericSortType<int64_t>();
    testNumericSortType<float>();
    testNumericSortType<double>();
    testNumericSortType<Imath::V2i>();
    testNumericSortType<Imath::V2f>();
    testNumericSortType<Imath::V2d>();
    testNumericSortType<Imath::V3i>();
    testNumericSortType<Imath::V3f>();
    testNumericSortType<Imath::V3d>();
    const auto source = std::make_shared<DPInt64Attr>(std::vector<int64_t>{
        9007199254740992LL, 9007199254740993LL, INT64_MIN, INT64_MAX, INT64_MAX - 1});
    std::vector<std::size_t> indices(5U);
    assert(sortAttributeIndices(source, 0, true, indices));
    assert(indices == std::vector<std::size_t>({3, 4, 1, 0, 2}));
    assert(sortAttributeIndices(source, 0, false, indices));
    assert(indices == std::vector<std::size_t>({2, 0, 1, 4, 3}));
    const auto boolean = std::make_shared<DPBoolAttr>(std::vector<bool>{true, false, true, false});
    indices.resize(4U);
    assert(sortAttributeIndices(boolean, 2, true, indices));
    assert(indices == std::vector<std::size_t>({0, 2, 1, 3}));
    const auto vector =
        std::make_shared<DPVec2dAttr>(std::vector<Imath::V2d>(4U, Imath::V2d(1, 2)));
    const auto before = indices;
    assert(!sortAttributeIndices(vector, 2, true, indices) && indices == before);
    assert(!sortAttributeIndices(vector, -1, true, indices));
    const auto bad = std::make_shared<DPDoubleAttr>(
        std::vector<double>{1, 2, 3, std::numeric_limits<double>::quiet_NaN()});
    assert(!sortAttributeIndices(bad, 0, true, indices) && indices == before);
    assert(!sortAttributeIndices(source, 0, true, indices));
    assert(!sortAttributeIndices(DPAttributeData::ConstPtr(), 0, true, indices));
    assert(!sortAttributeIndices(std::make_shared<DPStringAttr>(std::vector<std::string>(4U, "a")),
                                 0, true, indices));
    indices.clear();
    assert(sortAttributeIndices(std::make_shared<DPInt64Attr>(std::vector<int64_t>()), 0, true,
                                indices));
}

} // namespace

void runExpressionSortTests() {
    testWorldCenterNodeChain();
    testStagedMayaArrayBuild();
    testPrimitiveExplode();
    testWorldPivotTransform();
    testWideExponentPointMean();
    testGroupPatternBoundary();
    testMeshDeformationInfo();
    testExactIntegerBlends();
    testFloatingBlendEndpoints<float>();
    testFloatingBlendEndpoints<double>();
    testCommonWorldPointPrecision();
    testExtremeAffineProductSums();
    testNormalAndScaledProductExtremes();
    testTransformedBoundingColors();
    testNewExpressionAttributes();
    testExactExpressionIntegerRead();
    testBoundingColors();
    testEvaluationBoundaries();
    testColorPublication();
    testBlendPublication();
    testCallIsolation();
    testExpressionResults();
    testAttributeSort();
}
