#define DPData OriginalDPData
#define DCurveData OriginalDCurveData
#define DMeshData OriginalDMeshData
#define MFnMatrixAttribute OriginalMatrixAttribute
#include "group_transfer_init_doubles.h"
#undef DPData
#undef DCurveData
#undef DMeshData
#undef MFnMatrixAttribute
struct MFnMatrixAttribute : Attribute {
    enum Type { kFloat };
    MObject create(const char* longName,const char* shortName,Type,MStatus* status=nullptr) {
        return bind(longName,shortName,"matrix float",status);
    }
};
struct TypeId {
    int value;
    int id() const { return value; }
    operator MFnData::Type() const { return MFnData::kAny; }
};
struct DPData { static const TypeId id; }; const TypeId DPData::id = {1};
struct DCurveData { static const TypeId id; }; const TypeId DCurveData::id = {2};
struct DMeshData { static const TypeId id; }; const TypeId DMeshData::id = {3};
enum { DP_ATTR_CLASS_POINT, DP_ATTR_CLASS_PRIMITIVE };
const double kMFnNurbsEpsilon=1e-6;
#include "affect_spiral_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPAffect", DPAffect::initialize) +
        schemaTrace("DPSpiralField", DPSpiralField::initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]); assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    int failures = 0, exceptions = 0;
    for (auto initialize : {DPAffect::initialize, DPSpiralField::initialize}) {
        resetCalls(); assert(initialize()); const int count = callCount;
        for (int i = 0; i < count; ++i) {
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                resetCalls(i); injectedStatus = code;
                assert(initialize() == code && callCount == i + 1); ++failures;
            }
            resetCalls(); attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1); ++exceptions;
        }
    }
    std::cout << "Affect/spiral: frozen schema, " << failures << " status failures, "
              << exceptions << " exceptions passed\n";
}
