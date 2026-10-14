#include "common/utility/DRandomValueGeneration.h"
#include <maya/MStatus.h>
#include <perlin.h>
#include <array>
#include <cassert>
#include <iostream>
#include <string>

#include "animation_seed_methods.inc"

namespace {
void verifySeed(int seed) {
    std::array<int, 3> actualSeeds = {};
    std::array<double, 3> values = {};
    assert(signalNoiseSeeds(seed, actualSeeds, values) == MS::kSuccess);
    const double amplitude[] = {0.5, 2.0, -3.0};
    for (unsigned axis = 0; axis < 3; ++axis) {
        const auto expected =
            static_cast<int>((static_cast<std::uint64_t>(seed) + axis) %
                             (static_cast<std::uint64_t>(std::numeric_limits<int>::max()) + 1U));
        assert(actualSeeds[axis] == expected);
        noise::module::Perlin reference;
        reference.SetSeed(expected);
        reference.SetOctaveCount(6);
        reference.SetFrequency(2.0);
        reference.SetLacunarity(2.0);
        reference.SetPersistence(0.5);
        reference.SetNoiseQuality(noise::QUALITY_BEST);
        assert(values[axis] == reference.GetValue(13.25 * 0.01, 0.0, 0.0) * amplitude[axis]);
    }
    std::vector<double> actual, expected;
    assert(curveRadiusSeeds(seed, true, 1.25, actual) == MS::kSuccess);
    const auto next = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(seed) + 1U) %
        (static_cast<std::uint64_t>(std::numeric_limits<int>::max()) + 1U));
    dommy::random::generateDoubleValues(next, 0.0, 1.25, 5, expected);
    assert(actual == expected);
}
} // namespace

int main(int argc, char** argv) {
    for (int seed = 0; seed < 100; ++seed)
        verifySeed(seed);
    verifySeed(std::numeric_limits<int>::max() - 2);
    if (argc == 2 && std::string(argv[1]) == "--normal-only")
        return 0;
    verifySeed(std::numeric_limits<int>::max() - 1);
    verifySeed(std::numeric_limits<int>::max());
    for (int seed : {-1, std::numeric_limits<int>::min()}) {
        std::array<int, 3> seeds = {{11, 22, 33}};
        std::array<double, 3> values = {{4, 5, 6}};
        assert(signalNoiseSeeds(seed, seeds, values) == MS::kInvalidParameter);
        assert((seeds == std::array<int, 3>{{11, 22, 33}}));
        assert((values == std::array<double, 3>{{4, 5, 6}}));
        std::vector<double> radii = {9};
        assert(curveRadiusSeeds(seed, true, 1.25, radii) == MS::kInvalidParameter);
        assert(radii == std::vector<double>{9});
        assert(curveRadiusSeeds(seed, false, 1.25, radii) == MS::kSuccess);
        assert(radii == std::vector<double>(5, 0.0));
        assert(curveRadiusSeeds(seed, true, 0.0, radii) == MS::kSuccess);
    }
    std::cout << "Animation seeds: 101 legacy-safe seeds, INT_MAX boundaries, negative inputs "
                 "and inactive radius generation passed with real Perlin/random helpers.\n";
}
