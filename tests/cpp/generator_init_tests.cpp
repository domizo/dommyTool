#include "maya_attribute_doubles.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#include <iterator>

// Capture the historical schema before adding fail-fast propagation.
#define CHECK_MSTATUS(value) do { (void)(value); } while (false)
namespace MFnGeometryData { using MFnData::kNurbsCurve; }
struct MFnStringData {
    MObject create(const char*, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status) *status = result;
        return {"default"};
    }
};
#include "generator_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DHEyelashGen", DHEyelashGen::initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream fixture(argv[1]);
    const std::string expected((std::istreambuf_iterator<char>(fixture)), {});
    assert(fixture && trace == expected);
    const int count = callCount;
    testFailures("DHEyelashGen", DHEyelashGen::initialize, count);
    for (int index = 0; index < count; ++index) {
        resetCalls(); attributeThrowAt = index;
        assert(DHEyelashGen::initialize() == MS::kFailure);
        assert(callCount == index + 1);
    }
    std::cout << "Eyelash: frozen schema, " << count
              << " status and exception boundaries passed\n";
}
