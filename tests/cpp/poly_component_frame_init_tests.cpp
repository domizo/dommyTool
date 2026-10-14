#include "maya_attribute_doubles.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include <fstream>
#include <iostream>
#include <iterator>
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "poly_component_frame_init.inc"

int main(int argc, char** argv) {
    assert(argc==2);
    const std::string schema=schemaTrace("DPolyFaceToMat",DPolyFaceToMat::initialize)+
        schemaTrace("DPolyEdgeToMat",DPolyEdgeToMat::initialize);
    if(std::string(argv[1])=="--emit") { std::cout<<schema;return 0; }
    std::ifstream file(argv[1]);assert(file.good());
    assert(schema==std::string((std::istreambuf_iterator<char>(file)),{}));
    const char* names[] = {"DPolyFaceToMat", "DPolyEdgeToMat"};
    MStatus (*initializers[])() = {DPolyFaceToMat::initialize, DPolyEdgeToMat::initialize};
    const int counts[] = {33, 34};
    for (int node = 0; node < 2; ++node) {
        const std::string trace = schemaTrace(names[node], initializers[node]);
        testFailures(names[node], initializers[node], counts[node]);
        for (int at = 0; at < counts[node]; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(initializers[node]() == MS::kFailure && callCount == at + 1);
            assert(schemaTrace(names[node], initializers[node]) == trace);
        }
    }
    std::cout << "Poly frame initialization: 67 frozen operations, 134 status failures, 67 exceptions and valid retries passed\n";
}
