#include "maya_attribute_doubles.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include <fstream>
#include <iostream>
#include <iterator>

struct MFnNurbsCurveData { static const MFnData::Type kMesh = MFnData::kMesh; };
#define CHECK_MSTATUS(value) { (void)(value); }
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "legacy_mesh_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* names[] = {"DBSMirrorMesh", "DBSFlipMesh", "DBSUpDownMesh", "DPolySymmetry", "DPolyResetVtx"};
    MStatus (*initializers[])() = {DBSMirrorMesh::initialize, DBSFlipMesh::initialize,
        DBSUpDownMesh::initialize, DPolySymmetry::initialize, DPolyResetVtx::initialize};
    const int counts[] = {36, 42, 38, 34, 32};
    std::string schema;
    for (int i = 0; i < 5; ++i) {
        schema += schemaTrace(names[i], initializers[i]);
        assert(callCount == counts[i]);
    }
    if (std::string(argv[1]) == "--emit") { std::cout << schema; return 0; }
    std::ifstream file(argv[1]); assert(file.good());
    assert(schema == std::string((std::istreambuf_iterator<char>(file)), {}));
    for (int i = 0; i < 5; ++i) {
        testFailures(names[i], initializers[i], counts[i]);
        for (int at = 0; at < counts[i]; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(initializers[i]() == MS::kFailure && callCount == at + 1);
        }
    }
    std::cout << "Legacy mesh initialization: 182 frozen operations, 364 failures, 182 exceptions passed\n";
}
