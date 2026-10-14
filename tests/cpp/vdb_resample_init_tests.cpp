#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Retained only to emit the legacy schema before hardening.
#define CHECK_MSTATUS(x) do { const MStatus observed = (x); (void)observed; } while (false)
struct MFnStringData {
    MObject create(const char* text, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status) *status = result;
        return result ? MObject{text} : MObject::kNullObj;
    }
};
struct DOpenVDBData { static constexpr MFnData::Type id = MFnData::kAny; };
#include "vdb_resample_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto initialize = DOpenVDBResample::initialize;
    const std::string trace = schemaTrace("DOpenVDBResample", initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DOpenVDBResample", initialize, 76);
    for (int failure = 0; failure < 76; ++failure) {
        resetCalls(); attributeThrowAt = failure;
        assert(initialize() == MS::kFailure && callCount == failure + 1);
        assert(schemaTrace("DOpenVDBResample", initialize) == trace);
    }
    std::cout << "Resample initializer: 76 schema operations, 152 statuses, 76 exceptions passed\n";
}
