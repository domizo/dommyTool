#include "common/utility/DCurveEvaluation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DCurveSampling.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DCurveBatchBudget.h"
#include <maya/MMatrix.h>

#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Only API shape/control flow is modeled. Real frame rotations are a Maya gate.
struct MVector {
    std::array<double, 3> values;
    MVector(double x = 0, double y = 0, double z = 0) : values{{x,y,z}} {}
    double& operator[](std::size_t i) { return values.at(i); }
    double operator[](std::size_t i) const { return values.at(i); }
    MVector operator^(const MVector& rhs) const {
        return MVector(values[1]*rhs[2]-values[2]*rhs[1],
                       values[2]*rhs[0]-values[0]*rhs[2], values[0]*rhs[1]-values[1]*rhs[0]);
    }
    bool operator==(const MVector& rhs) const { return values == rhs.values; }
    bool operator!=(const MVector& rhs) const { return !(*this == rhs); }
    MVector operator+(const MVector& rhs) const { return MVector(values[0]+rhs[0], values[1]+rhs[1], values[2]+rhs[2]); }
    MVector operator*(double scale) const { return MVector(values[0]*scale, values[1]*scale, values[2]*scale); }
};
using MPoint = MVector;
MPoint DPToMP(const MPoint& value) { return value; }
namespace MSpace { enum Space { kObject }; }
namespace dommy { namespace maya {
bool isFiniteVector(const MVector& value) { return curve::finiteTriple(value); }
bool isFloatRepresentablePoint(const MPoint& value) {
    return numeric::canRepresentAsFloat(value[0]) && numeric::canRepresentAsFloat(value[1]) &&
        numeric::canRepresentAsFloat(value[2]);
}
} }
struct MString {
    const char* asChar() const { return "attribute"; }
};
struct Value {
    double scalar = 1;
    int integer = 0;
    bool flag = false;
    MVector vector{1,0,0};
    std::array<double, 2> pair{{0,1}};
};
struct MDataHandle {
    Value value;
    bool valid;
    int* handleReads;
    void read() const { assert(valid); ++*handleReads; }
    double asDouble() const { read(); return value.scalar; }
    float asFloat() const { read(); return static_cast<float>(value.scalar); }
    const double* asDouble2() const { read(); return value.pair.data(); }
    short asShort() const { read(); return static_cast<short>(value.integer); }
    int asInt() const { read(); return value.integer; }
    bool asBool() const { read(); return value.flag; }
    MVector asVector() const { read(); return value.vector; }
    MString asString() const { read(); return MString(); }
};
struct MDataBlock {
    int failAt = 0, calls = 0, handleReads = 0;
    std::map<int, Value> values;
    MDataHandle inputValue(int attribute, MStatus* status) {
        const bool valid = ++calls != failAt;
        *status = valid ? MS::kSuccess : MS::kFailure;
        return MDataHandle{values[attribute], valid, &handleReads};
    }
};
enum {
    aWeightAttriute, aWeight, aRadius, aArcStep, aSpiralOffset, aRadiusAttribute,
    aArcStepAttribute, aSpiralOffsetAttribute, aDirectionType, aOffsetVector,
    aOffsetVectorAttribute, aUseAimVector, aAimVector, aUpVector, aDsitance,
    aSubdivide, aCheckOverlap, aTolerance,
    aParamType, aBaseParam, aParamAttributeName, aDistance, aOffset, aWrapMode,
    aWeightAttributeName, aWeightScale, aRepeatWeight, aSampling, aRange,
    aStartLength, aEndLength, aStartInvert, aEndInvert, aAnotherCurve,
    aIsDeleteOriginal, aStartOffset, aEndOffset
};
enum { DC_VEC_kTangent, DC_VEC_kNormal };
#define CHECK_MSTATUS_AND_RETURN_IT(status) do { if ((status) != MS::kSuccess) return status; } while (false)

