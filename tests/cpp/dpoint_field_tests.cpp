#include "dpoint_field_doubles.h"
#include "dpoint_field.inc"
#include "point_input_cases.h"
#include <cfenv>

namespace {
int evaluations = 0, failureCases = 0, shortArrayCases = 0;
const double nan = std::numeric_limits<double>::quiet_NaN();
const double infinity = std::numeric_limits<double>::infinity();
enum class Kind { Vector, Noise, Goal };

template <typename T>
DPAttributeData::Ptr attribute(const std::vector<T>& values) {
    return std::make_shared<TypedDPAttributeData<T>>(values);
}
void reset() {
    resetCalls();
    resetPointInputFixture();
    shortArrayAt = -1;
    excessiveAllocations = invalidArrayAccesses = 0;
    input = DPData(); hasInput = true;
    positions = MVectorArray(); velocities = MVectorArray(); masses = MDoubleArray();
    positions.values = {MVector(), MVector(1,0,0)};
    velocities.values = {MVector(1,2,3), MVector(-1,0,1)};
    masses.values = {3,4};
    input.attributes["vel"] = attribute<Imath::V3d>({Imath::V3d(3,4,5), Imath::V3d(1,-1,2)});
    input.attributes["noise"] = attribute<float>({0.5f,2});
    numericInputs = {{"mMagnitude",2}, {"aDamping",0.5}, {"aSpringConstant",3},
                     {"mUseMaxDistance",1}, {"mMaxDistance",10}, {"mAttenuation",0}};
    stringInputs = {{"aVectorAttribute","vel"}, {"aNoiseAttribute","noise"}};
    publications = cleanCalls = falloffReads = 0;
    publishedForces.values = {MVector(99,99,99)};
    falloffValues = {1,1}; sampledRatios.clear();
}
MStatus evaluate(Kind kind, const MObject& plug = mOutputForce, int index = 42) {
    ++evaluations;
    MDataBlock block;
    const MPlug output(plug, index);
    switch (kind) {
    case Kind::Vector: return DPFluidVectorField().compute(output, block);
    case Kind::Noise: return DPFluidNoiseField().compute(output, block);
    case Kind::Goal: return DPGoalField().compute(output, block);
    }
    return MS::kFailure;
}
void assertForces(const std::vector<MVector>& expected) {
    assert(publications == 1 && cleanCalls == 1);
    assert(publishedForces.values == expected);
    assert(publishedIndex == 42 && inputIndex == 42);
    assert(invalidArrayAccesses == 0 && excessiveAllocations == 0);
}
void zeroForces() { assertForces({MVector(), MVector()}); }
void assertNoPublication() {
    assert(publications == 0 && cleanCalls == 0);
    assert(publishedForces.values == std::vector<MVector>({MVector(99,99,99)}));
    assert(invalidArrayAccesses == 0 && excessiveAllocations == 0);
}
void optionalPointInputs() {
    optionalPointReaderCases(input, hasInput, DPGoalField::inDP, reset);
    for (auto kind : {Kind::Vector, Kind::Noise, Kind::Goal}) {
        reset();
        assert(evaluate(kind) == MS::kSuccess);
        const auto trace = operations;
        for (const char* operationName : {"input inDP", "point.bind"}) {
            const auto found = std::find(trace.begin(), trace.end(), operationName);
            assert(found != trace.end());
            const int at = static_cast<int>(found - trace.begin());
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                reset();
                failAt = at;
                injectedStatus = code;
                assert(evaluate(kind) == code);
                assertNoPublication();
                if (std::string(operationName) == "input inDP") {
                    assert(pointInputFixture.dataReads == 0);
                }
                reset();
                assert(evaluate(kind) == MS::kSuccess);
            }
            reset();
            attributeThrowAt = at;
            assert(evaluate(kind) == MS::kFailure);
            assertNoPublication();
        }
        for (int invalid = 0; invalid < 4; ++invalid) {
            reset();
            if (invalid == 0) pointInputFixture.wrongMayaType = true;
            if (invalid == 1) pointInputFixture.wrongCppType = true;
            if (invalid == 2) pointInputFixture.nullRawData = true;
            if (invalid == 3) input.topology = false;
            assert(evaluate(kind) == (invalid == 3 ? MS::kInvalidParameter : MS::kFailure));
            assertNoPublication();
        }
        reset();
        hasInput = false;
        pointInputFixture.connected = true;
        assert(evaluate(kind) == MS::kFailure);
        assertNoPublication();
        reset();
        hasInput = false;
        assert(evaluate(kind) == MS::kSuccess);
        zeroForces();
        const auto query = std::find(operations.begin(), operations.end(), "point.connection");
        assert(query != operations.end());
        const int at = static_cast<int>(query - operations.begin());
        for (bool connected : {false, true}) {
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                reset();
                hasInput = false;
                pointInputFixture.connected = connected;
                failAt = at;
                injectedStatus = code;
                assert(evaluate(kind) == code);
                assertNoPublication();
            }
        }
    }
}

