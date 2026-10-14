#include "mesh_emitter_doubles.h"
#include "primitive_metadata_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DCGroupFromDP", DCGroupFromDP::initialize) +
        schemaTrace("DCGroupDelete", DCGroupDelete::initialize) +
        schemaTrace("DMAttributeCopy", DMAttributeCopy::initialize) +
        schemaTrace("DCAttributeCopy", DCAttributeCopy::initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    int failures = 0, exceptions = 0;
    for (auto initialize : {DCGroupFromDP::initialize, DCGroupDelete::initialize,
                           DMAttributeCopy::initialize, DCAttributeCopy::initialize}) {
        resetCalls();
        assert(initialize());
        const int count = callCount;
        for (int i = 0; i < count; ++i) {
            for (const auto code : {MS::kFailure, MS::kInvalidParameter}) {
                resetCalls(i);
                injectedStatus = code;
                assert(initialize() == code && callCount == i + 1);
                ++failures;
            }
            resetCalls();
            attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1);
            ++exceptions;
            resetCalls();
            assert(initialize());
        }
    }
    std::cout << "Primitive metadata: frozen initialization; " << failures
              << " status failures and " << exceptions << " exceptions passed\n";
}
