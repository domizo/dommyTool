#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DCurveConnection.h"
#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DCurveBatchBudget.h"
#include "modules/curve/DCurve/Utility/DCurveDuplicatePlan.h"
#include "modules/curve/DCurve/Utility/DCurveReplacementPlan.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DPrimitiveCopyBudget.h"
#include "DDataUtility/Data/DPGroupData.h"
#include <maya/MMatrix.h>

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

using DPoint = std::array<float, 3>;
using DPointList = std::vector<DPoint>;
using DPIndexList = std::vector<std::size_t>;
using DStringList = std::vector<std::string>;
using DMatrixArray = std::vector<MMatrix>;
struct MPoint {
    std::array<double, 3> values;
    MPoint(double x = 0, double y = 0, double z = 0) : values{{x,y,z}} {}
    double& operator[](std::size_t index) { return values.at(index); }
    double operator[](std::size_t index) const { return values.at(index); }
};
struct MPointArray {
    static int appends, failAt;
    std::vector<MPoint> points;
    unsigned int length() const { return static_cast<unsigned int>(points.size()); }
    MPoint& operator[](unsigned int i) { return points.at(i); }
    const MPoint& operator[](unsigned int i) const { return points.at(i); }
    MStatus append(const MPoint& point) {
        if (++appends == failAt) return MS::kFailure;
        points.push_back(point); return MS::kSuccess;
    }
};
int MPointArray::appends = 0;
int MPointArray::failAt = 0;
struct Ramp { float value = 0.5f; int failAt = 0, calls = 0; };
namespace dommy { namespace maya {
bool isFloatRepresentablePoint(const MPoint& point) {
    return numeric::canRepresentAsFloat(point[0]) && numeric::canRepresentAsFloat(point[1]) &&
        numeric::canRepresentAsFloat(point[2]);
}
bool sampleFiniteRamp(Ramp& ramp, float, float& value) {
    value = ramp.value; return ++ramp.calls != ramp.failAt && std::isfinite(value);
}
} }
struct DCurveInternalData {
    static int replacements, failReplacementAt, copies, throwCopyAt;
    DPointList points;
    MMatrix matrix;
    int metadata = 17;
    DPAttrList pointAttributes;
    DPGrpList pointGroups;
    explicit DCurveInternalData(const DPointList& value) : points(value) {}
    DCurveInternalData(const DCurveInternalData& other)
        : points(other.points), matrix(other.matrix), metadata(other.metadata) {
        if (++copies == throwCopyAt) throw std::bad_alloc();
        for (const auto& field : other.pointAttributes)
            pointAttributes.emplace(field.first, field.second->copy());
        for (const auto& group : other.pointGroups)
            pointGroups.emplace(group.first, group.second->copy());
    }
    DCurveInternalData(const MPointArray& value, int, bool) {
        for (const MPoint& point : value.points) {
            DPoint converted;
            for (unsigned int axis = 0U; axis < 3U; ++axis)
                if (!dommy::numeric::checkedFloatCast(point[axis], converted[axis]))
                    throw std::invalid_argument("float output");
            points.push_back(converted);
        }
    }
    bool hasValidCurveTopology() const {
        dommy::attribute::ValidationBudget budget(points.size());
        if (points.size() < 2U ||
            !dommy::attribute::validateAttributeList(pointAttributes, points.size(), budget))
            return false;
        for (const auto& group : pointGroups)
            if (!group.second || group.second->getGroupIds().size() != points.size())
                return false;
        return true;
    }
    std::size_t numberOfPoints() const { return points.size(); }
    std::size_t numberOfKnots() const { return points.size(); }
    ConstDPAttrIter beginAttr() const { return pointAttributes.begin(); }
    ConstDPAttrIter endAttr() const { return pointAttributes.end(); }
    ConstDPGrpIter beginGrp() const { return pointGroups.begin(); }
    ConstDPGrpIter endGrp() const { return pointGroups.end(); }
    bool appendOperationCost(dommy::curve::CurveBatchBudget& budget) const;
    DPointList getPositionConst() const { return points; }
    const DPointList* getPositionPtrConst() const { return &points; }
    MMatrix getTransformMatrix() const { return matrix; }
    void setTransformMatrix(const MMatrix& value) { matrix = value; }
    bool replacePositions(const DPointList& value) {
        if (++replacements == failReplacementAt) return false;
        points = value; return true;
    }
    static void reset() { replacements = failReplacementAt = copies = throwCopyAt = 0; }
};
int DCurveInternalData::replacements = 0;
int DCurveInternalData::failReplacementAt = 0;
int DCurveInternalData::copies = 0;
int DCurveInternalData::throwCopyAt = 0;
using DCItem = std::shared_ptr<DCurveInternalData>;
struct Group {
    std::vector<bool> ids;
    const std::vector<bool>& getGroupIds() const { return ids; }
    void getActiveIndexList(DPIndexList& result) const {
        for (std::size_t i = 0; i < ids.size(); ++i) if (ids[i]) result.push_back(i);
    }
};
struct Attribute { virtual ~Attribute() {} };
template<typename T> struct TypedAttribute : Attribute {
    std::vector<T> values;
    explicit TypedAttribute(const std::vector<T>& input) : values(input) {}
    const std::vector<T>& value() const { return values; }
};
struct DCurveData {
    using DCurveList = std::vector<DCItem>;
    DCurveList curves;
    std::map<std::string, std::shared_ptr<Group>> groups;
    std::map<std::string, std::shared_ptr<Attribute>> attributes;
    template<typename T>
    std::shared_ptr<const TypedAttribute<T>> getValidTypedAttributedata(const std::string& name) const {
        const auto found = attributes.find(name);
        return found == attributes.end() ? std::shared_ptr<const TypedAttribute<T>>() :
            std::dynamic_pointer_cast<const TypedAttribute<T>>(found->second);
    }
    bool failInsert = false;
    int inserts = 0;
    std::size_t numberOfCurves() const { return curves.size(); }
    const DCurveInternalData& getCurve(std::size_t i) const { return *curves.at(i); }
    bool isExistGrp(const std::string& name) const { return groups.count(name) != 0U; }
    std::shared_ptr<const Group> getGrpPtr(const std::string& name) const { return groups.at(name); }
    bool insert(const DCurveList& value) {
        ++inserts; if (failInsert) return false; curves = value; return true;
    }
};
enum { DC_kLinear, DC_kCubic };
enum { DC_kClose, DC_kOpen };
#include "curve_world_methods.inc"

