#define MFnNumericAttribute BaseNumeric
#define MFnTypedAttribute BaseTyped
#include "maya_attribute_doubles.h"
#undef MFnNumericAttribute
#undef MFnTypedAttribute
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
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
struct MFnNumericAttribute : BaseNumeric {
    using BaseNumeric::setMin;
    MStatus setMin(double x, double y, double z) {
        return operation(name + " min " + text(x) + " " + text(y) + " " + text(z));
    }
};
struct MFnStringData {
    MObject create(const char*, MStatus* status = nullptr) {
        const MStatus result = operation("string.create");
        if (status) *status = result;
        return {"default"};
    }
};
struct MFnMessageAttribute {};
// Retained only for capturing the original log-only initialization schema.
#define CHECK_MSTATUS(value) do { (void)(value); } while (false)
#include "fur_aho_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string fur = schemaTrace("DCFurGen", DCFurGen::initialize);
    assert(callCount == 100);
    const std::string aho = schemaTrace("DCAhoGen", DCAhoGen::initialize);
    assert(callCount == 97);
    if (std::string(argv[1]) == "--emit") { std::cout << fur << aho; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(fur + aho == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DCFurGen", DCFurGen::initialize, 100);
    testFailures("DCAhoGen", DCAhoGen::initialize, 97);
    for (auto initialize : {DCFurGen::initialize, DCAhoGen::initialize}) {
        resetCalls(); assert(initialize()); const int count = callCount;
        for (int at = 0; at < count; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(initialize() == MS::kFailure && callCount == at + 1);
        }
    }
    std::cout << "Fur/Aho: frozen 197 operations, 394 status failures and 197 exceptions passed\n";
}
