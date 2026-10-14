#include "group_transfer_init_doubles.h"
#include "point_utility_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPFillSpace", DPFillSpace::initialize) +
        schemaTrace("DPMerge", DPMerge::initialize) +
        schemaTrace("DPParticleAttribute", DPParticleAttribute::initialize);
    if (std::string(argv[1]) == "--emit") {std::cout << trace; return 0;}
    std::ifstream file(argv[1]); assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    int failures = 0, exceptions = 0;
    for (auto initialize : {DPFillSpace::initialize, DPMerge::initialize,
                           DPParticleAttribute::initialize}) {
        resetCalls(); assert(initialize()); const int count = callCount;
        for (int i = 0; i < count; ++i) {
            for (const auto code : {MS::kFailure, MS::kInvalidParameter}) {
                resetCalls(i); injectedStatus = code;
                assert(initialize() == code && callCount == i + 1); ++failures;
            }
            resetCalls(); attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1); ++exceptions;
            resetCalls(); assert(initialize());
        }
    }
    std::cout << "Point utilities: frozen schema, " << failures << " status failures and "
              << exceptions << " exceptions passed\n";
}
