#include "fluid_emitter_doubles.h"
#include "fluid_emitter.inc"
#include "point_input_cases.h"

#include <iostream>
#include <limits>

namespace {
int evaluations = 0;
const char* const channels[] = {"density", "heat", "fuel"};

void reset() {
    fixture = FluidFixture();
    resetCalls();
    resetPointInputFixture();
}
MStatus evaluate() {
    ++evaluations;
    DPFluidEmitter node;
    return node.fluidEmitter(MObject(), MMatrix(), 0);
}
void optionalPointInputs() {
    optionalPointReaderCases(fixture.input, fixture.hasInput, DPFluidEmitter::inDP, reset);
    for (short mode : {short(0), short(1)}) {
        reset();
        fixture.mode = mode;
        fixture.base[0] = 1;
        assert(evaluate() == MS::kSuccess);
        const auto trace = operations;
        for (const char* operationName : {"input input", "point.bind"}) {
            const auto found = std::find(trace.begin(), trace.end(), operationName);
            assert(found != trace.end());
            const int at = static_cast<int>(found - trace.begin());
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                reset();
                fixture.mode = mode;
                fixture.base[0] = 1;
                failAt = at;
                injectedStatus = code;
                assert(evaluate() == code);
                assert(fixture.emitted.empty() && fixture.updates == 0);
                if (std::string(operationName) == "input input") {
                    assert(pointInputFixture.dataReads == 0);
                }
                reset();
                fixture.mode = mode;
                fixture.base[0] = 1;
                assert(evaluate() == MS::kSuccess);
            }
            reset();
            fixture.mode = mode;
            fixture.base[0] = 1;
            attributeThrowAt = at;
            assert(evaluate() == MS::kFailure);
            assert(fixture.emitted.empty() && fixture.updates == 0);
        }
        for (int invalid = 0; invalid < 4; ++invalid) {
            reset();
            fixture.mode = mode;
            fixture.base[0] = 1;
            if (invalid == 0) pointInputFixture.wrongMayaType = true;
            if (invalid == 1) pointInputFixture.wrongCppType = true;
            if (invalid == 2) pointInputFixture.nullRawData = true;
            if (invalid == 3) fixture.input.topology = false;
            assert(evaluate() == (invalid == 3 ? MS::kInvalidParameter : MS::kFailure));
            assert(fixture.emitted.empty() && fixture.updates == 0);
        }
        reset();
        fixture.mode = mode;
        fixture.base[0] = 1;
        fixture.hasInput = false;
        assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
        const auto query = std::find(operations.begin(), operations.end(), "point.connection");
        assert(query != operations.end());
        const int at = static_cast<int>(query - operations.begin());
        for (bool connected : {false, true}) {
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                reset();
                fixture.mode = mode;
                fixture.base[0] = 1;
                fixture.hasInput = false;
                pointInputFixture.connected = connected;
                failAt = at;
                injectedStatus = code;
                assert(evaluate() == code);
                assert(fixture.emitted.empty() && fixture.updates == 0);
            }
        }
        reset();
        fixture.mode = mode;
        fixture.base[0] = 1;
        fixture.hasInput = false;
        pointInputFixture.connected = true;
        assert(evaluate() == MS::kFailure);
        assert(fixture.emitted.empty() && fixture.updates == 0);
    }
    // Existing disabled/no-emission branches do not newly acquire inDP.
    reset();
    fixture.mode = 0;
    assert(evaluate() == MS::kSuccess && fixture.inputReads == 0);
}
template <typename Value>
DPAttributeData::Ptr attribute(const std::vector<Value>& values) {
    return std::make_shared<TypedDPAttributeData<Value> >(values);
}
template <typename Value>
void scalarCases() {
    for (std::size_t channel = 0; channel < 3; ++channel) {
        for (int mode : {1, 2}) {
            reset();
            fixture.channels[channel] = mode;
            fixture.input.attributes[channels[channel]] = attribute<Value>({Value(2)});
            assert(evaluate() == MS::kSuccess);
            assert(fixture.emitted.size() == 1);
            assert(fixture.emitted[0].values[channel] == 2);
            assert(fixture.emitted[0].scale == 1);
            assert(fixture.randomGets == 0 && fixture.randomSets == 0);
            // Node defaults are fallbacks, never multipliers of attributes.
            fixture.base[channel] = 9;
            fixture.emitted.clear();
            assert(evaluate() == MS::kSuccess);
            assert(fixture.emitted.size() == 1 && fixture.emitted[0].values[channel] == 2);
        }
    }
}

