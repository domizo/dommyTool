#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
// For emitting the original contract before hardening only.
#define CHECK_MSTATUS(x) do {const MStatus result=(x);(void)result;} while(false)
struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status) *status = result;
        return result ? MObject{value} : MObject::kNullObj;
    }
};
struct DOpenVDBData {static constexpr MFnData::Type id = MFnData::kAny;};
struct DPData {static constexpr MFnData::Type id = MFnData::kAny;};
namespace openvdb {
enum GridClass {GRID_LEVEL_SET};
struct GridBase {
    static std::string gridClassToString(GridClass) {return "level set";}
    static std::string gridClassToMenuName(GridClass) {return "Level Set";}
};
}
#include "vdb_generation_init.inc"
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string schema = schemaTrace("DOpenVDBPlatonic", DOpenVDBPlatonic::initialize) +
                               schemaTrace("DOpenVDBFracture", DOpenVDBFracture::initialize);
    if (std::string(argv[1]) == "--emit") {std::cout << schema; return 0;}
    std::ifstream file(argv[1]); assert(file.good());
    assert(schema == std::string((std::istreambuf_iterator<char>(file)), {}));
    const int counts[] = {55,57}; int n = 0;
    for (auto initialize : {DOpenVDBPlatonic::initialize, DOpenVDBFracture::initialize}) {
        const int count = counts[n++];
        const auto trace = schemaTrace("node", initialize);
        testFailures("node", initialize, count);
        for (int i = 0; i < count; ++i) {
            resetCalls(); attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1);
            assert(schemaTrace("node", initialize) == trace);
        }
    }
    std::cout << "Sphere/fracture: frozen 55/57 operations, 224 status failures, 112 exceptions passed\n";
}
