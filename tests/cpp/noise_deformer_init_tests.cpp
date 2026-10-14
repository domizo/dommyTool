#include "maya_attribute_doubles.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include "noise/PerlinNoise.h"
#include "noise_deformer_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("noiseDeformer", noiseDeformer::initialize);
    const int count = callCount;
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]); assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    assert(count == 136);
    testFailures("noiseDeformer", noiseDeformer::initialize, count);
    for (int at = 0; at < count; ++at) {
        resetCalls(); attributeThrowAt = at;
        assert(noiseDeformer::initialize() == MS::kFailure && callCount == at + 1);
        assert(schemaTrace("noiseDeformer", noiseDeformer::initialize) == trace);
    }
    std::cout << "DNoise initialization: 136 frozen operations, 272 status failures, 136 exceptions and valid retries passed; no Maya registration runtime\n";
}
