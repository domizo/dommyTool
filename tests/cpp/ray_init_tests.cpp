#define DPData OriginalDPData
#define DCurveData OriginalDCurveData
#define DMeshData OriginalDMeshData
#include "group_transfer_init_doubles.h"
#undef DPData
#undef DCurveData
#undef DMeshData
struct TypeId {
    int value;
    int id() const { return value; }
    operator MFnData::Type() const { return MFnData::kAny; }
};
struct DPData { static const TypeId id; }; const TypeId DPData::id = {1};
struct DCurveData { static const TypeId id; }; const TypeId DCurveData::id = {2};
struct DMeshData { static const TypeId id; }; const TypeId DMeshData::id = {3};
#include "ray_init.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPRay", DPRay::initialize);
    if (std::string(argv[1]) == "--emit") { std::cout << trace; return 0; }
    std::ifstream file(argv[1]); assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    resetCalls(); assert(DPRay::initialize()); const int count = callCount;
    for (int i = 0; i < count; ++i) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            resetCalls(i); injectedStatus = code;
            assert(DPRay::initialize() == code && callCount == i + 1);
        }
        resetCalls(); attributeThrowAt = i;
        assert(DPRay::initialize() == MS::kFailure && callCount == i + 1);
    }
    std::cout << "Ray: frozen schema, " << count * 2 << " status failures, "
              << count << " exceptions passed\n";
}
