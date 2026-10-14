#include <maya/MStatus.h>
#include "common/utility/DUvParameter.h"
#include "math/DRandomGenerator.h"
#include <cassert>
#include <iostream>

struct MGlobal { static void displayError(const char*) {} };
#include "surface_uv_wrapping.inc"

int main()
{
    double u = 99.0, v = 99.0;
    assert(sampleUV(0, 1e-300, 0, 1e-300, 1, 1, 0, 0,
                    .5, -.5, 0, 0, 0, 0, u, v) == MS::kSuccess);
    assert(std::isfinite(u) && u >= 0 && u <= 1e-300);
    assert(std::isfinite(v) && v >= 0 && v <= 1e-300);
    assert(sampleUV(.5, 3.5, .5, 3.5, 6, 6, 0, 0,
                    3, -3, 0, 0, 0, 0, u, v) == MS::kSuccess);
    assert(u == .75 && v == .75); // Origin-relative wrap on both sides.

    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 9; ++j) {
            assert(sampleUV(-2, 1, 4, 7, 8, 9, i, j,
                            100, -100, .125, -.25, .25, .25, u, v) == MS::kSuccess);
            assert(u >= -2 && u <= 1 && v >= 4 && v <= 7);
        }
    }
    double value = 99;
    assert(dommy::uv::wrapOffsetParameter(.5, .5, 0, 0, 0, 1, value) && value == 1);
    assert(dommy::uv::wrapOffsetParameter(0, -1, 0, 0, 0, 1, value) && value == 1);
    assert(dommy::uv::wrapOffsetParameter(0, 2, 0, 0, 0, 1, value) && value == 0);
    assert(dommy::uv::wrapOffsetParameter(.25, std::ldexp(1.0, 60), 0, 0, 0, 3, value));
    assert(value == 1.25); // Small phase must survive a very large offset.
    assert(dommy::uv::wrapOffsetParameter(.25, 1e308, -1e308, 0, 0, 3, value));
    assert(value == .25); // Rounded cancellation must not hide an in-range phase.
    assert(dommy::uv::wrapOffsetParameter(.25, 1e308, 1e308, 0, 0, 3, value));
    assert(std::isfinite(value) && value >= 0 && value <= 3);
    const double tiny = std::numeric_limits<double>::denorm_min();
    assert(dommy::uv::wrapOffsetParameter(0, .5, 0, 0, 0, tiny, value));
    assert(value >= 0 && value <= tiny);
    value = 99;
    assert(!dommy::uv::wrapOffsetParameter(0, 1, 0, 0, 2, 2, value) && value == 99);
    assert(!dommy::uv::wrapOffsetParameter(0, std::numeric_limits<double>::infinity(),
                                         0, 0, 0, 1, value) && value == 99);
    assert(!dommy::uv::wrapOffsetParameter(0, 0, 0, 0,
        std::numeric_limits<double>::quiet_NaN(), 1, value) && value == 99);
    assert(admitRanges(0, 1e-300, 0, 1e-300) == MS::kSuccess);
    assert(admitRanges(-1e308, 1e308, 0, 1) == MS::kInvalidParameter);
    assert(admitRanges(0, 1, -1e308, 1e308) == MS::kInvalidParameter);
    std::cout << "Production surface UV admission/math: tiny ranges, origins, endpoints, phase and overflow passed; no native geometry\n";
}
