#include "maya_attribute_doubles.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>

// Also permits capturing the pre-fix initializer trace without rewriting it.
#define CHECK_MSTATUS_AND_RETURN_IT(value) do {if (!(value)) return (value);} while (false)
struct MFnStringData {
    MObject create(const char* value, MStatus* status) {
        *status = operation(std::string("string.create ") + value);
        return *status ? MObject{value} : MObject::kNullObj;
    }
};
struct DMeshData {static constexpr MFnData::Type id = MFnData::kAny;};
#include "mesh_convert_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto trace = schemaTrace("DMConvertFromMeshes", DMConvertFromMeshes::initialize);
    const int count = callCount;
    if (std::string(argv[1]) == "--emit") {std::cout << trace; return 0;}
    std::ifstream file(argv[1]); assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    assert(count == 40);
    testFailures("DMConvertFromMeshes", DMConvertFromMeshes::initialize, count);
    for (int i = 0; i < count; ++i) {
        resetCalls(); attributeThrowAt = i;
        assert(DMConvertFromMeshes::initialize() == MS::kFailure && callCount == i + 1);
        assert(schemaTrace("DMConvertFromMeshes", DMConvertFromMeshes::initialize) == trace);
    }
    std::cout << "DMConvertFromMeshes: frozen 40 operations, 80 status failures and 40 exceptions passed\n";
}
