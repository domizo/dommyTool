#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Used only to freeze the pre-hardening schema.
#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus observed = (x);                                                              \
        (void)observed;                                                                            \
    } while (false)
struct MFnStringData {
    MObject create(const char* text, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status)
            *status = result;
        return result ? MObject{text} : MObject::kNullObj;
    }
};
struct MFnMeshData {
    MObject create(MStatus* status) {
        *status = operation("mesh.create");
        return *status ? MObject{"mesh"} : MObject::kNullObj;
    }
};
struct DOpenVDBData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
#include "vdb_clip_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto initialize = DOpenVDBClip::initialize;
    const std::string trace = schemaTrace("DOpenVDBClip", initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DOpenVDBClip", initialize, 56);
    for (int failure = 0; failure < 56; ++failure) {
        resetCalls();
        attributeThrowAt = failure;
        assert(initialize() == MS::kFailure && callCount == failure + 1);
        assert(schemaTrace("DOpenVDBClip", initialize) == trace);
    }
    std::cout
        << "Clip initializer: 56 frozen operations, 112 failed statuses, 56 exceptions passed\n";
}
