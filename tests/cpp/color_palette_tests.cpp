#include "color_palette_doubles.h"

// Generated production bodies require the doubles above. The frozen
// reference loops then share the generated palette/random helpers.
#include "color_palette.inc"

#include "../fixtures/color_palette_legacy_assignments.inc"

using Reader = MStatus (*)(MDataBlock &, int, ColorGeometry &);
ColorGeometry geometry() {
    ColorGeometry result;
    result.points.resize(7);
    for (int count : {4, 3}) {
        auto child = std::make_shared<ColorGeometry>();
        child->points.resize(count);
        result.children.push_back(child);
    }
    return result;
}
std::string signature(const ColorGeometry &result) {
    std::ostringstream stream;
    for (const auto &color : result.colors)
        stream << color[0] << ',' << color[1] << ',' << color[2] << ';';
    for (const auto &child : result.children)
        stream << '[' << signature(*child) << ']';
    return stream.str();
}
void setup(int mode = 0, int seed = 17) {
    reset();
    inputs["mode"].number = mode;
    inputs["seed"].number = seed;
    reportedPaletteCount = 0;
    palette = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
}
std::string assignments(bool baseline = false) {
    std::ostringstream out;
    MDataBlock data;
    for (int node = 0; node < 3; ++node)
        for (int assignment = 0; assignment < 2; ++assignment)
            for (int mode = 0; mode < 2; ++mode)
                for (int seed : {0, 17, 2147483647})
                    for (int count : {1, 3}) {
                        setup(mode, seed);
                        palette.resize(count);
                        auto candidate = geometry();
                        const Reader reader = baseline ? (node == 0   ? baselineDPColor
                                                          : node == 1 ? baselineDCColor
                                                                      : baselineDMColor)
                                                       : (node == 0   ? paletteDPColor
                                                          : node == 1 ? paletteDCColor
                                                                      : paletteDMColor);
                        assert(reader(data, assignment, candidate));
                        out << node << ' ' << assignment << ' ' << mode << ' ' << seed << ' '
                            << count << ' ' << signature(candidate) << '\n';
                    }
    return out.str();
}
int failures() {
    MDataBlock data;
    int tested = 0;
    for (auto reader : {paletteDPColor, paletteDCColor, paletteDMColor})
        for (int assignment = 0; assignment < 2; ++assignment)
            for (int mode = 0; mode < 2; ++mode) {
                setup(mode);
                auto candidate = geometry();
                assert(reader(data, assignment, candidate));
                const auto trace = operations;
                for (int index = 0; index < int(trace.size()); ++index) {
                    for (auto status : {MS::kFailure, MS::kInvalidParameter}) {
                        setup(mode);
                        failAt = index;
                        injectedStatus = status;
                        candidate = geometry();
                        assert(!reader(data, assignment, candidate) && callCount == index + 1);
                        ++tested;
                        if (trace[index] != "attribute.publish")
                            assert(candidate.colors.empty() &&
                                   candidate.children[0]->colors.empty());
                    }
                    setup(mode);
                    throwAt = index;
                    candidate = geometry();
                    assert(!reader(data, assignment, candidate) && callCount == index + 1);
                    ++tested;
                }
            }
    return tested;
}
void boundaries() {
    MDataBlock data;
    for (auto reader : {paletteDPColor, paletteDCColor, paletteDMColor}) {
        for (int invalid = 0; invalid < 6; ++invalid) {
            setup();
            auto candidate = geometry();
            if (invalid == 0)
                palette.clear();
            if (invalid == 1)
                palette[0][0] = NAN;
            if (invalid == 2)
                palette[1][1] = INFINITY;
            if (invalid == 3)
                reportedPaletteCount = dommy::generation::kMaxGeneratedPointCount + 1U;
            if (invalid == 4)
                reportedPaletteCount = std::numeric_limits<unsigned int>::max();
            if (invalid == 5)
                reportedPaletteCount = (256U * 1024U * 1024U) / (4U * sizeof(MColor)) + 1U;
            assert(!reader(data, 1, candidate));
            assert(candidate.colors.empty() && candidate.children[0]->colors.empty());
            if (invalid >= 3)
                assert(std::find(operations.begin(), operations.end(), "palette.value") ==
                       operations.end());
        }
        for (int failureIndex : {4, 10}) {
            // Control reads precede input/count; resize is #4, final copy #10.
            setup();
            shortAt = failureIndex;
            auto candidate = geometry();
            assert(!reader(data, 0, candidate));
            assert(shortApplied && candidate.colors.empty());
        }
        setup();
        auto empty = geometry();
        empty.points.clear();
        empty.children.clear();
        assert(reader(data, 0, empty));
    }
    setup();
    MColorArray result;
    assert(dommy::color::readPalette(data, aPaletteColor, result));
    assert(result.length() == 3 && result[0].x == 1 && result[1].y == 1 && result[2].z == 1);
}
int main(int argc, char **argv) {
    const std::string trace = assignments();
    if (argc == 2 && std::string(argv[1]) == "--emit") {
        std::cout << trace;
        return 0;
    }
    assert(trace == assignments(true));
    boundaries();
    const int count = failures();
    std::cout << "Color palette: 72 frozen assignments, bounds, short arrays and " << count
              << " injected failures passed\n";
}
