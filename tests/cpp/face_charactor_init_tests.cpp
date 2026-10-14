#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#include <iterator>
#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
// Only the custom-data overload is represented as a mesh type in this double.
// Production IDs/registration are covered separately by node-contract tests.
struct DMeshData { static const MFnData::Type id = MFnData::kMesh; };
struct MFnStringData {
    MObject create(const char* text, MStatus* status) {
        *status = operation(std::string("string.default ") + text);
        return {text};
    }
};
#include "face_charactor_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPolyFaceInfo", DPolyFaceInfo::initialize) +
                              schemaTrace("DMCharactor", DMCharactor::initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]); assert(file);
    assert(trace == std::string(std::istreambuf_iterator<char>(file), {}));
    testFailures("DPolyFaceInfo", DPolyFaceInfo::initialize, 26);
    testFailures("DMCharactor", DMCharactor::initialize, 41);
    for (auto initialize : {DPolyFaceInfo::initialize, DMCharactor::initialize}) {
        resetCalls(); assert(initialize()); const int count = callCount;
        for (int at = 0; at < count; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(!initialize() && callCount == at + 1);
        }
    }
    std::cout << "Face/Charactor initialization: 67 frozen operations, 134 failures, 67 exceptions passed\n";
}
