// Exact production sampling, ramp-binding and random-stream boundaries.
// Curve queries are validated parameter traces, not native geometry emulation.
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaRandomValueGeneration.h"
#include "common/utility/DNumericValidation.h"
#include <cassert>
#include <iostream>
#include <vector>

#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
static int calls = 0, failAt = -1, admissions = 0;
static MStatus failure = MS::kFailure;
MStatus operation() { return calls++ == failAt ? failure : MStatus(MS::kSuccess); }
struct MDoubleArray : std::vector<double> {
    using std::vector<double>::vector;
    unsigned int length() const { return size(); }
    MStatus setLength(unsigned int count) {
        const MStatus status = operation();
        if (status) resize(count);
        return status;
    }
    MStatus copy(const MDoubleArray& other) {
        const MStatus status = operation();
        if (status) assign(other.begin(), other.end());
        return status;
    }
    double& operator[](std::size_t i) { return at(i); }
    const double& operator[](std::size_t i) const { return at(i); }
};
struct MObject {};
MObject thisMObject() { return {}; }
struct MPlug { MPlug() = default; MPlug(MObject, MObject) {} };
struct MRampAttribute {
    MRampAttribute(MPlug, MStatus* status) { *status = operation(); }
};
struct MPoint {};
namespace MSpace { enum Space { kWorld }; }
const double kMFnNurbsEpsilon = 1e-6;
static std::vector<double> queryParameters, queryDistances;
struct Curve {
    double start = 0, end = 1, lengthValue = 2;
    bool overrideParameter = false;
    double returnedParameter = 0;
    MStatus getKnotDomain(double& first, double& last) {
        first = start; last = end; return operation();
    }
    double length(double, MStatus* status) { *status = operation(); return lengthValue; }
    double findParamFromLength(double distance, MStatus* status) {
        assert(std::isfinite(distance) && distance >= 0);
        queryDistances.push_back(distance);
        *status = operation();
        return overrideParameter ? returnedParameter : start + distance / lengthValue * (end-start);
    }
    MStatus getPointAtParam(double parameter, MPoint&, MSpace::Space) {
        assert(std::isfinite(parameter) && parameter >= start && parameter <= end);
        queryParameters.push_back(parameter);
        return operation();
    }
};
static Curve crvFn, tanCrvFn;
static std::vector<MDoubleArray> randomOutput;
#include "eyelash_controls.inc"

