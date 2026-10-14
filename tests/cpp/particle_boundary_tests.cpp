// Attribute doubles cover control flow and explicit declarations, not Maya internals.
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "noise/PerlinNoise.h"
#include <maya/MGlobal.h>
#include <maya/MVector.h>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "maya_attribute_doubles.h"

struct FieldBase : AttributeNode {
    static const MObject mOutputForce;
};
const MObject FieldBase::mOutputForce = {"mOutputForce"};

MVector& operator+=(MVector& lhs, const MVector& rhs) {
    lhs = lhs + rhs;
    return lhs;
}

#include "particle_boundary_methods.inc"

namespace {

bool near(double a, double b) {
    return std::fabs(a - b) <= 1.0e-12;
}

void testRadialNoise() {
    const MVector input(0.13, 0.27, 0.39), scale(1.0, 1.0, 1.0), offset;
    const noise::PerlinOctave perlin(2, 1U);
    const MVector sample(perlin.noise(input.x, input.y, input.z),
                         perlin.noise(input.y, input.z, input.x),
                         perlin.noise(input.z, input.x, input.y));
    assert(std::fabs(sample.x) > 1.0e-8);
    const double values[] = {-1.0, -0.0, 0.0, 1.0e-12, 1.0};
    for (double x : values) {
        for (double y : values) {
            for (double z : values) {
                MVector target = input;
                assert(evaluateRadialNoise(target, MVector(x, y, z), scale, offset, 2, 1U) ==
                       MS::kSuccess);
                assert(near(target.x, input.x + sample.x * x));
                assert(near(target.y, input.y + sample.y * y));
                assert(near(target.z, input.z + sample.z * z));
            }
        }
    }
    // All-negative amplitudes must follow the same budget and coordinate checks.
    const MVector negative(-1.0, -1.0, -1.0);
    const std::size_t workLimit = dommy::generation::kMaxWorkItemCount;
    const std::size_t overBudgetCount = workLimit / (16U * 3U) + 1U;
    assert(overBudgetCount <= dommy::generation::kMaxGeneratedPointCount);
    MVector target = input;
    assert(evaluateRadialNoise(target, negative, scale, offset, 16, overBudgetCount) ==
           MS::kFailure);
    assert(target.x == input.x && target.y == input.y && target.z == input.z);
    const double maximum = std::numeric_limits<double>::max();
    assert(evaluateRadialNoise(target, negative, scale, MVector(maximum, 0, 0), 2, 1U) ==
           MS::kFailure);
    assert(target.x == input.x && target.y == input.y && target.z == input.z);
    // Zero amplitude still skips inactive noise work and coordinate validation.
    assert(evaluateRadialNoise(target, MVector(), scale, MVector(maximum, 0, 0), 16,
                               overBudgetCount) == MS::kSuccess);
    assert(target.x == input.x && target.y == input.y && target.z == input.z);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream file(argv[1]);
    assert(file.good());
    std::ostringstream expected;
    expected << file.rdbuf();
    const std::string actual = schemaTrace("DCurlNoiseField", DCurlNoiseField::initialize) +
                               schemaTrace("DCurveField", DCurveField::initialize);
    if (actual != expected.str()) {
        std::cerr << "Field initializer declaration trace changed:\n" << actual;
        return 1;
    }
    const int failures = testFailures("DCurlNoiseField", DCurlNoiseField::initialize, 91) +
                         testFailures("DCurveField", DCurveField::initialize, 64);
    testRadialNoise();
    std::cout << "Particle boundaries: " << failures
              << " injected initializer failures, frozen attributes/dependencies, 125 signed noise cases: OK\n";
}
