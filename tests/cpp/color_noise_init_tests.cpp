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
struct DPData {
    static const TypeId id;
};
const TypeId DPData::id = {1};
struct DCurveData {
    static const TypeId id;
};
const TypeId DCurveData::id = {2};
struct DMeshData {
    static const TypeId id;
};
const TypeId DMeshData::id = {3};
struct ColorNumeric : MFnNumericAttribute {
    using MFnNumericAttribute::setDefault;
    MObject createColor(const char *name, const char *shortName, MStatus *status = nullptr) {
        return bind(name, shortName, "color", status);
    }
    MStatus setDefault(double a, double b, double c, double d) {
        return operation(name + " default " + text(a) + " " + text(b) + " " + text(c) + " " +
                         text(d));
    }
};
#define MFnNumericAttribute ColorNumeric
#include "color_noise_init.inc"

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string trace =
        schemaTrace("DPColor", DPColor::initialize) + schemaTrace("DPNoise", DPNoise::initialize) +
        schemaTrace("DPDFNoise", DPDFNoise::initialize) +
        schemaTrace("DPPyroNoise", DPPyroNoise::initialize) +
        schemaTrace("DPFastNoise", DPFastNoise::initialize) +
        schemaTrace("DCColor", DCColor::initialize) + schemaTrace("DCNoise", DCNoise::initialize);
    if (std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    std::ifstream file(argv[1]);
    assert(file.good());
    assert(trace == std::string((std::istreambuf_iterator<char>(file)), {}));
    int operationsTested = 0;
    for (auto initialize :
         {DPColor::initialize, DPNoise::initialize, DPDFNoise::initialize, DPPyroNoise::initialize,
          DPFastNoise::initialize, DCColor::initialize, DCNoise::initialize}) {
        resetCalls();
        assert(initialize());
        const int total = callCount;
        operationsTested += total;
        for (int i = 0; i < total; ++i) {
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                resetCalls(i);
                injectedStatus = code;
                assert(initialize() == code && callCount == i + 1);
            }
            resetCalls();
            attributeThrowAt = i;
            assert(initialize() == MS::kFailure && callCount == i + 1);
            resetCalls();
            assert(initialize());
        }
    }
    std::cout << "Color/noise: frozen " << operationsTested << "-operation schema, "
              << operationsTested * 2 << " failed statuses, " << operationsTested
              << " exceptions passed\n";
}
