#include "mesh_emitter_doubles.h"
#include "mesh_noise_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DMNoise", DMNoise::initialize) +
                              schemaTrace("DMFastNoise", DMFastNoise::initialize);
    std::ifstream file(argv[1]);
    assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    const int failures = testFailures("DMNoise", DMNoise::initialize, 132) +
                         testFailures("DMFastNoise", DMFastNoise::initialize, 138);
    int exceptions = 0;
    for (auto initialize : {DMNoise::initialize, DMFastNoise::initialize}) {
        resetCalls();
        assert(initialize() == MS::kSuccess);
        const int calls = callCount;
        for (int index = 0; index < calls; ++index) {
            resetCalls();
            attributeThrowAt = index;
            assert(initialize() == MS::kFailure);
            assert(callCount == index + 1);
            resetCalls();
            assert(initialize() == MS::kSuccess);
            ++exceptions;
        }
    }
    std::cout << "Mesh noise initialization: unchanged 270-operation schema, " << failures
              << " status failures and " << exceptions << " exceptions passed\n";
}