void fallbackAndDisabledCases() {
    for (std::size_t channel = 0; channel < 3; ++channel) {
        for (const std::string& name : {std::string("missing"), std::string()}) {
            reset();
            fixture.names[channels[channel]] = name;
            fixture.base[channel] = 3;
            assert(evaluate() == MS::kSuccess);
            assert(fixture.emitted.size() == 1 && fixture.emitted[0].values[channel] == 3);
        }
        reset();
        fixture.channels[channel] = 0;
        fixture.base[channel] = 9;
        fixture.input.attributes[channels[channel]] = attribute<std::string>({"not a scalar"});
        const std::size_t other = (channel + 1) % 3;
        fixture.input.attributes[channels[other]] = attribute<float>({2});
        assert(evaluate() == MS::kSuccess);
        assert(fixture.emitted.size() == 1 && fixture.emitted[0].values[channel] == 0);
        assert(fixture.emitted[0].values[other] == 2);
    }
    reset();
    assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
    assert(fixture.inputReads == 1); // Zero defaults still inspect grid attributes.
    reset();
    fixture.channels = {{0, 0, 0}};
    fixture.input.attributes["density"] = attribute<float>({2});
    assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
    reset();
    fixture.hasInput = false;
    assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
    reset();
    fixture.input.attributes["density"] = attribute<float>({0});
    fixture.base[0] = 9;
    assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
    reset();
    fixture.input.attributes["heat"] = attribute<float>({-1});
    assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
}

void invalidAttributeCases() {
    const double infinity = std::numeric_limits<double>::infinity();
    const std::vector<DPAttributeData::Ptr> invalid = {
        attribute<bool>({true}), attribute<std::string>({"2"}),
        attribute<Imath::V3f>({Imath::V3f(2, 2, 2)}),
        attribute<float>({}), attribute<float>({2, 3}),
        attribute<double>({infinity}), attribute<double>({-infinity}),
        attribute<double>({std::numeric_limits<double>::quiet_NaN()}),
        attribute<double>({std::numeric_limits<double>::max()})};
    for (const char* channel : channels) {
        for (const auto& value : invalid) {
            reset();
            fixture.input.attributes[channel] = value;
            assert(evaluate() == MS::kFailure);
            assert(fixture.emitted.empty());
        }
    }
    for (std::size_t count : {0U, 2U}) {
        reset();
        fixture.input.positions.resize(count);
        assert(evaluate() == MS::kFailure && fixture.emitted.empty());
    }
    // Validate the entire payload before publishing even the first voxel.
    reset();
    fixture.resolution = {{2, 1, 1}};
    fixture.input.positions.resize(2);
    fixture.input.attributes["density"] = attribute<double>({2, infinity});
    assert(evaluate() == MS::kFailure && fixture.emitted.empty());
}

template <typename Vector>
void colorCases() {
    reset();
    fixture.emitColor = true;
    fixture.input.attributes["density"] = attribute<float>({2});
    fixture.input.attributes["col"] = attribute<Vector>({Vector(1, 0, 0)});
    assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 1);
    assert(fixture.emitted[0].colorEnabled && fixture.emitted[0].color.r == 1);
    assert(fixture.emitted[0].color.b == 0);
    fixture.input.attributes.erase("col");
    fixture.emitted.clear();
    assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 1);
    assert(fixture.emitted[0].color.b == 1); // Missing color uses node fallback.
}

