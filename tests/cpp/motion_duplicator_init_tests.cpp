#include "maya_attribute_doubles.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Permit capturing the original callback before adding its missing guards.
#define CHECK_MSTATUS(value) do {(void)(value);} while (false)
#define CHECK_MSTATUS_AND_RETURN_IT(value) do {if (!(value)) return (value);} while (false)
#include "motion_duplicator_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto trace = schemaTrace("DTMotionDuplicator", DTMotionDuplicator::initialize);
    const int count = callCount;
    if (std::string(argv[1]) == "--emit") {std::cout << trace; return 0;}
    std::ifstream file(argv[1]); assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    assert(count == 277);
    testFailures("DTMotionDuplicator", DTMotionDuplicator::initialize, count);
    for (int i = 0; i < count; ++i) {
        resetCalls(); attributeThrowAt = i;
        assert(DTMotionDuplicator::initialize() == MS::kFailure && callCount == i + 1);
        assert(schemaTrace("DTMotionDuplicator", DTMotionDuplicator::initialize) == trace);
    }
    std::cout << "DTMotionDuplicator: 277 frozen initialization operations, 554 status failures and 277 exceptions passed\n";
}