struct DCurveInternalData {
    std::size_t count = 2;
    bool valid = true;
    std::size_t numberOfPoints() const { return count; }
    bool hasValidCurveTopology() const { return valid; }
    bool appendOperationCost(dommy::curve::CurveBatchBudget& budget) const {
        return hasValidCurveTopology() && budget.append(count,count,0U,0U);
    }
};
using DCItem = std::shared_ptr<DCurveInternalData>;
struct DCurveData {
    using DCurveList = std::vector<DCItem>;
    DCurveList curves;
    bool fail = false;
    int calls = 0;
    bool insert(const DCurveList& input) {
        ++calls;
        if (fail) return false;
        curves = input;
        return true;
    }
};
struct PointCount {
    std::size_t count;
    std::size_t length() const { return count; }
};
#include "curve_deformation_methods.inc"

struct Curve {
    int failure = 0, throwAt = 0, closestCalls = 0, tangentCalls = 0, normalCalls = 0;
    double parameter = 0.25;
    MVector closest{0,0,0}, tangentValue{0,1,0}, normalValue{1,0,0};
    void query(int id, MStatus* status) {
        if (throwAt == id) throw std::runtime_error("Injected curve query exception");
        *status = failure == id ? MS::kFailure : MS::kSuccess;
    }
    MVector closestPoint(const MVector&, double* result, double, int, MStatus* status) {
        ++closestCalls;
        query(1, status);
        if (*status == MS::kSuccess) *result = parameter;
        return closest;
    }
    MVector tangent(double param, int, MStatus* status) {
        ++tangentCalls; assert(param == parameter); query(2, status); return tangentValue;
    }
    MVector normal(double param, int, MStatus* status) {
        ++normalCalls; assert(param == parameter); query(3, status); return normalValue;
    }
};
void testFrames() {
    const MVector point(0,2,0), untouched(7,8,9);
    MVector tangent = untouched, normal = untouched, binormal = untouched;
    for (int query = 1; query <= 3; ++query) {
        Curve curve; curve.failure = query;
        assert(dommy::curve::frameAtClosestPoint(curve, point, static_cast<const MVector*>(nullptr),
            1e-6, 0, tangent, normal, binormal) == MS::kFailure);
        assert(tangent == untouched && normal == untouched && binormal == untouched);
        if (query == 1) assert(curve.tangentCalls == 0 && curve.normalCalls == 0);
        if (query == 2) assert(curve.normalCalls == 0);
        curve.failure = 0; curve.throwAt = query;
        bool threw = false;
        try { (void)dommy::curve::frameAtClosestPoint(curve, point, static_cast<const MVector*>(nullptr),
            1e-6, 0, tangent, normal, binormal); } catch (const std::runtime_error&) { threw = true; }
        assert(threw && tangent == untouched && normal == untouched && binormal == untouched);
    }
    Curve curve;
    curve.parameter = std::numeric_limits<double>::quiet_NaN();
    assert(dommy::curve::frameAtClosestPoint(curve, point, static_cast<const MVector*>(nullptr),
        1e-6, 0, tangent, normal, binormal) == MS::kInvalidParameter);
    assert(curve.tangentCalls == 0);
    curve.parameter = 0.25;
    for (MVector bad : {MVector(), MVector(0,1,0), MVector(1e-15,0,0),
                       MVector(std::numeric_limits<double>::infinity(),0,0)}) {
        curve.normalValue = bad;
        assert(dommy::curve::frameAtClosestPoint(curve, point, static_cast<const MVector*>(nullptr),
            1e-6, 0, tangent, normal, binormal) == MS::kInvalidParameter);
        assert(tangent == untouched && normal == untouched && binormal == untouched);
    }
    curve.normalValue = MVector(1,0,0);
    assert(dommy::curve::frameAtClosestPoint(curve, point, static_cast<const MVector*>(nullptr),
        1e-6, 0, tangent, normal, binormal) == MS::kSuccess);
    assert(tangent == MVector(0,1,0) && normal == MVector(1,0,0) && binormal == MVector(0,0,-1));
    const int normalCalls = curve.normalCalls;
    assert(dommy::curve::frameAtClosestPoint(curve, point, &binormal,
        1e-6, 0, tangent, normal, binormal) == MS::kSuccess);
    assert(curve.normalCalls == normalCalls && binormal == MVector(0,0,-1));
    MVector huge(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), 0);
    assert(dommy::curve::normalizeDirection(huge) && std::fabs(huge[0]*huge[0]+huge[1]*huge[1]-1) < 1e-12);
}
void testInputs() {
    using Reader = MStatus (*)(MDataBlock&);
    const Reader readers[] = {read_curl, read_offset, read_weight, read_stitch, read_extend};
    const int counts[] = {8, 10, 9, 3, 9};
    for (int node = 0; node != 5; ++node) {
        const auto read = readers[node];
        const int count = counts[node];
        for (int failure = 1; failure <= count; ++failure) {
            MDataBlock data; data.failAt = failure;
            const int extra = node == 3 && failure > 2 ? 1 : 0;
            assert(read(data) == MS::kFailure && data.calls == failure && data.handleReads == failure-1+extra);
        }
        MDataBlock data;
        assert(read(data) == MS::kSuccess && data.calls == count && data.handleReads == count+(node == 3 ? 1 : 0));
    }
    for (int attribute : {aWeight, aRadius, aArcStep, aSpiralOffset}) {
        MDataBlock data; data.values[attribute].scalar = std::numeric_limits<double>::infinity();
        assert(read_curl(data) == MS::kInvalidParameter);
    }
    MDataBlock badMode; badMode.values[aDirectionType].integer = 3;
    assert(read_offset(badMode) == MS::kInvalidParameter);
    MDataBlock badSubdivide; badSubdivide.values[aSubdivide].integer = -1;
    assert(read_offset(badSubdivide) == MS::kInvalidParameter);
    MDataBlock badTolerance; badTolerance.values[aTolerance].scalar = 0;
    assert(read_offset(badTolerance) == MS::kInvalidParameter);
    MDataBlock badVector; badVector.values[aOffsetVector].vector[0] = std::numeric_limits<double>::quiet_NaN();
    assert(read_offset(badVector) == MS::kInvalidParameter);
    MDataBlock badWrap; badWrap.values[aWrapMode].integer = 3;
    assert(read_weight(badWrap) == MS::kInvalidParameter);
    MDataBlock badDistance; badDistance.values[aDistance].scalar = 0;
    assert(read_weight(badDistance) == MS::kInvalidParameter);
    for (int attribute : {aStartLength, aEndLength, aStartOffset, aEndOffset}) {
        MDataBlock bad; bad.values[attribute].scalar = std::numeric_limits<double>::quiet_NaN();
        assert(read_extend(bad) == MS::kInvalidParameter);
    }
    MDataBlock negative; negative.values[aStartLength].scalar = -1;
    assert(read_extend(negative) == MS::kInvalidParameter);
    MDataBlock badRange; badRange.values[aRange].pair[0] = std::numeric_limits<double>::quiet_NaN();
    assert(read_stitch(badRange) == MS::kInvalidParameter);
}