DCItem line(float first, float last)
{
    return DCItem(new DCurveInternalData(DPointList{{{first,0,0}}, {{last,0,0}}}));
}

void testReplacementPlanAndFailures()
{
    DCurveData target; target.curves = {line(1,2), line(3,4)};
    target.groups["both"] = std::make_shared<Group>();
    target.groups["both"]->ids = {true,true};
    dommy::curve::CurveReplacementPlan plan;
    assert(resolve_replacements(target, {"index:1", "both", "missing", "index:99"}, plan));
    assert(plan.curves.size() == 3 && plan.points == 6);
    assert(plan.curves[0].source == 0 && plan.curves[0].target == 1);
    assert(plan.curves[1].source == 1 && plan.curves[1].target == 0);
    assert(plan.curves[2].source == 1 && plan.curves[2].target == 1);
    for (const std::string& bad : {"index:", "index:-1", "index:1junk", "index:9999999999999999999999999"}) {
        assert(!resolve_replacements(target, {bad}, plan));
        assert(plan.curves.size() == 3 && plan.points == 6);
    }
    DPIndexList indices{0,1};
    const auto resolve = [&](std::size_t, const DPIndexList*& result, std::size_t&) { result = &indices; return true; };
    assert(!dommy::curve::buildCurveReplacementPlan(1, 2, resolve,
        [](std::size_t) { return std::size_t(6000000); }, plan));
    assert(plan.points == 6);
    indices.assign(1000001, 0);
    assert(!dommy::curve::buildCurveReplacementPlan(1, 2, resolve,
        [](std::size_t) { return std::size_t(2); }, plan));
    indices = {0};
    assert(!dommy::curve::buildCurveReplacementPlan(1, 2,
        [&](std::size_t, const DPIndexList*& result, std::size_t& work) {
            result = &indices; work = dommy::generation::kMaxWorkItemCount + 1; return true;
        }, [](std::size_t) { return std::size_t(2); }, plan));
    assert(plan.points == 6);

    DCurveData input; input.curves = {line(0,1), line(0,1)};
    assert(validate_metadata(&input) == MS::kInvalidParameter);
    input.attributes["src_path"] = std::make_shared<TypedAttribute<std::string>>(DStringList{"index:0", "both"});
    assert(validate_metadata(&input) == MS::kInvalidParameter);
    input.attributes["restmat"] = input.attributes["src_path"];
    assert(validate_metadata(&input) == MS::kInvalidParameter);
    input.attributes["restmat"] = std::make_shared<TypedAttribute<MMatrix>>(DMatrixArray(1));
    assert(validate_metadata(&input) == MS::kInvalidParameter);
    input.attributes["restmat"] = std::make_shared<TypedAttribute<MMatrix>>(DMatrixArray(2));
    assert(validate_metadata(&input) == MS::kSuccess);
    input.attributes["src_path"] = input.attributes["restmat"];
    assert(validate_metadata(&input) == MS::kInvalidParameter);
    input.curves[0]->matrix[3][0] = 100;
    input.curves[1]->matrix[3][0] = 200;
    DMatrixArray rest(2); rest[0][3][0] = 1; rest[1][3][0] = 2;
    DCurveData output;
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kSuccess);
    assert(output.curves.size() == 3 && output.inserts == 1);
    assert(output.curves[0]->points[0][0] == 2 && output.curves[0]->matrix[3][0] == 100);
    assert(output.curves[1]->points[0][0] == -1 && output.curves[1]->matrix[3][0] == 200);
    assert(output.curves[0]->metadata == 17 && target.curves[1]->points[0][0] == 3);
    const DCItem sentinel = line(77,78);
    for (int failed = 1; failed <= 3; ++failed) {
        DCurveInternalData::reset(); DCurveInternalData::failReplacementAt = failed;
        output = DCurveData(); output.curves = {sentinel};
        assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kFailure);
        assert(output.curves[0] == sentinel && output.inserts == 0);
        DCurveInternalData::reset(); DCurveInternalData::throwCopyAt = failed;
        assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kFailure);
        assert(output.curves[0] == sentinel && output.inserts == 0);
    }
    DCurveInternalData::reset(); output.failInsert = true;
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kFailure);
    assert(output.curves[0] == sentinel);
    output.failInsert = false; rest[1][0][3] = 1;
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kInvalidParameter);
    assert(output.curves[0] == sentinel);
}