void goldens() {
    reset(); assert(evaluate(Kind::Vector) == MS::kSuccess);
    assertForces({MVector(7,10,13), MVector(1,-2,5)});
    reset(); assert(evaluate(Kind::Noise) == MS::kSuccess);
    assertForces({MVector(3,6,9), MVector(-16,0,16)});
    reset(); assert(evaluate(Kind::Goal) == MS::kSuccess);
    assertForces({MVector(11,4,-3), MVector(13,6,-1)});
    assert(sampledRatios.size() == 2 && std::fabs(sampledRatios[0]-std::sqrt(5.0)/10) < 1e-12);
    reset(); numericInputs["mAttenuation"] = 1; falloffValues = {0.5,0.5};
    assert(evaluate(Kind::Goal) == MS::kSuccess);
    const double weight = (1-std::sqrt(5.0)/10)*0.5;
    assert(std::fabs(publishedForces[0].x-11*weight) < 1e-12);
    reset(); input.points.resize(1); numericInputs["mUseMaxDistance"] = 0;
    assert(evaluate(Kind::Goal) == MS::kSuccess);
    assertForces({MVector(11,4,-3), MVector(7,6,-1)});
    assert(falloffReads == 0);
    reset(); masses.values = {2}; assert(evaluate(Kind::Noise) == MS::kSuccess);
    assertForces({MVector(2,4,6), MVector(-8,0,8)});
    reset(); masses.values.clear(); assert(evaluate(Kind::Noise) == MS::kSuccess);
    assertForces({MVector(1,2,3), MVector(-4,0,4)});
    reset(); input.attributes.clear(); assert(evaluate(Kind::Vector) == MS::kSuccess);
    assertForces(velocities.values); // Preserve the historical velocity-plus-field formula.
    reset(); input.attributes.clear(); assert(evaluate(Kind::Noise) == MS::kSuccess); zeroForces();
}

void finiteExtremeForces() {
    reset(); velocities.values = {MVector(1e308,0,0), MVector()};
    masses.values = {1}; numericInputs["mMagnitude"] = .25;
    input.attributes["noise"] = attribute<float>({2,2});
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(evaluate(Kind::Noise) == MS::kSuccess);
    assertForces({MVector(5e307,0,0), MVector()});
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);

    reset(); velocities.values = {MVector(-1e308,0,0), MVector()};
    input.attributes["vel"] = attribute<Imath::V3d>({Imath::V3d(1e308,0,0), Imath::V3d()});
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(evaluate(Kind::Vector) == MS::kSuccess);
    assertForces({MVector(1e308,0,0), MVector()});
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);

    reset(); velocities.values = {MVector(1e308,0,0), MVector()};
    numericInputs["mMagnitude"] = 1e-308; numericInputs["aSpringConstant"] = 0;
    numericInputs["aDamping"] = 2; numericInputs["mUseMaxDistance"] = 0;
    std::feclearexcept(FE_ALL_EXCEPT);
    assert(evaluate(Kind::Goal) == MS::kSuccess);
    assert(std::fabs(publishedForces[0].x + 2) < 1e-14);
    assertForces({publishedForces[0], MVector()});
    assert(std::fetestexcept(FE_OVERFLOW | FE_INVALID) == 0);
}

template <typename T> void vectorType() {
    reset(); input.attributes["vel"] = attribute<T>({T(3,4,5), T(1,-1,2)});
    assert(evaluate(Kind::Vector) == MS::kSuccess);
    assertForces({MVector(7,10,13), MVector(1,-2,5)});
}
template <typename T> void scalarType() {
    reset(); input.attributes["noise"] = attribute<T>({T(1),T(2)});
    assert(evaluate(Kind::Noise) == MS::kSuccess);
    assertForces({MVector(6,12,18), MVector(-16,0,16)});
}

