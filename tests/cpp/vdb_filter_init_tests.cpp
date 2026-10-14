#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Used only when freezing the original, unchecked initializer trace.
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
#include "vdb_filter_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto general = DOpenVDBFilter::initialize;
    const auto level = DOpenVDBFilterLevelSet::initialize;
    const std::string trace =
        schemaTrace("DOpenVDBFilter", general) + schemaTrace("DOpenVDBFilterLevelSet", level);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    const int counts[] = {41, 116};
    MStatus (*initializers[])() = {general, level};
    const char* names[] = {"DOpenVDBFilter", "DOpenVDBFilterLevelSet"};
    for (int n = 0; n < 2; ++n) {
        testFailures(names[n], initializers[n], counts[n]);
        for (int failure = 0; failure < counts[n]; ++failure) {
            resetCalls();
            attributeThrowAt = failure;
            assert(initializers[n]() == MS::kFailure && callCount == failure + 1);
            assert(schemaTrace("DOpenVDBFilter", general) +
                       schemaTrace("DOpenVDBFilterLevelSet", level) ==
                   trace);
        }
    }
    std::cout << "VDB filters: 157 frozen operations, 314 failed statuses, 157 exceptions passed\n";
}
