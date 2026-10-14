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
#include "vdb_divergence_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto initialize = DOpenVDBRemoveDivergence::initialize;
    const std::string trace = schemaTrace("DOpenVDBRemoveDivergence", initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DOpenVDBRemoveDivergence", initialize, 61);
    for (int failure = 0; failure < 61; ++failure) {
        resetCalls(); attributeThrowAt = failure;
        assert(initialize() == MS::kFailure && callCount == failure + 1);
        assert(schemaTrace("DOpenVDBRemoveDivergence", initialize) == trace);
    }
    std::cout << "Remove Divergence initializer: 61 schema operations, 122 statuses, 61 exceptions passed\n";
}
