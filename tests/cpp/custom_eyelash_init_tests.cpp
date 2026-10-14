#define MFnTypedAttribute BaseTypedAttribute
#include "maya_attribute_doubles.h"
#undef MFnTypedAttribute
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DStatusOperation.h"
#include "modules/curve/DCurve/dcurve_ids.h"
#include <fstream>
#include <iostream>
#include <iterator>

// Extend the shared attribute double for a custom-data type ID.
struct CurveTypeId { unsigned int value; };
struct DCurveData { static const CurveTypeId id; };
const CurveTypeId DCurveData::id = {ID_DC_DATA};
struct MFnTypedAttribute : BaseTypedAttribute {
    MObject create(const char* longName, const char* shortName, CurveTypeId type,
                   const MObject& value, MStatus* status) {
        return bind(longName, shortName,
                    "plugin " + text(type.value) + " " + value.name, status);
    }
};
// Only needed to capture the original log-only initializer before correction.
#define CHECK_MSTATUS(value) do { (void)(value); } while (false)
#include "custom_eyelash_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const auto initialize = DCEyelashGen::initialize;
    const std::string trace = schemaTrace("DCEyelashGen", initialize);
    assert(callCount == 167);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    testFailures("DCEyelashGen", initialize, 167);
    for (int at = 0; at < 167; ++at) {
        resetCalls(); attributeThrowAt = at;
        assert(initialize() == MS::kFailure && callCount == at + 1);
        assert(schemaTrace("DCEyelashGen", initialize) == trace);
    }
    std::cout << "Custom Eyelash: frozen 167 operations, 334 status failures and 167 exceptions passed\n";
}
