#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#define CHECK_MSTATUS(x)                                                                           \
    {                                                                                              \
        const MStatus s = (x);                                                                     \
        if (!s)                                                                                    \
            s.pAPIerror("probe", __LINE__);                                                        \
    }
struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const MStatus s = operation("string.create");
        if (status)
            *status = s;
        return s ? MObject{value} : MObject::kNullObj;
    }
};
struct TypeId {
    int value;
    int id() const {
        return value;
    }
    operator MFnData::Type() const {
        return MFnData::kAny;
    }
};
struct DPData {
    static const TypeId id;
};
struct DCurveData {
    static const TypeId id;
};
struct DMeshData {
    static const TypeId id;
};
struct DOpenVDBData {
    static const TypeId id;
};
const TypeId DPData::id{1}, DCurveData::id{2}, DMeshData::id{3}, DOpenVDBData::id{4};
#include "vdb_smoothing_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPAttrSmooth", DPAttrSmooth::initialize) +
                              schemaTrace("DOpenVDBSamplingDP", DOpenVDBSamplingDP::initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    const int failures = testFailures("DPAttrSmooth", DPAttrSmooth::initialize, 55) +
                         testFailures("DOpenVDBSamplingDP", DOpenVDBSamplingDP::initialize, 17);
    int exceptions = 0;
    for (auto initialize : {DPAttrSmooth::initialize, DOpenVDBSamplingDP::initialize}) {
        resetCalls();
        assert(initialize());
        const int count = callCount;
        for (int i = 0; i < count; ++i) {
            resetCalls();
            attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1);
            ++exceptions;
            resetCalls();
            assert(initialize());
        }
    }
    std::cout << "VDB smoothing/sampling: frozen 72-operation schema, " << failures
              << " failed statuses and " << exceptions << " exceptions passed\n";
}