void testReplacementAdmission()
{
    DCurveData input, target, output;
    input.curves = {line(0,1), line(0,1)};
    target.curves = {line(1,2)};
    const DCItem sentinel = line(77,78);
    output.curves = {sentinel};
    DMatrixArray rest(2);
    dommy::curve::CurveReplacementPlan plan;
    assert(resolve_replacements(target, {"index:0", "index:0"}, plan));
    auto& fields = target.curves[0]->pointAttributes;
    fields["s"] = std::make_shared<DPStringAttr>(
        std::vector<std::string>(2, std::string(50U, 'x')), std::string(20U, 'd'));
    assert(target.curves[0]->hasValidCurveTopology());
    DCurveInternalData::reset();
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kInvalidParameter);
    assert(DCurveInternalData::copies == 0 && DCurveInternalData::replacements == 0);
    assert(output.curves == DCurveData::DCurveList{sentinel} && output.inserts == 0);

    // Defaults repeat per primitive too, even when every value is small.
    fields["s"] = std::make_shared<DPStringAttr>(
        std::vector<std::string>(2, "x"), std::string(80U, 'd'));
    DCurveInternalData::reset();
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kInvalidParameter);
    assert(DCurveInternalData::copies == 0 && output.inserts == 0);

    fields.clear();
    for (int index = 0; index < 8; ++index)
        fields[std::string(1, char('a' + index))] = std::make_shared<DPMat4Attr>(DMatrixArray(2));
    DCurveInternalData::reset();
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kInvalidParameter);
    assert(DCurveInternalData::copies == 0 && output.inserts == 0);

    fields.clear();
    target.curves[0]->points.assign(50U, DPoint{{1,0,0}});
    target.curves[0]->pointGroups["g"] = std::make_shared<DPGroupData>(DPIdList(50U, true));
    input.curves.push_back(line(0,1)); rest.resize(3);
    assert(resolve_replacements(target, {"index:0", "index:0", "index:0"}, plan));
    DCurveInternalData::reset();
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kInvalidParameter);
    assert(DCurveInternalData::copies == 0 && output.inserts == 0);

    // Validate every selected source frame before any earlier clone starts.
    target.curves[0]->pointGroups.clear(); target.curves[0]->points.resize(2U);
    rest[2][0][3] = 1.0;
    DCurveInternalData::reset();
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kInvalidParameter);
    assert(DCurveInternalData::copies == 0 && DCurveInternalData::replacements == 0);
    rest[2] = MMatrix();
    input.curves[2]->matrix[0][3] = 1.0;
    DCurveInternalData::reset();
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kInvalidParameter);
    assert(DCurveInternalData::copies == 0 && DCurveInternalData::replacements == 0);
    input.curves[2]->matrix = MMatrix();

    fields["s"] = std::make_shared<DPStringAttr>(std::vector<std::string>(2, "x"), "default");
    target.curves[0]->pointGroups["g"] = std::make_shared<DPGroupData>(DPIdList{true,false});
    DCurveInternalData::reset();
    assert(apply_replacements(&input, &target, &rest, plan, &output) == MS::kSuccess);
    assert(output.curves.size() == 3U && output.inserts == 1 && DCurveInternalData::copies == 3);
    for (const auto& curve : output.curves) {
        const auto field = std::dynamic_pointer_cast<const DPStringAttr>(curve->pointAttributes.at("s"));
        assert(field->value() == std::vector<std::string>({"x", "x"}) && field->defaultValue() == "default");
        assert(field != target.curves[0]->pointAttributes.at("s"));
        assert(curve->pointGroups.at("g") != target.curves[0]->pointGroups.at("g"));
        assert(curve->pointGroups.at("g")->getGroupIds() == DPIdList({true,false}));
    }
}