void compatibilityCases() {
    for (Kind kind : {Kind::Vector, Kind::Noise, Kind::Goal}) {
        reset(); positions.values.clear(); velocities.values.clear(); masses.values.clear();
        input.points.clear(); input.attributes.clear();
        assert(evaluate(kind) == MS::kSuccess); assertForces({});
        reset(); hasInput = false;
        assert(evaluate(kind) == MS::kSuccess); zeroForces();
        reset(); velocities.values.resize(1);
        assert(evaluate(kind) == MS::kSuccess); zeroForces();
        reset(); numericInputs["mMagnitude"] = nan;
        assert(evaluate(kind) == MS::kSuccess); zeroForces();
        reset(); assert(evaluate(kind, MObject{"unhandled"}) == MS::kUnknownParameter);
        assert(callCount == 0); assertNoPublication();
        reset(); assert(evaluate(kind, mOutputForce, -1) == MS::kFailure); assertNoPublication();
        reset(); positions.reportedLength = dommy::generation::kMaxGeneratedPointCount + 1U;
        assert(evaluate(kind) == MS::kInvalidParameter); assertNoPublication();
        assert(operations.back() == "vector.read mInputPositions");
        reset(); velocities.values.clear();
        assert(evaluate(kind) == MS::kSuccess);
        if (kind == Kind::Vector) assertForces({MVector(6,8,10), MVector(2,-2,4)});
        else if (kind == Kind::Noise) zeroForces();
        else assertForces({MVector(12,6,0), MVector(12,6,0)});
    }
    for (Kind kind : {Kind::Vector, Kind::Noise}) {
        const std::string key = kind == Kind::Vector ? "vel" : "noise";
        for (const auto& invalid : {attribute<std::string>({"bad","bad"}),
                                   attribute<float>({float(nan),float(nan)}), attribute<float>({1})}) {
            reset(); input.attributes[key] = invalid;
            assert(evaluate(kind) == MS::kSuccess); zeroForces();
        }
        reset(); input.points.resize(1);
        assert(evaluate(kind) == MS::kSuccess); zeroForces();
        reset(); velocities[1].x = infinity;
        assert(evaluate(kind) == MS::kSuccess); zeroForces();
    }
    reset(); input.attributes["vel"] = attribute<Imath::V3d>({Imath::V3d(1), Imath::V3d(nan)});
    assert(evaluate(Kind::Vector) == MS::kSuccess); zeroForces();
    reset(); numericInputs["mMagnitude"] = std::numeric_limits<double>::max();
    assert(evaluate(Kind::Vector) == MS::kSuccess); zeroForces();
    for (double mass : {-1.0, nan, infinity}) {
        reset(); masses.values[1] = mass;
        assert(evaluate(Kind::Noise) == MS::kSuccess); zeroForces();
    }
    reset(); masses.values = {1,2,3};
    assert(evaluate(Kind::Noise) == MS::kSuccess); zeroForces();
    reset(); velocities[1].x = nan;
    assert(evaluate(Kind::Goal) == MS::kSuccess);
    assertForces({MVector(11,4,-3), MVector()});
    reset(); falloffValues[1] = nan;
    assert(evaluate(Kind::Goal) == MS::kSuccess);
    assertForces({MVector(11,4,-3), MVector()});
    reset(); numericInputs["mMaxDistance"] = 1;
    assert(evaluate(Kind::Goal) == MS::kSuccess); zeroForces(); assert(falloffReads == 0);
}

void failureInjection() {
    for (Kind kind : {Kind::Vector, Kind::Noise, Kind::Goal}) {
        reset(); assert(evaluate(kind) == MS::kSuccess);
        const std::vector<std::string> calls = operations;
        assert(std::count(calls.begin(), calls.end(), "array.setLength") == 1);
        assert(std::count(calls.begin(), calls.end(), "array.copy") == 1);
        for (MS::MStatusCode code : {MS::kFailure, MS::kInvalidParameter}) {
            for (std::size_t index = 0; index < calls.size(); ++index) {
                reset(); failAt = index; injectedStatus = code;
                assert(evaluate(kind) == code);
                assert(callCount == static_cast<int>(index + 1));
                assert(cleanCalls == 0 && invalidArrayAccesses == 0);
                if (calls[index] == "output.clean") assert(publications == 1);
                else assertNoPublication();
                reset(); assert(evaluate(kind) == MS::kSuccess); ++failureCases;
            }
        }
        for (std::size_t index = 0; index < calls.size(); ++index) {
            if (calls[index] == "array.setLength" || calls[index] == "array.copy") {
                reset(); shortArrayAt = index;
                assert(evaluate(kind) == MS::kFailure); assertNoPublication();
                assert(callCount == static_cast<int>(index + 1)); ++shortArrayCases;
            }
            reset(); attributeThrowAt = index;
            assert(evaluate(kind) == MS::kFailure);
            assert(cleanCalls == 0 && invalidArrayAccesses == 0);
            if (calls[index] != "output.clean") assertNoPublication();
        }
    }
}
} // namespace

int main() {
    optionalPointInputs();
    goldens();
    finiteExtremeForces();
    vectorType<Imath::V3i>(); vectorType<Imath::V3f>(); vectorType<Imath::V3d>();
    scalarType<int32_t>(); scalarType<uint32_t>(); scalarType<int64_t>();
    scalarType<float>(); scalarType<double>();
    compatibilityCases();
    failureInjection();
    std::cout << "DPoint fields: " << evaluations << " evaluations, " << failureCases
              << " API failures/retries, " << shortArrayCases << " short-array failures: OK\n";
}
