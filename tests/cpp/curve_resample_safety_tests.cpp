#include "maya_attribute_doubles.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include <maya/MPoint.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace MSpace {
enum Space { kObject };
}
const double kMFnNurbsEpsilon = 1.0e-3;

// These doubles return predetermined samples; they do not implement NURBS.
namespace {
std::vector<MPoint> baseSamples, targetSamples, rebuiltPoints;
double reportedLength = 11.0;
bool nullCreation = false, wrongCvCount = false, throwOnQuery = false;
} // namespace

struct MPointArray {
    std::vector<MPoint> points;
    unsigned int length() const {
        return static_cast<unsigned int>(points.size());
    }
    MStatus setLength(unsigned int count) {
        const MStatus status = operation("points.resize");
        if (status == MS::kSuccess)
            points.resize(count);
        return status;
    }
    MPoint& operator[](unsigned int index) {
        return points.at(index);
    }
    const MPoint& operator[](unsigned int index) const {
        return points.at(index);
    }
};

struct MFnNurbsCurveData {
    MObject create(MStatus* status) {
        *status = operation("curveData.create");
        return *status == MS::kSuccess && !nullCreation ? MObject{"curveData"} : MObject::kNullObj;
    }
};

struct MFnNurbsCurve {
    MObject object;
    bool valid = false;
    MFnNurbsCurve() = default;
    MFnNurbsCurve(const MObject& value, MStatus* status) : object(value) {
        assert(!value.isNull());
        *status = operation("curve.bind." + value.name);
        valid = *status == MS::kSuccess;
    }
    double length(double tolerance, MStatus* status) const {
        assert(valid && tolerance == kMFnNurbsEpsilon);
        if (throwOnQuery)
            throw std::runtime_error("injected query exception");
        *status = operation("curve.length");
        return reportedLength;
    }
    double findParamFromLength(double length, MStatus* status) const {
        assert(valid && std::isfinite(length));
        *status = operation("curve.parameter");
        return length;
    }
    MStatus getPointAtParam(double parameter, MPoint& result, MSpace::Space space) const {
        assert(valid && space == MSpace::kObject && std::isfinite(parameter));
        const MStatus status = operation("curve.sample");
        if (status == MS::kSuccess) {
            const auto& samples = object.name == "target" ? targetSamples : baseSamples;
            const auto index = static_cast<std::size_t>(std::llround(parameter));
            result = samples.at(index);
        }
        return status;
    }
    MObject copy(const MObject& source, MObject& parent, MStatus* status) {
        assert(source.name == "base" && parent.name == "curveData");
        *status = operation("curve.copy");
        valid = *status == MS::kSuccess;
        return valid ? MObject{"copy"} : MObject::kNullObj;
    }
    MObject rebuild(unsigned int spans, unsigned int degree, unsigned int range, unsigned int type,
                    bool endpoints, bool tangents, bool history, float tolerance, MStatus* status) {
        assert(valid && degree == 3U && range == 1U && type == 0U);
        assert(endpoints && tangents && !history && tolerance == 0.01f);
        *status = operation("curve.rebuild");
        if (*status == MS::kSuccess)
            rebuiltPoints.resize(spans + 3U);
        return *status == MS::kSuccess ? MObject{"rebuilt"} : MObject::kNullObj;
    }
    int numCVs(MStatus* status) const {
        assert(valid && object.name == "rebuilt");
        *status = operation("curve.cvCount");
        return wrongCvCount ? -1 : static_cast<int>(rebuiltPoints.size());
    }
    MStatus setCVs(const MPointArray& points, MSpace::Space space) {
        assert(valid && object.name == "rebuilt" && space == MSpace::kObject);
        const MStatus status = operation("curve.setCVs");
        if (status == MS::kSuccess)
            rebuiltPoints = points.points;
        return status;
    }
    MStatus updateCurve() {
        assert(valid);
        return operation("curve.update");
    }
};

