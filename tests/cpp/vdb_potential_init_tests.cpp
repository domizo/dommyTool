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
namespace openvdb { namespace math {
// OpenVDB 5.2 Math.h specialization, not a solver approximation.
template<typename T> struct Delta;
template<> struct Delta<float> { static float value() { return 1e-5f; } };
}}
#include "vdb_potential_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto initialize = DVDBPotentialFlow::initialize;
    const std::string trace = schemaTrace("DVDBPotentialFlow", initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DVDBPotentialFlow", initialize, 89);
    for (int failure = 0; failure < 89; ++failure) {
        resetCalls(); attributeThrowAt = failure;
        assert(initialize() == MS::kFailure && callCount == failure + 1);
        assert(schemaTrace("DVDBPotentialFlow", initialize) == trace);
    }
    std::cout << "Potential Flow initializer: 89 schema operations, 178 statuses, 89 exceptions passed\n";
}

