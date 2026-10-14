#define MFnTypedAttribute BaseTyped
#include "maya_attribute_doubles.h"
#undef MFnTypedAttribute
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "modules/curve/DCurve/Generate/DTextGenerationPlan.h"
#include <fstream>
#include <iostream>
#include <iterator>

struct TypeId { unsigned value; };
struct DPData { static const TypeId id; };
const TypeId DPData::id = {1};
struct DCurveData { static const TypeId id; };
const TypeId DCurveData::id = {2};
struct MFnTypedAttribute : BaseTyped {
    using BaseTyped::create;
    MObject create(const char* a, const char* b, TypeId type, const MObject& c, MStatus* s) {
        return bind(a, b, "plugin " + text(type.value) + " " + c.name, s);
    }
};
struct MFnMessageAttribute : Attribute {
    MObject create(const char* a, const char* b, MStatus* s) { return bind(a, b, "message", s); }
};
struct MFnStringData {
    MObject create(const char* value, MStatus* status) {
        *status = operation("string.create " + std::string(value));
        return {value};
    }
};
#define CHECK_MSTATUS(value) do { (void)(value); } while (false)
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "curve_source_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* names[] = {"DCurveFromCurve", "DCurveLinkGen", "DCurveFromMesh", "DCurveFromPfx", "DCText"};
    MStatus (*initializers[])() = {DCurveFromCurve::initialize, DCurveLinkGen::initialize,
        DCurveFromMesh::initialize, DCurveFromPfx::initialize, DCText::initialize};
    const int counts[] = {109, 125, 11, 58, 42};
    std::string schema;
    for (int i=0; i<5; ++i) {
        schema += schemaTrace(names[i], initializers[i]);
        assert(callCount == counts[i]);
    }
    if (std::string(argv[1]) == "--emit") { std::cout << schema; return 0; }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(schema == std::string((std::istreambuf_iterator<char>(file)), {}));
    for (int i=0; i<5; ++i) {
        testFailures(names[i], initializers[i], counts[i]);
        for (int at=0; at<counts[i]; ++at) {
            resetCalls(); attributeThrowAt = at;
            assert(initializers[i]() == MS::kFailure && callCount == at + 1);
        }
    }
    std::cout << "Source curve initializers: 345 frozen operations, 690 status failures, 345 exceptions passed\n";
}
