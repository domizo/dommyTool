#include "noise/DWoodNoise.h"
#include "geometry/DFieldIconGeometry.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using GLenum = unsigned int;
using GLbitfield = unsigned int;
const GLenum GL_LINES = 1U;
int viewBegins = 0, viewEnds = 0, primitiveBegins = 0, primitiveEnds = 0;
int vertexFailure = -1, drawExceptions = 0;
std::vector<dommy::geometry::FieldIconPoint> vertices;
struct M3dView {
    enum DisplayStyle { kWireFrame };
    enum DisplayStatus { kDormant };
    void beginGL() { ++viewBegins; }
    void endGL() { ++viewEnds; }
};
struct MDagPath {};
void glPushAttrib(GLbitfield) {}
void glPopAttrib() {}
void glBegin(GLenum mode) { assert(mode == GL_LINES); ++primitiveBegins; }
void glEnd() { ++primitiveEnds; }
void glVertex3d(double x, double y, double z) {
    if (static_cast<int>(vertices.size()) == vertexFailure) throw std::runtime_error("vertex");
    vertices.push_back({{x, y, z}});
}
struct DCurlNoiseField {
    void draw(M3dView&, const MDagPath&, M3dView::DisplayStyle, M3dView::DisplayStatus);
};
struct DOpenVDBField {
    void draw(M3dView&, const MDagPath&, M3dView::DisplayStyle, M3dView::DisplayStatus);
};
#define DOMMY_DRAW_CATCH(owner) catch (...) { ++drawExceptions; }
#include "owned_field_draw.inc"

bool close(double first, double second) { return std::fabs(first - second) < 1e-10; }

double value(const dommy::noise::WoodSettings& settings) {
    return dommy::noise::WoodNoise(settings).GetValue(.137, .283, .419);
}

void woodChecks()
{
    using dommy::noise::WoodSettings;
    using dommy::noise::WoodNoise;
    WoodSettings analytic;
    analytic.grainAmount = 0.0;
    analytic.ringFrequency = 2.5;
    analytic.bias = .125;
    const WoodNoise rings(analytic);
    assert(rings.GetSourceModuleCount() == 0);
    for (double radius : {0.0, .05, .1, .2, .3, 1.0})
        assert(close(rings.GetValue(radius, 100.0, 0.0),
                     std::cos(radius * 2.5 * 2.0 * std::acos(-1.0)) + .125));
    analytic.rotationRadians[0] = std::acos(-1.0) / 2.0;
    assert(close(WoodNoise(analytic).GetValue(0, .05, 0), rings.GetValue(.05, 0, 0)));

    WoodSettings settings;
    settings.seed = 13;
    settings.warpOctaves = 4;
    settings.ringFrequency = 2.5;
    settings.grainStretch = .2;
    settings.grainAmount = .3;
    settings.bias = .1;
    settings.warpFrequency = .8;
    settings.warpAmount = .3;
    settings.rotationRadians = {{.15, -.2, .4}};
    const double baseline = value(settings);
    assert(std::isfinite(baseline) && value(settings) == baseline);
    for (int mode = 0; mode < 10; ++mode) {
        WoodSettings changed = settings;
        switch (mode) {
            case 0: ++changed.seed; break;
            case 1: ++changed.warpOctaves; break;
            case 2: changed.ringFrequency += .4; break;
            case 3: changed.grainStretch += .4; break;
            case 4: changed.grainAmount += .4; break;
            case 5: changed.bias += .4; break;
            case 6: changed.warpFrequency += .4; break;
            case 7: changed.warpAmount += .4; break;
            case 8: changed.rotationRadians[0] += .4; break;
            case 9: changed.rotationRadians[2] += .4; break;
        }
        assert(std::isfinite(value(changed)) && !close(value(changed), baseline));
    }
    for (std::uint32_t seed : {0U, 1U, 0xFFFFFFFFU})
        for (int octaves : {1, 32}) {
            settings.seed = seed;
            settings.warpOctaves = octaves;
            assert(std::isfinite(value(settings)));
        }
    const double infinity = std::numeric_limits<double>::infinity();
    for (int mode = 0; mode < 6; ++mode) {
        WoodSettings invalid = settings;
        if (mode == 0) invalid.warpOctaves = 0;
        if (mode == 1) invalid.warpOctaves = 33;
        if (mode == 2) invalid.bias = infinity;
        if (mode == 3) invalid.rotationRadians[1] = infinity;
        if (mode == 4) invalid.warpFrequency = -1.0;
        if (mode == 5) invalid.grainAmount = std::numeric_limits<double>::quiet_NaN();
        bool failed = false;
        try { (void)value(invalid); } catch (const ::noise::ExceptionInvalidParam&) { failed = true; }
        assert(failed);
    }
    for (double coordinate : {infinity, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::max()}) {
        bool failed = false;
        try { (void)rings.GetValue(coordinate, 0, coordinate); }
        catch (const ::noise::ExceptionInvalidParam&) { failed = true; }
        assert(failed);
    }
}

template <typename Field>
void drawChecks()
{
    for (int failure : {-1, 0, 1, 71, 161}) {
        vertices.clear();
        viewBegins = viewEnds = primitiveBegins = primitiveEnds = drawExceptions = 0;
        vertexFailure = failure;
        M3dView view;
        Field field;
        field.draw(view, MDagPath(), M3dView::kWireFrame, M3dView::kDormant);
        assert(viewBegins == 1 && viewEnds == 1 && primitiveBegins == 1 && primitiveEnds == 1);
        assert(drawExceptions == (failure >= 0 ? 1 : 0));
        assert(vertices.size() == (failure >= 0 ? static_cast<std::size_t>(failure) : 162U));
        for (const auto& vertex : vertices)
            for (double component : vertex)
                assert(std::isfinite(component) && std::fabs(component) <= 1.25);
        if (failure < 0)
            for (std::size_t index = 0; index < vertices.size(); index += 2U)
                assert(vertices[index] != vertices[index + 1U]);
    }
}

int main()
{
    woodChecks();
    drawChecks<DCurlNoiseField>();
    drawChecks<DOpenVDBField>();
    std::cout << "Owned Wood: analytic rings, controls/seeds, rotation, finite admission; Field icons: 81 lines and exception cleanup passed\n";
}
