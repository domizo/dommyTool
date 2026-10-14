#include "maya_attribute_doubles.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include <fstream>
#include <iostream>
#include <iterator>

// Used only when freezing the original, log-only initialization bodies.
#define CHECK_MSTATUS(value) do { (void)(value); } while (false)
namespace MFnNurbsCurveData { using MFnData::kMesh; }
#include "generator_registration.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* names[] = {"DHEyeGen", "DCurveDuplicator", "DBubbleCrystal", "LeafGen", "icosahedronNode"};
    MStatus (*initializers[])() = {DHEyeGen::initialize, DCurveDuplicator::initialize,
        DBubbleCrystal::initialize, LeafGen::initialize, icosahedronNode::initialize};
    const int expectedCounts[] = {128, 77, 93, 100, 18};
    std::string trace;
    for (unsigned int i = 0; i < 5; ++i) {
        trace += schemaTrace(names[i], initializers[i]);
        assert(callCount == expectedCounts[i]);
    }
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]); assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    for (unsigned int i = 0; i < 5; ++i) {
        const std::string baseline = schemaTrace(names[i], initializers[i]);
        testFailures(names[i], initializers[i], expectedCounts[i]);
        for (int at = 0; at < expectedCounts[i]; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(initializers[i]() == MS::kFailure && callCount == at + 1);
            assert(schemaTrace(names[i], initializers[i]) == baseline);
        }
    }
    std::cout << "Generator registration: frozen 416 operations, 832 status failures and 416 exceptions passed\n";
}
