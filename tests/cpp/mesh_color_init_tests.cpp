#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include <fstream>
#include <iostream>
#include <iterator>
#define CHECK_MSTATUS(s) { (void)(s); }
#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
#include "mesh_color_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPolyThickness", DPolyThickness::initialize) +
                              schemaTrace("DStretchData", DStretchData::initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream fixture(argv[1]);
    assert(fixture);
    assert(trace == std::string(std::istreambuf_iterator<char>(fixture), {}));
    testFailures("DPolyThickness", DPolyThickness::initialize, 11);
    testFailures("DStretchData", DStretchData::initialize, 45);
    for (auto fn : {DPolyThickness::initialize, DStretchData::initialize}) {
        resetCalls(); assert(fn()); const int count = callCount;
        for (int at = 0; at < count; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(!fn()); assert(callCount == at + 1);
        }
    }
    std::cout << "Color-node initialization: frozen 56 operations, failure/exception checks passed\n";
}
