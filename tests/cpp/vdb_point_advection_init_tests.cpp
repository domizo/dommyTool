#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Only used to freeze the original unchecked initialization schema.
#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus s = (x);                                                                     \
        (void)s;                                                                                   \
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
struct DPData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
struct DCurveData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
#include "vdb_point_advection_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto initialize = DOpenVDBAdvectPoints::initialize;
    const std::string trace = schemaTrace("DOpenVDBAdvectPoints", initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DOpenVDBAdvectPoints", initialize, 84);
    for (int failure = 0; failure < 84; ++failure) {
        resetCalls();
        attributeThrowAt = failure;
        assert(initialize() == MS::kFailure && callCount == failure + 1);
        assert(schemaTrace("DOpenVDBAdvectPoints", initialize) == trace);
    }
    std::cout
        << "Point advection: 84 frozen operations, 168 failed statuses, 84 exceptions passed\n";
}
