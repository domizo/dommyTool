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
struct DOpenVDBData {
    static constexpr MFnData::Type id = MFnData::kAny;
};
// Only the external enum/label boundary is modelled. Production retains the
// exact vecTypeExamples call; this fixture does not emulate VDB registration.
namespace openvdb {
enum VecType { V0, V1, V2, V3, V4 };
const int NUM_VEC_TYPES = 5;
struct GridBase {
    static std::string vecTypeExamples(VecType type) {
        return "enum_" + text(int(type));
    }
};
} // namespace openvdb
#include "vdb_vector_merge_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto init = DOpenVDBVectorMerge::initialize;
    const std::string trace = schemaTrace("DOpenVDBVectorMerge", init);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    const int failures = testFailures("DOpenVDBVectorMerge", init, 49);
    for (int i = 0; i < 49; ++i) {
        resetCalls();
        attributeThrowAt = i;
        assert(init() == MS::kFailure && callCount == i + 1);
        assert(schemaTrace("DOpenVDBVectorMerge", init) == trace);
    }
    std::cout << "VectorMerge: frozen 49-operation schema, " << failures
              << " failed statuses and 49 exceptions passed\n";
}
