#include "maya_attribute_doubles.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaEntryPointGuard.h"
#include "common/utility/DGenerationLimits.h"
#include <fstream>
#include <iostream>
#include <iterator>

struct DCurveData { static const MFnData::Type id = MFnData::kAny; };
struct MFnNurbsCurveData { static const MFnData::Type kNurbsCurve = MFnData::kNurbsCurve; };
struct MFnMessageAttribute : Attribute {
    MObject create(const char* a, const char* b, MStatus* s) { return bind(a,b,"message",s); }
};
#define CHECK_MSTATUS(value) { (void)(value); }
#define CHECK_MSTATUS_AND_RETURN_IT(value) DOMMY_RETURN_IF_FAILURE(value)
#include "curve_conversion_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* names[] = {"DCConvertFromCurves", "DSpiralCurve", "woolCurve"};
    MStatus (*initializers[])() = {DCConvertFromCurves::initialize, DSpiralCurve::initialize, woolCurve::initialize};
    const int counts[] = {26,50,45};
    std::string schema;
    for (int i=0;i<3;++i) {
        schema += schemaTrace(names[i],initializers[i]);
        assert(callCount == counts[i]);
    }
    if (std::string(argv[1]) == "--emit") {std::cout<<schema;return 0;}
    std::ifstream file(argv[1]); assert(file.good());
    assert(schema == std::string((std::istreambuf_iterator<char>(file)), {}));
    for (int i=0;i<3;++i) {
        testFailures(names[i], initializers[i], counts[i]);
        for (int at=0;at<counts[i];++at) {
            resetCalls();attributeThrowAt=at;
            assert(initializers[i]() == MS::kFailure && callCount == at+1);
        }
    }
    std::cout<<"Curve conversion/legacy initialization: 121 frozen operations, 242 failures, 121 exceptions passed\n";
}
