#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include "convert_weights_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPConvertWeights", DPConvertWeights::initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file);
    assert(trace == std::string(std::istreambuf_iterator<char>(file), {}));
    testFailures("DPConvertWeights", DPConvertWeights::initialize, 24);
    for (int at = 0; at < 24; ++at) {
        resetCalls();
        attributeThrowAt = at;
        assert(!DPConvertWeights::initialize() && callCount == at + 1);
        assert(schemaTrace("DPConvertWeights", DPConvertWeights::initialize) == trace);
    }
    std::cout << "DPConvertWeights: 24 frozen operations, 48 failures, 24 exceptions passed\n";
}
