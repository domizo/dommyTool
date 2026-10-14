#pragma once

#include <array>
#include <cstdint>

namespace noise {

constexpr int kMaxPerlinOctaves = 32;

class Perlin {
public:
    // Seed zero is deterministic. Callers that need variation must supply an
    // explicit seed so Maya dependency-graph reevaluation is reproducible.
    explicit Perlin(std::uint32_t seed = 0U);

    double noise(double x) const { return noise(x, 0.0, 0.0); }
    double noise(double x, double y) const { return noise(x, y, 0.0); }
    double noise(double x, double y, double z) const;

private:
    std::array<int, 512> permutation_;
};

class PerlinOctave {
public:
    PerlinOctave(int octaves,
                 std::uint32_t seed = 0U,
                 double alpha = 0.5,
                 double beta = 2.0);

    double noise(double x) const { return noise(x, 0.0, 0.0); }
    double noise(double x, double y) const { return noise(x, y, 0.0); }
    double noise(double x, double y, double z) const;

    // Checked callers can distinguish a legitimate zero sample from invalid
    // settings or arithmetic overflow. Failure leaves result unchanged.
    bool tryNoise(double x, double y, double z, double& result) const;

private:
    Perlin perlin_;
    int octaves_;
    double alpha_;
    double beta_;
    bool valid_;
};

} // namespace noise