void reset(int at = -1) {
    calls = admissions = 0; failAt = at;
    queryDistances.clear(); queryParameters.clear(); randomOutput.clear();
    crvFn = Curve(); tanCrvFn = Curve();
}
void testReflection() {
    using dommy::generation::reflectEyelashParameterOnce;
    for (double start : {-10.0, 0.0, 10.0}) {
        const double end = start + 2;
        for (int i = -4; i <= 8; ++i) {
            const double input = start + i * .5;
            const double legacy = input < start ? start+(start-input) :
                                  input > end ? end-(input-end) : input;
            double result = 123;
            assert(reflectEyelashParameterOnce(input, start, end, result));
            assert(result == legacy);
        }
    }
    for (double input : {-10.0, 10.0, std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()}) {
        double result = 123;
        assert(!reflectEyelashParameterOnce(input, 0, 1, result) && result == 123);
    }
    double result = 123;
    assert(!reflectEyelashParameterOnce(-1e308, 1e308, 1.5e308, result) && result == 123);
    assert(!reflectEyelashParameterOnce(0, 1, 0, result));
    assert(!reflectEyelashParameterOnce(0, 0, 0, result));
    assert(!reflectEyelashParameterOnce(0, 0, std::numeric_limits<double>::infinity(), result));
}
void testSampling() {
    for (auto probe : {nativeSampling, customSampling}) {
        reset(); assert(probe(3, 0, 0));
        const int operations = calls;
        assert(admissions == 1 && queryParameters.front() == 0 && queryParameters.back() == 1);
        for (const auto status : {MS::kFailure, MS::kInvalidParameter}) {
            failure = status;
            for (int at = 0; at < operations; ++at) {
                reset(at); assert(!probe(3, 0, 0));
                assert(calls == at+1 && admissions == 0);
            }
        }
        reset(); assert(probe(3, 0, .25));
        assert(queryParameters.front() == .25 && queryParameters.back() == .75);
        reset(); assert(!probe(2, 0, 10) && queryParameters.empty());
        reset(); assert(!probe(2, 0, std::numeric_limits<double>::quiet_NaN()) && queryParameters.empty());
        reset(); crvFn.end = 0; assert(!probe(2, 0, 0) && queryParameters.empty());
        reset(); crvFn.start = std::numeric_limits<double>::quiet_NaN();
        assert(!probe(2, 0, 0) && queryParameters.empty());
    }
    reset(); assert(!customSampling(2, 10, 0) && queryParameters.empty());
    reset(); tanCrvFn.start = .25;
    assert(!customSampling(2, 0, 0) && queryParameters.empty());
    reset(); tanCrvFn.end = 0; assert(!customSampling(2, 0, 0) && queryParameters.empty());
    // Preserve the legacy origin formula; no implicit nonzero-domain remapping.
    reset(); crvFn.start = tanCrvFn.start = 10; crvFn.end = tanCrvFn.end = 11;
    assert(!customSampling(2, 0, 0) && queryParameters.empty());
    reset(); crvFn.end = tanCrvFn.end = 1e308;
    assert(!customSampling(2, 1e308, 0) && admissions == 0);
    for (double invalid : {-1.0, 2.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        reset(); crvFn.overrideParameter = true; crvFn.returnedParameter = invalid;
        assert(!nativeSampling(2, 0, 0) && queryParameters.empty());
    }
    reset(); crvFn.start = 1e308; crvFn.end = 1.5e308;
    assert(!nativeSampling(2, 0, 8e307) && queryParameters.empty());
    reset(); crvFn.lengthValue = std::numeric_limits<double>::infinity();
    assert(!nativeSampling(2, 0, 0) && queryDistances.empty());
}
void testRamps() {
    for (auto probe : {nativeRamps, customRamps}) {
        reset(); assert(probe() && calls == 10 && admissions == 1);
        for (const auto status : {MS::kFailure, MS::kInvalidParameter}) {
            failure = status;
            for (int i = 0; i < 10; ++i) {
                reset(i); assert(probe() == status && calls == i+1 && admissions == 0);
            }
        }
    }
}
void testRandomStreams() {
    const unsigned int offsets[] = {8, 7, 0, 1, 2, 3, 4, 5};
    const double minimum[] = {-1, -.5, -2, -3, -4, -5, -6, -7};
    const double maximum[] = {2, .5, 2, 3, 4, 5, 6, 7};
    const int largest = std::numeric_limits<int>::max();
    for (int seed : {0, 1, 100, largest-8, largest-1, largest}) {
        reset(); assert(randomProbe(seed));
        assert(randomOutput.size() == 8);
        for (unsigned int i = 0; i < 8; ++i) {
            std::vector<double> expected;
            const auto shifted = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(seed)+offsets[i]) % (static_cast<std::uint64_t>(largest)+1U));
            dommy::random::generateValues(shifted, minimum[i], maximum[i], 5, expected, true);
            assert(randomOutput[i] == expected);
        }
    }
    const int operations = calls;
    for (int at = 0; at < operations; ++at) {
        reset(at); assert(!randomProbe(1));
        assert(admissions == 0 && randomOutput.empty() && calls == at+1);
    }
    reset(); assert(!randomProbe(-1) && admissions == 0 && randomOutput.empty());
}
int main() {
    testReflection(); testSampling(); testRamps(); testRandomStreams();
    std::cout << "Eyelash: reflection, native/custom sampling, 20 ramp bindings and 8 random streams passed\n";
}