void testWorldTransformsAndConnections()
{
    MMatrix matrix; matrix[0][0] = 0; matrix[0][1] = 2;
    matrix[1][0] = -3; matrix[1][1] = 0; matrix[3][0] = 100;
    std::array<double, 3> direction;
    assert(dommy::numeric::checkedTransformDirection(DPoint{{1,0,0}}, matrix, direction));
    assert((direction == std::array<double, 3>{{0,2,0}}));
    assert(dommy::curve::normalizeDirection(direction));
    assert((direction == std::array<double, 3>{{0,1,0}}));
    DCurveData input; input.curves = {line(0,1)};
    DMatrixArray matrices(1, matrix);
    std::vector<dommy::curve::CurveChain> chains{{{0,false}}};
    DCurveData output;
    assert(emit_connections(&input, matrices, chains, &output) == MS::kSuccess);
    assert((output.curves[0]->points[0] == DPoint{{100,0,0}}));
    assert((output.curves[0]->points[1] == DPoint{{100,2,0}}));
    assert(output.curves[0]->matrix[3][0] == 0); // Baked world points, identity output.
    assert(input.curves[0]->points[0][0] == 0);

    input.curves = {line(0,1), line(0,1)};
    matrices.assign(2, MMatrix()); matrices[0][3][0] = 100; matrices[1][3][0] = 101;
    std::vector<dommy::curve::ConnectionEndpoints> endpoints(2);
    for (std::size_t i = 0; i < 2; ++i) {
        for (unsigned int end = 0; end < 2; ++end) {
            assert(dommy::numeric::checkedTransformPoint(input.curves[i]->points[end], matrices[i], endpoints[i].points[end]));
            assert(dommy::numeric::checkedTransformDirection(DPoint{{1,0,0}}, matrices[i], endpoints[i].tangents[end]));
        }
    }
    assert(dommy::curve::buildCurveConnectionChains(endpoints, 0, true, 0.1, chains));
    assert(chains.size() == 1);
    assert(emit_connections(&input, matrices, chains, &output) == MS::kSuccess);
    assert(output.curves[0]->points.size() == 3 && output.curves[0]->points[0][0] == 100 && output.curves[0]->points[2][0] == 102);
    for (int fail = 1; fail <= 3; ++fail) {
        MPointArray::appends = 0; MPointArray::failAt = fail;
        const auto previous = output.curves;
        assert(emit_connections(&input, matrices, chains, &output) == MS::kFailure);
        assert(output.curves == previous);
    }
    MPointArray::failAt = 0;
    matrices[1][3][0] = std::numeric_limits<double>::max();
    const auto previous = output.curves;
    assert(emit_connections(&input, matrices, chains, &output) == MS::kFailure && output.curves == previous);
}

