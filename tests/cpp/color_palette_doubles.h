#pragma once
// DG arrays and geometry publication are doubles. Palette reading, array
// checking, all three assignment loops and seeded random generation are real.
#include "mesh_emitter_doubles.h"

#include "common/utility/DColorMode.h"
#include "common/utility/DMayaCheckedArray.h"
#include "math/DRandomGenerator.h"
static std::vector<MVector> palette;
static unsigned int reportedPaletteCount = 0;
struct MColor : MVector {
    MColor(double x = 0, double y = 0, double z = 0) : MVector(x, y, z) {}
};
using MColorArray = Array<MColor>;
using DColor = MVector;
using DVec3Data = MVector;
using DColorList = std::vector<MVector>;
using DVec3DataList = DColorList;
static const MObject aPaletteColor{"palette"}, aPaletteType{"mode"}, aSeed{"seed"};
struct MArrayDataHandle {
    unsigned int position = 0;
    unsigned int elementCount(MStatus *status) {
        *status = api("palette.count");
        return reportedPaletteCount ? reportedPaletteCount : palette.size();
    }
    MDataHandle inputValue(MStatus *status) {
        *status = api("palette.value");
        MDataHandle h;
        h.valid = bool(*status);
        h.input = &inputs["palette"];
        for (int axis = 0; axis < 3; ++axis)
            h.input->vec[axis] = palette.at(position)[axis];
        return h;
    }
    MStatus next() {
        const auto status = api("palette.next");
        if (status)
            ++position;
        return status;
    }
};
struct PaletteBlock : MDataBlock {
    MArrayDataHandle inputArrayValue(MObject, MStatus *status) {
        *status = api("palette.input");
        return {};
    }
};
#define MDataBlock PaletteBlock
struct ColorGeometry {
    DPointList points;
    DColorList colors;
    std::vector<std::shared_ptr<ColorGeometry>> children;
    std::size_t numberOfPoints() const { return points.size(); }
    std::size_t numberOfCurves() const { return children.size(); }
    std::size_t numberOfMesh() const { return children.size(); }
    const DPointList *getPositionPtrConst() const { return &points; }
    auto beginCurves() -> decltype(children.begin()) { return children.begin(); }
    auto endCurves() -> decltype(children.end()) { return children.end(); }
    auto beginMeshes() -> decltype(children.begin()) { return children.begin(); }
    auto endMeshes() -> decltype(children.end()) { return children.end(); }
};
namespace dommy {
namespace color {
inline bool publishValues(ColorGeometry &output, const std::string &name, const DColorList &values,
                          std::size_t count) {
    assert(name == "Cd" && values.size() == count);
    if (!api("attribute.publish"))
        return false;
    output.colors = values;
    return true;
}
} // namespace color
} // namespace dommy
