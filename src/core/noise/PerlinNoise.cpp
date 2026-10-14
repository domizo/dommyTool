#include "PerlinNoise.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace noise {
namespace {

bool splitCoordinate(double coordinate, int& lattice, double& fraction)
{
    if (!std::isfinite(coordinate)) return false;

    const double floored = std::floor(coordinate);
    double wrapped = std::fmod(floored, 256.0);
    if (!std::isfinite(wrapped)) return false;
    if (wrapped < 0.0) wrapped += 256.0;
    if (wrapped < 0.0 || wrapped >= 256.0) return false;

    lattice = static_cast<int>(wrapped);
    fraction = coordinate - floored;
    return std::isfinite(fraction) && fraction >= 0.0 && fraction < 1.0;
}

double fade(double value)
{
    return value * value * value *
        (value * (value * 6.0 - 15.0) + 10.0);
}

double interpolate(double weight, double first, double second)
{
    return first + weight * (second - first);
}

double gradient(int hash, double x, double y, double z)
{
    const int lowerHash = hash & 15;
    const double first = lowerHash < 8 ? x : y;
    const double second = lowerHash < 4
        ? y
        : (lowerHash == 12 || lowerHash == 14 ? x : z);
    return ((lowerHash & 1) == 0 ? first : -first) +
        ((lowerHash & 2) == 0 ? second : -second);
}

} // namespace

Perlin::Perlin(std::uint32_t seed)
{
    std::array<int, 512>::iterator midpoint = permutation_.begin() + 256;
    std::mt19937 engine(seed);

    std::iota(permutation_.begin(), midpoint, 0);
    std::shuffle(permutation_.begin(), midpoint, engine);
    std::copy(permutation_.begin(), midpoint, midpoint);
}

double Perlin::noise(double x, double y, double z) const
{
    int latticeX = 0;
    int latticeY = 0;
    int latticeZ = 0;
    double fractionalX = 0.0;
    double fractionalY = 0.0;
    double fractionalZ = 0.0;
    if (!splitCoordinate(x, latticeX, fractionalX) ||
        !splitCoordinate(y, latticeY, fractionalY) ||
        !splitCoordinate(z, latticeZ, fractionalZ)) {
        return 0.0;
    }

    const double u = fade(fractionalX);
    const double v = fade(fractionalY);
    const double w = fade(fractionalZ);

    const int a = permutation_[latticeX] + latticeY;
    const int aa = permutation_[a] + latticeZ;
    const int ab = permutation_[a + 1] + latticeZ;
    const int b = permutation_[latticeX + 1] + latticeY;
    const int ba = permutation_[b] + latticeZ;
    const int bb = permutation_[b + 1] + latticeZ;

    const double lower = interpolate(
        v,
        interpolate(
            u,
            gradient(permutation_[aa], fractionalX, fractionalY, fractionalZ),
            gradient(
                permutation_[ba], fractionalX - 1.0, fractionalY,
                fractionalZ)),
        interpolate(
            u,
            gradient(
                permutation_[ab], fractionalX, fractionalY - 1.0,
                fractionalZ),
            gradient(
                permutation_[bb], fractionalX - 1.0, fractionalY - 1.0,
                fractionalZ)));

    const double upper = interpolate(
        v,
        interpolate(
            u,
            gradient(
                permutation_[aa + 1], fractionalX, fractionalY,
                fractionalZ - 1.0),
            gradient(
                permutation_[ba + 1], fractionalX - 1.0, fractionalY,
                fractionalZ - 1.0)),
        interpolate(
            u,
            gradient(
                permutation_[ab + 1], fractionalX, fractionalY - 1.0,
                fractionalZ - 1.0),
            gradient(
                permutation_[bb + 1], fractionalX - 1.0,
                fractionalY - 1.0, fractionalZ - 1.0)));

    const double result = interpolate(w, lower, upper);
    return std::isfinite(result) ? result : 0.0;
}

PerlinOctave::PerlinOctave(int octaves,
                           std::uint32_t seed,
                           double alpha,
                           double beta)
    : perlin_(seed),
      octaves_(octaves),
      alpha_(alpha),
      beta_(beta),
      valid_(octaves >= 0 && octaves <= kMaxPerlinOctaves &&
             std::isfinite(alpha) && std::isfinite(beta))
{
}

double PerlinOctave::noise(double x, double y, double z) const
{
    double result = 0.0;
    tryNoise(x, y, z, result);
    return result;
}

bool PerlinOctave::tryNoise(double x, double y, double z, double& output) const
{
    if (!valid_ || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(z)) {
        return false;
    }

    double result = 0.0;
    double amplitude = 1.0;
    for (int octave = 0; octave < octaves_; ++octave) {
        const double octaveValue = perlin_.noise(x, y, z) * amplitude;
        const double nextResult = result + octaveValue;
        if (!std::isfinite(octaveValue) || !std::isfinite(nextResult)) {
            return false;
        }
        result = nextResult;

        if (octave + 1 < octaves_) {
            x *= beta_;
            y *= beta_;
            z *= beta_;
            amplitude *= alpha_;
            if (!std::isfinite(x) || !std::isfinite(y) ||
                !std::isfinite(z) || !std::isfinite(amplitude)) {
                return false;
            }
        }
    }
    output = result;
    return true;
}

} // namespace noise
