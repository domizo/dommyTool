#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Used only to freeze the pre-hardening schema.
#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus status = (x);                                                                \
        (void)status;                                                                              \
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
#include "vdb_fill_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto initialize = DOpenVDBFill::initialize;
    const std::string trace = schemaTrace("DOpenVDBFill", initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DOpenVDBFill", initialize, 52);
    for (int failure = 0; failure < 52; ++failure) {
        resetCalls();
        attributeThrowAt = failure;
        assert(initialize() == MS::kFailure && callCount == failure + 1);
        assert(schemaTrace("DOpenVDBFill", initialize) == trace);
    }
    std::cout
        << "Fill initializer: 52 frozen operations, 104 failed statuses, 52 exceptions passed\n";
}
