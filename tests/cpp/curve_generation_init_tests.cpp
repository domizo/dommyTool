#define MFnTypedAttribute BaseTyped
#include "maya_attribute_doubles.h"
#undef MFnTypedAttribute
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "modules/point/DPoint/dpoint_ids.h"
#include "modules/curve/DCurve/dcurve_ids.h"
#include <fstream>
#include <iostream>
#include <iterator>

struct TypeId { unsigned value; };
struct DPData { static const TypeId id; };
const TypeId DPData::id = {ID_DP_DATA};
struct DCurveData { static const TypeId id; };
const TypeId DCurveData::id = {ID_DC_DATA};
struct MFnTypedAttribute : BaseTyped {
    using BaseTyped::create;
    MObject create(const char* name, const char* shortName, TypeId type,
                   const MObject& value, MStatus* status) {
        return bind(name, shortName, "plugin " + text(type.value) + " " + value.name, status);
    }
};
struct MFnStringData {
    MObject create(const char*, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status) *status = result;
        return {"default"};
    }
};
// Allows the pre-change successful schema to be captured without editing it.
#define CHECK_MSTATUS(value) do { (void)(value); } while (false)
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "curve_generation_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    std::string schema;
    const char* names[] = {"DCurveFromDP", "DCurveCircleGen", "DCurveFromSurface", "DCurveClothFromSurface"};
    MStatus (*initializers[])() = {DCurveFromDP::initialize, DCurveCircleGen::initialize,
                                  DCurveFromSurface::initialize, DCurveClothFromSurface::initialize};
    const int counts[] = {80, 48, 76, 48};
    for (int i=0; i<4; ++i) {
        schema += schemaTrace(names[i], initializers[i]);
        assert(callCount == counts[i]);
    }
    if (std::string(argv[1]) == "--emit") { std::cout << schema; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(schema == std::string((std::istreambuf_iterator<char>(file)), {}));
    for (int i=0; i<4; ++i) {
        testFailures(names[i], initializers[i], counts[i]);
        for (int at=0; at<counts[i]; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(initializers[i]() == MS::kFailure && callCount == at + 1);
        }
    }
    std::cout << "Curve generators: 252 frozen operations, 504 status failures, 252 exceptions passed\n";
}