struct SamplingCurve {
    int failure = 0, parameterCalls = 0, pointCalls = 0;
    double distance = 0, shifted = 0;
    MPoint point{1,2,3};
    double findParamFromLength(double value, MStatus* status) {
        ++parameterCalls; distance = value;
        *status = failure == 1 ? MS::kFailure : MS::kSuccess;
        return failure == 3 ? std::numeric_limits<double>::quiet_NaN() : value * 10.0;
    }
    MStatus getPointAtParam(double parameter, MPoint& result, MSpace::Space) {
        ++pointCalls; shifted = parameter;
        if (failure == 2) return MS::kFailure;
        result = point;
        return MS::kSuccess;
    }
};
void testSamplingAndEndpointDirections() {
    dommy::curve::StitchSamplingPlan plan;
    assert(dommy::curve::checkedStitchSamplingPlan(2, 3, plan));
    MPoint point;
    for (unsigned int i = 0; i < 3; ++i) {
        SamplingCurve curve;
        assert(sample_stitch(curve, 2, 0.25, 0.5, 0.2, plan, i, point) == MS::kSuccess);
        assert(curve.distance == 0.5 + 0.5*i && curve.shifted == curve.distance*10.0+0.2);
        assert(point == MPoint(1,2,3));
    }
    for (int failure : {1,2,3}) {
        SamplingCurve curve; curve.failure = failure;
        assert(sample_stitch(curve, 2, 0, 1, 0, plan, 0, point) != MS::kSuccess);
        assert(curve.pointCalls == (failure == 2 ? 1 : 0));
    }
    SamplingCurve curve; curve.point[0] = 1e100;
    assert(sample_stitch(curve, 2, 0, 1, 0, plan, 0, point) == MS::kInvalidParameter);
    curve.point[0] = 1;
    assert(sample_stitch(curve, 2, 0, 1, std::numeric_limits<double>::infinity(),
        plan, 0, point) == MS::kInvalidParameter);

    Curve directionCurve;
    MVector direction(7,8,9);
    assert(dommy::curve::directionAtParameter(directionCurve,
        std::numeric_limits<double>::quiet_NaN(), true, 0, direction) == MS::kInvalidParameter);
    assert(directionCurve.normalCalls == 0 && direction == MVector(7,8,9));
    directionCurve.failure = 3;
    assert(dommy::curve::directionAtParameter(directionCurve, 0.25, true, 0, direction) == MS::kFailure);
    assert(direction == MVector(7,8,9));
    directionCurve.failure = 0;
    assert(dommy::curve::directionAtParameter(directionCurve, 0.25, false, 0, direction) == MS::kSuccess);
    assert(direction == MVector(0,1,0));
    directionCurve.normalValue = MVector();
    assert(dommy::curve::directionAtParameter(directionCurve, 0.25, true, 0, direction) == MS::kSuccess);
    assert(!dommy::curve::normalizeDirection(direction));
    const auto source = std::make_shared<const std::vector<MPoint>>(
        std::initializer_list<MPoint>{MPoint(1,2,3), MPoint(5,6,7)});
    MPoint first(17,18,19), second(20,21,22);
    assert(extend_endpoint(source, true, true, MVector(1,0,0), 2, 1, -1,
        first, second) == MS::kSuccess);
    assert(first == MPoint(2,2,3) && second == MPoint(4,2,3));
    assert(extend_endpoint(source, false, true, MVector(1,0,0), 2, 1, -1,
        first, second) == MS::kSuccess);
    assert(first == MPoint(4,6,7) && second == MPoint(6,6,7));
    assert(extend_endpoint(source, true, false, MVector(1,0,0), 2, 100, -100,
        first, second) == MS::kSuccess);
    assert(first == MPoint(1,2,3) && second == MPoint(3,2,3));
    const MPoint savedFirst = first, savedSecond = second;
    assert(extend_endpoint(source, true, false, MVector(1,0,0), std::numeric_limits<double>::max(),
        0, 0, first, second) == MS::kInvalidParameter);
    assert(first == savedFirst && second == savedSecond);
}
void testBatchAndClosure() {
    DCurveOutputBatch batch;
    DCurveData output;
    assert(batch.publishTo(output) && output.calls == 0);
    assert(!batch.append(DCItem()));
    auto curve = std::make_shared<DCurveInternalData>();
    curve->valid = false; assert(!batch.append(curve));
    curve->valid = true; curve->count = 1000000;
    assert(batch.append(curve));
    assert(!batch.append(curve)); // Aggregate budget, without a large allocation.
    auto secondCurve = std::make_shared<DCurveInternalData>();
    assert(batch.append(secondCurve));
    output.fail = true;
    assert(!batch.publishTo(output) && output.calls == 1 && output.curves.empty());
    output.fail = false;
    assert(batch.publishTo(output) && output.calls == 2 && output.curves.size() == 2);

    std::vector<MVector> retained{{0,0,0}, {1,0,0}, {1,1,0}, {0.1,0,0}};
    bool changed = true;
    assert(close_result(retained, {4}, false, changed) == MS::kSuccess);
    assert(!changed && retained.front() == retained.back() && retained.size() == 4);
    retained = {{0,0,0}, {1,0,0}, {1,1,0}};
    assert(close_result(retained, {6}, false, changed) == MS::kSuccess);
    assert(changed && retained.front() == retained.back() && retained.size() == 4);
    retained = {{0,0,0}, {1,0,0}, {1,1,0}};
    assert(close_result(retained, {6}, true, changed) == MS::kSuccess);
    assert(changed && retained.front() != retained.back() && retained.size() == 3);
    retained.resize(1);
    assert(close_result(retained, {6}, true, changed) == MS::kInvalidParameter);
}
int main() {
    testFrames(); testInputs(); testBatchAndClosure(); testSamplingAndEndpointDirections();
    std::cout << "Curve input, frame, batch and closure failure fixtures: OK\n";
}
