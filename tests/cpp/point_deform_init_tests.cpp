#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#include <iterator>
// Only the attribute overload is doubled; production node/data IDs are
// checked separately by the registration contract tests.
struct DPData { static const MFnData::Type id = MFnData::kMesh; };
#include "point_deform_init.inc"
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPPointDeform", DPPointDeform::initialize);
    std::ifstream file(argv[1]); assert(file);
    assert(trace == std::string(std::istreambuf_iterator<char>(file), {}));
    testFailures("DPPointDeform", DPPointDeform::initialize, 31);
    for (int at = 0; at < 31; ++at) {
        resetCalls(); attributeThrowAt = at;
        assert(!DPPointDeform::initialize() && callCount == at + 1);
    }
    std::cout << "DPPointDeform: 31 frozen operations, 62 failures, 31 exceptions passed\n";
}
