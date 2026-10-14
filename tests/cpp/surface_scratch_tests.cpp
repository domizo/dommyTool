// Scratch/adapter control-flow test. No native geometry, shading or DG execution.
#include <maya/MStatus.h>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaCheckedArray.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
int calls = 0, failAt = -1, throwAt = -1, invalidAccesses = 0;
MS::MStatusCode injectedStatus = MS::kFailure;
std::function<void()> nestedEvaluation;
std::vector<std::vector<double>> sampledU, sampledV;
MStatus operation() {
    const int point = calls++;
    if (point == throwAt) throw std::runtime_error("injected host exception");
    return point == failAt ? injectedStatus : MS::kSuccess;
}
void reset(int failure = -1, int throwing = -1) {
    calls = 0; failAt = failure; throwAt = throwing; invalidAccesses = 0;
    sampledU.clear(); sampledV.clear(); nestedEvaluation = {};
}
}

struct MObject {};
struct MFloatPoint { float x = 0, y = 0, z = 0; };
struct MVector { double x = 0, y = 0, z = 0; };
template <typename Value> struct Array {
    std::vector<Value> values;
    Array() = default;
    Array(std::initializer_list<Value> input) : values(input) {}
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    Value& operator[](unsigned int index) {
        if (index >= length()) ++invalidAccesses;
        return values.at(index);
    }
    const Value& operator[](unsigned int index) const {
        if (index >= length()) ++invalidAccesses;
        return values.at(index);
    }
    MStatus append(Value value) {
        const MStatus status = operation();
        if (status) values.push_back(value);
        return status;
    }
    MStatus setLength(unsigned int size) {
        const MStatus status = operation();
        if (status) values.resize(size);
        return status;
    }
    MStatus set(Value value, unsigned int index) {
        const MStatus status = operation();
        if (status) (*this)[index] = value;
        return status;
    }
};
using MFloatPointArray = Array<MFloatPoint>;
using MFloatArray = Array<float>;
using MDoubleArray = Array<double>;
using MVectorArray = Array<MVector>;
struct Sample {
    MFloatPoint point;
    double u, v, preU, preV;
};
MStatus evaluate2dTextureInBatches(
    MObject, const MDoubleArray& u, const MDoubleArray& v, MVectorArray&, MDoubleArray&) {
    const MStatus status = operation();
    if (!status) return status;
    assert(u.length() == v.length());
    // Explicit evaluation seam, not a shading-engine result.
    if (nestedEvaluation) {
        auto nested = nestedEvaluation;
        nestedEvaluation = {};
        nested();
    }
    sampledU.push_back(u.values); sampledV.push_back(v.values);
    return status;
}

#include "surface_scratch.inc"

int main() {
    surfaceDuplicater first, second;
    const std::vector<Sample> oldSamples = {{{}, .125, .25, .25, .5}};
    const std::vector<Sample> newSamples = {
        {{}, .625, .5, .5, .25}, {{}, .875, .75, .75, .875}};
    float u = -1, v = -1;
    reset();
    assert(first.evaluate(oldSamples, u, v) == MS::kSuccess);
    assert(u == .25f && v == .5f);
    reset();
    assert(first.evaluate(newSamples, u, v) == MS::kSuccess);
    const int expectedCalls = calls;
    assert(expectedCalls == 19);
    assert(u == .75f && v == .875f);
    assert(sampledU.back() == std::vector<double>({.625, .875}));
    assert(sampledV.back() == std::vector<double>({.5, .75}));
    assert(invalidAccesses == 0);
    for (MS::MStatusCode failure : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = failure;
        for (int point = 0; point < expectedCalls; ++point) {
            u = v = -1;
            reset(point);
            const MStatus status = first.evaluate(newSamples, u, v);
            // CheckedArray's final append adapter throws; compute catches it.
            assert(status == (point >= expectedCalls - 2 ? MS::kFailure : failure));
            assert(calls == point + 1 && invalidAccesses == 0);
            assert(u == -1 && v == -1);
            reset();
            assert(first.evaluate(oldSamples, u, v) == MS::kSuccess);
            assert(u == .25f && v == .5f);
        }
    }
    for (int point = 0; point < expectedCalls; ++point) {
        u = v = -1;
        reset(-1, point);
        assert(first.evaluate(newSamples, u, v) == MS::kFailure);
        assert(calls == point + 1 && invalidAccesses == 0 && u == -1 && v == -1);
        reset();
        assert(first.evaluate(oldSamples, u, v) == MS::kSuccess);
        assert(u == .25f && v == .5f);
    }
    // Re-enter scratch/texture work on the same object, then another object.
    // This verifies data isolation, not a permission to relax kUntrusted.
    for (surfaceDuplicater* nested : {&first, &second}) {
        reset();
        nestedEvaluation = [&] {
            float nestedU = -1, nestedV = -1;
            assert(nested->evaluate(oldSamples, nestedU, nestedV) == MS::kSuccess);
            assert(nestedU == .25f && nestedV == .5f);
        };
        assert(first.evaluate(newSamples, u, v) == MS::kSuccess);
        assert(u == .75f && v == .875f && sampledU.size() == 2);
        assert(sampledU.front() == std::vector<double>({.125}));
        assert(sampledU.back() == std::vector<double>({.625, .875}));
        assert(invalidAccesses == 0);
    }
    reset();
    u = v = -1;
    assert(first.evaluate({}, u, v) == MS::kSuccess);
    assert(sampledU.back().empty() && u == -1 && v == -1);
    reset();
    MVectorArray colors;
    MDoubleArray transparencies;
    assert(first.getColorFromTex({}, MFloatArray({.5f}), {}, colors, transparencies) ==
           MS::kInvalidParameter);
    assert(calls == 0 && sampledU.empty());
    reset();
    const float rounded = static_cast<float>(.123456789);
    assert(first.getColorFromTex({}, MFloatArray({rounded}), MFloatArray({rounded}),
                                 colors, transparencies) == MS::kSuccess);
    assert(sampledU.back()[0] == static_cast<double>(rounded));
    std::cout << "Surface scratch: evaluation isolation, appends, texture conversion, Dot UV and all "
              << expectedCalls << " API failure/exception points passed (geometry/texture seams)\n";
}