struct MPlug {
    std::string name;
    bool operator!=(const MObject& object) const {
        return name != object.name;
    }
};
struct MDataBlock;
struct MDataHandle {
    bool valid;
    int integer;
    double number;
    MObject object;
    MFnData::Type dataType;
    MDataBlock* output;
    int asInt() const {
        assert(valid);
        return integer;
    }
    double asDouble() const {
        assert(valid);
        return number;
    }
    MFnData::Type type() const {
        assert(valid);
        return dataType;
    }
    MObject data() const {
        assert(valid);
        return object;
    }
    MStatus set(const MObject& value);
};
struct MDataBlock {
    int sampling = 12;
    double blend = 0.5;
    MObject base = {"base"}, target = {"target"}, published = {"oldOutput"};
    MFnData::Type baseType = MFnData::kNurbsCurve, targetType = MFnData::kNurbsCurve;
    int publications = 0;
    bool clean = false;

    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input." + attribute.name);
        const bool isTarget = attribute.name == "inTargetCurve";
        return {*status == MS::kSuccess,          sampling, blend, isTarget ? target : base,
                isTarget ? targetType : baseType, nullptr};
    }
    MDataHandle outputValue(const MObject& attribute, MStatus* status) {
        assert(attribute.name == "outCurve");
        *status = operation("output.handle");
        return {*status == MS::kSuccess, 0, 0.0, {}, MFnData::kNurbsCurve, this};
    }
    MStatus setClean(const MPlug& plug) {
        assert(plug.name == "outCurve");
        const MStatus status = operation("output.clean");
        if (status == MS::kSuccess)
            clean = true;
        return status;
    }
};
MStatus MDataHandle::set(const MObject& value) {
    assert(valid && output && value.name == "rebuilt");
    const MStatus status = operation("output.set");
    if (status == MS::kSuccess) {
        output->published = value;
        ++output->publications;
    }
    return status;
}

struct DResampleCurve : AttributeNode {
    static MObject inCurve, aSampling, outCurve;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};
struct DBSCurve : AttributeNode {
    static MObject inBaseCurve, inTargetCurve, aBlend, aSampling, outCurve;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};

#include "curve_resample_methods.inc"

