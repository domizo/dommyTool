#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Retained only to capture the pre-fix attribute contract with --emit.
#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus s = (x);                                                                     \
        if (!s)                                                                                    \
            s.pAPIerror("test", __LINE__);                                                         \
    } while (false)
struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status)
            *status = result;
        return result ? MObject{value} : MObject::kNullObj;
    }
};
struct DOpenVDBData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
#include "vdb_noise_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto init = DOpenVDBNoise::initialize;
    const std::string trace = schemaTrace("DOpenVDBNoise", init);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DOpenVDBNoise", init, 102);
    for (int failure = 0; failure < 102; ++failure) {
        resetCalls();
        attributeThrowAt = failure;
        assert(init() == MS::kFailure && callCount == failure + 1);
        assert(schemaTrace("DOpenVDBNoise", init) == trace);
    }
    std::cout << "DVDBNoise: frozen 102 operations, 204 failed statuses, 102 exceptions passed\n";
}
