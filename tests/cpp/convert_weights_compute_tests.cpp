#include "convert_weights_compute_doubles.h"
#include <algorithm>
#include <iostream>
#include "convert_weights_compute.inc"

namespace {
const Rows sentinel = {{0U, {{0U, 99.0f}, {2U, 77.0f}}}, {3U, {{4U, 66.0f}}}};

Rows expectedOutput() {
    Rows result;
    for (unsigned int i = 0U; i < input.size(); ++i) result[0U][i] = input[i];
    return result;
}

void success(const MPlug& plug) {
    DPConvertWeights node;
    MDataBlock block;
    resetCompute();
    assert(node.compute(plug, block) == MS::kSuccess);
    assert(output == expectedOutput() && publications == 1);
    assert((cleans == std::vector<std::string>{"inner.clean", "outer.clean", "block.clean"}));
}

void rejected(const MPlug& plug) {
    DPConvertWeights node;
    MDataBlock block;
    resetCompute();
    output = sentinel;
    assert(node.compute(plug, block) == MS::kUnknownParameter);
    assert(std::find(operations.begin(), operations.end(), "input") == operations.end());
    assert(output == sentinel && publications == 0 && cleans.empty());
}

int failureCases(const MPlug& plug) {
    input = {0.25f, -1000.0f, 2.0f};
    output = sentinel;
    success(plug);
    const auto trace = operations;
    const auto statusCalls = statusSites;
    const auto publication = std::find(trace.begin(), trace.end(), "outer.set") - trace.begin();
    int checked = 0;
    DPConvertWeights node;
    MDataBlock block;
    for (std::size_t at = 0; at < trace.size(); ++at) {
        // Inject both native status failures and C++ exceptions; void APIs only
        // receive exceptions. No later operation may run after a failure.
        for (int mode = 0; mode < 3; ++mode) {
            if (mode < 2 && !statusCalls[at]) continue;
            resetCompute(mode == 2 ? -1 : static_cast<int>(at));
            injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter;
            if (mode == 2) attributeThrowAt = static_cast<int>(at);
            output = sentinel;
            assert(node.compute(plug, block) == (mode == 2 ? MS::kFailure : injectedStatus));
            assert(callCount == static_cast<int>(at) + 1);
            assert(std::equal(operations.begin(), operations.end(), trace.begin()));
            if (static_cast<std::ptrdiff_t>(at) <= publication) {
                assert(output == sentinel && publications == 0 && cleans.empty());
            } else {
                assert(output == expectedOutput() && publications == 1);
            }
            success(plug); // A subsequent evaluation must recover.
            ++checked;
        }
    }
    return checked;
}
} // namespace

int main() {
    const MPlug root(MPlug::Root), row(MPlug::Row), array(MPlug::WeightArray);
    const MPlug weight(MPlug::Weight, 0U, 1U);
    input = {1.0f, 2.0f, 3.0f};
    output = sentinel;
    success(root);
    input = {9.0f};
    success(root);
    input = {-1.0f, 0.0f, -1000.0f, 2.0f, std::numeric_limits<float>::max()};
    success(root);
    success(root);
    success(row);
    success(array);
    success(weight);
    // A request for a missing weight must not allocate up to its logical index.
    success(MPlug(MPlug::Weight, 0U, std::numeric_limits<unsigned int>::max()));
    rejected(MPlug(MPlug::Unrelated));
    rejected(MPlug(MPlug::Row, 1U));
    rejected(MPlug(MPlug::WeightArray, 1U));
    rejected(MPlug(MPlug::Weight, 1U, 0U));
    MPlug invalidParent(MPlug::Weight);
    invalidParent.wrongParent = true;
    rejected(invalidParent);
    invalidParent.wrongParent = false;
    invalidParent.unindexedParent = true;
    rejected(invalidParent);

    DPConvertWeights node;
    MDataBlock block;
    for (const std::vector<float>& invalid : {
            std::vector<float>{}, std::vector<float>{std::numeric_limits<float>::infinity()},
            std::vector<float>{-std::numeric_limits<float>::infinity()},
            std::vector<float>{std::numeric_limits<float>::quiet_NaN()}}) {
        resetCompute();
        input = invalid;
        output = sentinel;
        assert(!node.compute(root, block));
        assert(output == sentinel && publications == 0 && cleans.empty());
        assert(operations.back() == "input");
    }
    resetCompute();
    input = {1.0f};
    output = sentinel;
    syntheticInputLength = static_cast<unsigned int>(dommy::generation::kMaxGeneratedPointCount + 1U);
    assert(node.compute(root, block) == MS::kInvalidParameter);
    assert(output == sentinel && publications == 0 && cleans.empty());
    assert(operations.back() == "input");

    int cases = 0;
    for (const MPlug& plug : {root, row, array, weight}) cases += failureCases(plug);
    std::cout << "DPConvertWeights: shrink/grow/repeat, nested plugs, invalid inputs and "
              << cases << " status/exception cases with recovery passed\n";
}
