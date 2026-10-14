#include "mesh_emitter_doubles.h"
#include "common/utility/DSpatialSearchPlan.h"
#include "common/utility/DColorMode.h"
#include <fstream>
#include <iostream>

enum { DM_C_POINT_PIECE, DM_C_PRIM_CENTER_PIECE, DM_C_ALL_POINT };
enum { DM_C_TAR_DPOINT, DM_C_TAR_DCURVE, DM_C_TAR_DMESH };
enum { D_COMP_POINT, D_COMP_EDGE, D_COMP_FACE, D_COMP_PRIM };
struct ColorNumericAttribute : MFnNumericAttribute {
    using MFnNumericAttribute::setDefault;
    MObject createColor(const char* name, const char* shortName, MStatus* s = nullptr) {
        return bind(name, shortName, "color", s);
    }
    MStatus setDefault(double a, double b, double c, double d) {
        return operation(name + " default " + text(a) + " " + text(b) + " " + text(c) + " " +
                         text(d));
    }
};
#define MFnNumericAttribute ColorNumericAttribute

#ifdef DOMMY_INITIALIZATION_ONLY
#include "mesh_controls.inc"
int main() {
    std::cout << schemaTrace("DMColor", DMColor::initialize)
              << schemaTrace("DMConnetedPieces", DMConnetedPieces::initialize)
              << schemaTrace("DMConnectTargets", DMConnectTargets::initialize)
              << schemaTrace("DMInfo", DMInfo::initialize);
}
#else
// The physical palette traversal is independent of sparse logical indices.
static std::vector<MVector> palette;
static unsigned reportedCount = 0;
struct MColor : MVector {
    MColor(double x = 0, double y = 0, double z = 0) : MVector(x, y, z) {}
};
using MColorArray = Array<MColor>;
struct MArrayDataHandle {
    unsigned index = 0;
    unsigned elementCount(MStatus* s) {
        *s = api("palette.count");
        return reportedCount ? reportedCount : palette.size();
    }
    MDataHandle inputValue(MStatus* s) {
        *s = api("palette.value");
        MDataHandle h;
        h.valid = bool(*s);
        h.input = &inputs["palette"];
        for (int i = 0; i < 3; ++i)
            h.input->vec[i] = palette.at(index)[i];
        return h;
    }
    MStatus next() {
        const auto s = api("palette.next");
        if (s)
            ++index;
        return s;
    }
};
struct PaletteBlock : MDataBlock {
    MArrayDataHandle inputArrayValue(MObject, MStatus* s) {
        *s = api("palette.input");
        return {};
    }
};
#define MDataBlock PaletteBlock
#include "mesh_controls.inc"

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DMColor", DMColor::initialize) +
                              schemaTrace("DMConnetedPieces", DMConnetedPieces::initialize) +
                              schemaTrace("DMConnectTargets", DMConnectTargets::initialize) +
                              schemaTrace("DMInfo", DMInfo::initialize);
    std::ifstream file(argv[1]);
    assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)), {});
    assert(trace == expected);
    const int failures = testFailures("DMColor", DMColor::initialize, 91) +
                         testFailures("DMConnetedPieces", DMConnetedPieces::initialize, 40) +
                         testFailures("DMConnectTargets", DMConnectTargets::initialize, 69) +
                         testFailures("DMInfo", DMInfo::initialize, 24);
    std::cout << "Mesh initialization: " << failures << " injected status failures passed\n";
    PaletteBlock data;
    for (auto reader : {readDMConnectTargets, readDMRemesh}) {
        reset();
        assert(reader(data) == MS::kSuccess);
        const int count = callCount;
        for (int i = 0; i < count; ++i) {
            reset();
            failAt = i;
            assert(!reader(data));
            assert(callCount == i + 1);
        }
    }
    reset();
    palette = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    MColorArray colors;
    assert(readPalette(data, colors) == MS::kSuccess && colors.length() == 3);
    assert(colors[0].x == 1 && colors[1].y == 1 && colors[2].z == 1);
    const int count = callCount;
    for (int i = 0; i < count; ++i) {
        reset();
        failAt = i;
        MColorArray out;
        assert(!readPalette(data, out));
        assert(callCount == i + 1);
    }
    reset();
    shortAt = 2;
    assert(!readPalette(data, colors));
    reset();
    reportedCount = dommy::generation::kMaxGeneratedPointCount + 1;
    assert(!readPalette(data, colors));
    reportedCount = 0;
    reset();
    palette.clear();
    assert(!readPalette(data, colors));
    reset();
    palette = {{NAN, 0, 0}};
    assert(!readPalette(data, colors));
    std::cout << "Mesh input controls and bounded palette failure paths passed\n";
}
#endif