void multiVoxelAndPointCases() {
    reset();
    fixture.resolution = {{2, 2, 2}};
    fixture.input.positions.resize(8);
    fixture.input.attributes["density"] = attribute<float>({1, 2, 3, 4, 5, 6, 7, 8});
    assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 8);
    for (const auto& voxel : fixture.emitted) {
        assert(voxel.values[0] == 1 + voxel.x + 2 * (voxel.y + 2 * voxel.z));
    }
    reset();
    fixture.mode = 0;
    fixture.input.attributes["density"] = attribute<float>({2});
    assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
    assert(fixture.gridReads == 0 && fixture.inputReads == 0);
    for (bool jitter : {false, true}) {
        reset();
        fixture.mode = 0;
        fixture.jitter = jitter;
        fixture.base = {{2, 3, 4}};
        fixture.input.attributes["density"] = attribute<float>({9});
        assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 1);
        assert(fixture.emitted[0].values == (std::array<float, 3>{{2, 3, 4}}));
        assert(fixture.emitted[0].scale == 1);
        assert(fixture.randomGets == 1 && fixture.randomSets == 1);
    }
    reset();
    fixture.mode = 0;
    fixture.staticFalloff = true;
    assert(evaluate() == MS::kSuccess);
    assert(fixture.falloffValue == 1 && fixture.updates == 1);
    reset();
    fixture.mode = 0;
    fixture.emitColor = true;
    assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 1);
    assert(fixture.emitted[0].colorEnabled);
    // Preserve the existing grid-mode positive-scalar gate for color-only input.
    reset();
    fixture.emitColor = true;
    assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
}

void colorAdmissionCases() {
    const std::vector<DPAttributeData::Ptr> invalid = {
        attribute<float>({1}), attribute<Imath::V3f>({}),
        attribute<Imath::V3f>({Imath::V3f(1), Imath::V3f(1)}),
        attribute<Imath::V3d>({Imath::V3d(std::numeric_limits<double>::infinity())}),
        attribute<Imath::V3d>({Imath::V3d(std::numeric_limits<double>::max())})};
    for (const auto& value : invalid) {
        reset();
        fixture.emitColor = true;
        fixture.input.attributes["density"] = attribute<float>({2});
        fixture.input.attributes["col"] = value;
        assert(evaluate() == MS::kFailure && fixture.emitted.empty());
        fixture.colorGrid = false;
        assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 1);
        assert(!fixture.emitted[0].colorEnabled);
    }
    reset();
    fixture.names["density"] = "customDensity";
    fixture.input.attributes["customDensity"] = attribute<float>({2});
    assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 1);
    assert(fixture.emitted[0].values[0] == 2);
}

void statusAndTimeCases() {
    reset();
    fixture.input.attributes["density"] = attribute<float>({2});
    assert(evaluate() == MS::kSuccess);
    const int successfulCalls = callCount;
    for (int failure = 0; failure < successfulCalls; ++failure) {
        reset();
        fixture.input.attributes["density"] = attribute<float>({2});
        failAt = failure;
        injectedStatus = MS::kInvalidParameter;
        assert(evaluate() == MS::kInvalidParameter);
        assert(callCount == failure + 1 && fixture.emitted.empty());
        resetCalls();
        assert(evaluate() == MS::kSuccess && fixture.emitted.size() == 1);
    }
    for (double delta : {0.0, -1.0}) {
        reset();
        fixture.deltaTime = delta;
        fixture.input.attributes["density"] = attribute<float>({2});
        assert(evaluate() == MS::kSuccess && fixture.emitted.empty());
        assert(fixture.gridReads == 0);
    }
    reset();
    fixture.currentTime = 0;
    assert(evaluate() == MS::kSuccess && fixture.randomResets == 1);
    for (short mode : {-1, 2}) {
        reset();
        fixture.mode = mode;
        assert(evaluate() == MS::kFailure && fixture.emitted.empty());
    }
    for (int offset : {-1, 1}) {
        reset();
        fixture.indexOffset = offset;
        fixture.input.attributes["density"] = attribute<float>({2});
        assert(evaluate() == MS::kFailure && fixture.emitted.empty());
    }
    reset();
    fixture.resolution[0] = 0;
    assert(evaluate() == MS::kFailure && fixture.emitted.empty());
    reset();
    fixture.deltaTime = std::numeric_limits<double>::quiet_NaN();
    assert(evaluate() == MS::kFailure && fixture.emitted.empty());
}
} // namespace

int main() {
    optionalPointInputs();
    scalarCases<int32_t>();
    scalarCases<uint32_t>();
    scalarCases<int64_t>();
    scalarCases<float>();
    scalarCases<double>();
    fallbackAndDisabledCases();
    invalidAttributeCases();
    colorCases<Imath::V3i>();
    colorCases<Imath::V3f>();
    colorCases<Imath::V3d>();
    multiVoxelAndPointCases();
    colorAdmissionCases();
    statusAndTimeCases();
    std::cout << "DPFluidEmitter: " << evaluations << " callback evaluations passed\n";
}