void testWorldDeletion()
{
    std::vector<DPointList> points{{{{0,0,0}}, {{1,0,0}}}, {{{0,0,0}}, {{1,0,0}}}};
    DMatrixArray matrices(2); matrices[1][3][0] = 100;
    std::vector<bool> flags;
    const auto build = [&]() { return dommy::curve::buildWorldLineDeletionPlan(points.size(),
        [&](std::size_t i) -> const DPointList& { return points.at(i); },
        [&](std::size_t i) { return matrices.at(i); }, flags); };
    assert(build() && !flags[0] && !flags[1]); // Same local line, distinct placement.
    points[1] = {{{-100,0,0}}, {{-99,0,0}}};
    assert(build() && flags[0] && !flags[1]); // Different local line, same world line.
    std::reverse(points[1].begin(), points[1].end());
    assert(build() && flags[0] && !flags[1]);
    points[1] = {{{0,0,0}}, {{1,0,0}}}; matrices[1] = MMatrix(); matrices[1][0][0] = -1; matrices[1][3][0] = 1;
    assert(build() && flags[0] && !flags[1]); // Reflection reverses the segment.
    points[1].push_back(DPoint{{2,0,0}});
    assert(build() && !flags[0] && !flags[1]); // Non-line curves remain untouched.
    matrices[0][0][3] = 1;
    assert(!build() && !flags[0] && !flags[1]);
    matrices[0] = MMatrix(); matrices[0][0][0] = std::numeric_limits<double>::infinity();
    assert(!build());
    bool called = false;
    assert(!dommy::curve::buildWorldLineDeletionPlan(20000,
        [&](std::size_t) -> const DPointList& { called = true; return points[0]; },
        [&](std::size_t) { return MMatrix(); }, flags));
    assert(!called);
}

void testStableClumping()
{
    const DPointList source{{{1,2,3}}, {{3.0e38f,0,0}}, {{3.0e38f,0,0}}};
    const DPointList target{{{9,9,9}}, {{-3.0e38f,0,0}}, {{-3.0e38f,0,0}}};
    DPointList output; Ramp ramp;
    assert(clump_points(source, target, 1, ramp, output) == MS::kSuccess);
    assert(output[0] == source[0] && output[1][0] == 0 && output[2][0] == 0);
    ramp = Ramp();
    assert(clump_points(source, target, 0, ramp, output) == MS::kSuccess && output == source);
    const DPointList previous = output;
    ramp = Ramp(); ramp.failAt = 2;
    assert(clump_points(source, target, 1, ramp, output) == MS::kFailure && output == previous);
    ramp = Ramp(); ramp.value = 1;
    assert(clump_points(source, target, 1, ramp, output) == MS::kSuccess && output[1] == target[1]);
    const DPointList accepted = output;
    ramp = Ramp(); ramp.value = 1;
    assert(clump_points(source, target, 2, ramp, output) == MS::kInvalidParameter && output == accepted);
    ramp = Ramp(); ramp.value = std::numeric_limits<float>::quiet_NaN();
    assert(clump_points(source, target, 1, ramp, output) == MS::kFailure && output == accepted);
}

int main()
{
    testReplacementPlanAndFailures(); testReplacementAdmission();
    testWorldTransformsAndConnections(); testWorldDeletion(); testStableClumping();
    std::cout << "Curve world-space/replacement executable checks passed\n";
}