namespace {
void testInitialization(const char* fixture) {
    std::ifstream input(fixture);
    assert(input.good());
    std::ostringstream expected;
    expected << input.rdbuf();
    const std::string actual = schemaTrace("DResampleCurve", DResampleCurve::initialize) +
                               schemaTrace("DBSCurve", DBSCurve::initialize);
    if (actual != expected.str()) {
        std::cerr << "Curve declaration trace changed:\n" << actual;
        std::abort();
    }
    assert(testFailures("DResampleCurve", DResampleCurve::initialize, 18) == 36);
    assert(testFailures("DBSCurve", DBSCurve::initialize, 31) == 62);
}

MDataBlock defaults(int failure = -1) {
    resetCalls(failure);
    baseSamples.clear();
    targetSamples.clear();
    rebuiltPoints.clear();
    for (int i = 0; i < 12; ++i) {
        baseSamples.emplace_back(i, i + 1, i + 2);
        targetSamples.emplace_back(i + 2, i - 1, i + 6);
    }
    reportedLength = 11.0;
    nullCreation = wrongCvCount = throwOnQuery = false;
    return MDataBlock();
}

void assertUnpublished(const MDataBlock& block) {
    assert(block.publications == 0 && block.published.name == "oldOutput" && !block.clean);
}

template <typename Node> int testComputeFailures(int inputCount) {
    Node node;
    auto block = defaults();
    assert(node.compute({"unrelated"}, block) == MS::kUnknownParameter && callCount == 0);
    assertUnpublished(block);
    assert(node.compute({"outCurve"}, block) == MS::kSuccess);
    const auto baseline = operations;
    assert(block.publications == 1 && block.clean && rebuiltPoints.size() == 12U);
    const int count = callCount;
    for (MS::MStatusCode failureStatus : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = failureStatus;
        for (int failure = 0; failure < count; ++failure) {
            block = defaults(failure);
            const MStatus status = node.compute({"outCurve"}, block);
            assert(status != MS::kSuccess);
            // Input/output API errors retain their exact status. Older shared
            // NURBS query paths deliberately normalize some errors to kFailure.
            if (failure < inputCount || failure >= count - 3)
                assert(status == failureStatus);
            assert(callCount == failure + 1);
            assert(std::equal(operations.begin(), operations.end(), baseline.begin()));
            if (failure == count - 1) {
                // A clean failure does not roll back an already successful set.
                assert(block.publications == 1 && !block.clean);
            } else {
                assertUnpublished(block);
            }
        }
    }
    block = defaults();
    assert(node.compute({"outCurve"}, block) == MS::kSuccess);
    block = defaults();
    throwOnQuery = true;
    assert(node.compute({"outCurve"}, block) == MS::kFailure);
    assertUnpublished(block);
    return count * 2;
}

template <typename Node> void testInvalidInputs() {
    Node node;
    for (int sampling : {-1, 0, 3, 1000001, std::numeric_limits<int>::max()}) {
        auto block = defaults();
        block.sampling = sampling;
        assert(node.compute({"outCurve"}, block) == MS::kInvalidParameter);
        assertUnpublished(block);
    }
    for (bool wrongType : {false, true}) {
        auto block = defaults();
        if (wrongType)
            block.baseType = MFnData::kAny;
        else
            block.base = MObject::kNullObj;
        assert(node.compute({"outCurve"}, block) == MS::kInvalidParameter);
        assertUnpublished(block);
    }
    for (double length : {-1.0, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
        auto block = defaults();
        reportedLength = length;
        assert(node.compute({"outCurve"}, block) == MS::kFailure);
        assertUnpublished(block);
    }
    for (int condition = 0; condition < 3; ++condition) {
        auto block = defaults();
        nullCreation = condition == 0;
        wrongCvCount = condition == 1;
        if (condition == 2)
            baseSamples[0].x = std::numeric_limits<double>::quiet_NaN();
        assert(node.compute({"outCurve"}, block) == MS::kFailure);
        assertUnpublished(block);
    }
}

void testBlend() {
    DBSCurve node;
    for (double blend : {-2.0, 0.0, 0.5, 1.0, 2.0}) {
        auto block = defaults();
        block.blend = blend;
        assert(node.compute({"outCurve"}, block) == MS::kSuccess);
        for (std::size_t i = 0; i < rebuiltPoints.size(); ++i) {
            assert(rebuiltPoints[i].x == i + 2 * blend);
            assert(rebuiltPoints[i].y == i + 1 - 2 * blend);
            assert(rebuiltPoints[i].z == i + 2 + 4 * blend);
            assert(rebuiltPoints[i].w == 1.0);
        }
    }
    for (double blend : {0.0, 0.5, 1.0}) {
        auto block = defaults();
        block.blend = blend;
        baseSamples.assign(12, MPoint(-1.0e308, 1.0e308, -1.0e308));
        targetSamples.assign(12, MPoint(1.0e308, -1.0e308, 1.0e308));
        assert(node.compute({"outCurve"}, block) == MS::kSuccess);
        const double expected = blend == 0 ? -1.0e308 : blend == 1 ? 1.0e308 : 0.0;
        for (const auto& point : rebuiltPoints)
            assert(point.x == expected && point.y == -expected && point.z == expected);
    }
    // Homogeneous coordinates retain the old MPoint Cartesian meaning. Test
    // opposite weights and tiny weights without an overflowing reciprocal.
    for (double weight : {2.0, -2.0, 1.0e-310}) {
        auto block = defaults();
        MPoint base(weight, 2 * weight, 3 * weight), target(-3 * weight, -4 * weight, -5 * weight);
        base.w = weight;
        target.w = -weight;
        baseSamples.assign(12, base);
        targetSamples.assign(12, target);
        assert(node.compute({"outCurve"}, block) == MS::kSuccess);
        for (const auto& point : rebuiltPoints) {
            assert(std::abs(point.x - 2) < 1.0e-12 && std::abs(point.y - 3) < 1.0e-12);
            assert(std::abs(point.z - 4) < 1.0e-12 && point.w == 1.0);
        }
    }
    for (double bad :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        auto block = defaults();
        block.blend = bad;
        assert(node.compute({"outCurve"}, block) == MS::kInvalidParameter);
        assertUnpublished(block);
    }
    for (int condition = 0; condition < 7; ++condition) {
        auto block = defaults();
        if (condition == 0) {
            block.target = MObject::kNullObj;
        } else if (condition == 1) {
            block.targetType = MFnData::kAny;
        } else if (condition == 2) {
            baseSamples[0].w = 0.0;
        } else if (condition == 3) {
            targetSamples[0].w = 0.0;
        } else if (condition == 4) {
            targetSamples[0].x = 1.0e308;
            targetSamples[0].w = 1.0e-308;
        } else if (condition == 5) {
            targetSamples[0].w = std::numeric_limits<double>::infinity();
        } else {
            baseSamples[0].x = -1.0e308;
            targetSamples[0].x = 1.0e308;
            block.blend = 2.0; // True result is outside the double range.
        }
        assert(node.compute({"outCurve"}, block) ==
               (condition < 2 ? MS::kInvalidParameter : MS::kFailure));
        assertUnpublished(block);
    }
}

void testCountsAndResampling() {
    std::size_t count = 0;
    assert(dommy::curve::checkedCurveSampleCount(4, count) && count == 4U);
    assert(dommy::curve::checkedCurveSampleCount(1000000, count) && count == 1000000U);
    DResampleCurve node;
    for (int samples : {4, 8, 12}) {
        auto block = defaults();
        block.sampling = samples;
        reportedLength = samples - 1;
        assert(node.compute({"outCurve"}, block) == MS::kSuccess);
        assert(rebuiltPoints.size() == static_cast<std::size_t>(samples));
        for (int i = 0; i < samples; ++i)
            assert(rebuiltPoints[i].x == i && rebuiltPoints[i].y == i + 1 &&
                   rebuiltPoints[i].z == i + 2);
    }
}
void testPointBlendTransaction() {
    const MPoint base(1, 2, 3), target(5, 6, 7);
    MPoint result = base;
    assert(dommy::curve::checkedCurvePointBlend(result, target, 0.5, result));
    assert(result.x == 3 && result.y == 4 && result.z == 5 && result.w == 1);
    result = target;
    assert(dommy::curve::checkedCurvePointBlend(base, result, 0.5, result));
    assert(result.x == 3 && result.y == 4 && result.z == 5 && result.w == 1);
    const MPoint saved = result;
    for (int axis = 0; axis < 4; ++axis) {
        for (double invalid :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            MPoint invalidPoint = target;
            double* components[] = {&invalidPoint.x, &invalidPoint.y, &invalidPoint.z,
                                    &invalidPoint.w};
            *components[axis] = invalid;
            assert(!dommy::curve::checkedCurvePointBlend(base, invalidPoint, 0.5, result));
            assert(result.x == saved.x && result.y == saved.y && result.z == saved.z &&
                   result.w == saved.w);
        }
    }
    MPoint huge(5, 6, 1.0e308), opposite(1, 2, -1.0e308);
    assert(!dommy::curve::checkedCurvePointBlend(opposite, huge, 2.0, result));
    assert(result.x == saved.x && result.y == saved.y && result.z == saved.z &&
           result.w == saved.w);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    testInitialization(argv[1]);
    const int failures = testComputeFailures<DResampleCurve>(2) + testComputeFailures<DBSCurve>(4);
    testInvalidInputs<DResampleCurve>();
    testInvalidInputs<DBSCurve>();
    testBlend();
    testCountsAndResampling();
    testPointBlendTransaction();
    std::cout
        << "Curve resample safety passed: 98 initializer and " << failures
        << " compute failures; sampling, endpoints, extrapolation and homogeneous coordinates\n";
}
