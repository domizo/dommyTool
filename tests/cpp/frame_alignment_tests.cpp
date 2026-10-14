#include "math/DFrameAlignment.h"
#include <maya/MStatus.h>
#include <cassert>
#include <iostream>
#include <limits>
#include <random>

// Only storage adapters: all rotation calculations below are production code.
struct MVector {
    double x, y, z;
};
struct MMatrix {
    double value[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    double* operator[](unsigned row) {
        return value[row];
    }
    const double* operator[](unsigned row) const {
        return value[row];
    }
};
#include "frame_adapter.inc"

namespace {
using V = dommy::math::FrameDirection;
using R = dommy::math::FrameRotation;

double dot(const V& a, const V& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V multiply(const V& v, const R& m) {
    V result = {};
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned column = 0; column < 3; ++column)
            result[column] += v[row] * m[row][column];
    return result;
}
void near(const V& actual, const V& expected) {
    for (unsigned i = 0; i < 3; ++i)
        assert(std::abs(actual[i] - expected[i]) < 2e-12);
}
void properRotation(const R& r) {
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            assert(std::abs(dot(r[i], r[j]) - (i == j ? 1 : 0)) < 2e-12);
    const double determinant = r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1]) -
                               r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0]) +
                               r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]);
    assert(std::abs(determinant - 1) < 2e-12);
}
R verify(const V& sf, const V& su, const V& tf, const V& tu, const V& expectedSf,
         const V& expectedSu, const V& expectedTf, const V& expectedTu) {
    R r;
    assert(dommy::math::makeFrameAlignment(sf, su, tf, tu, r));
    properRotation(r);
    near(multiply(expectedSf, r), expectedTf);
    near(multiply(expectedSu, r), expectedTu);
    MMatrix adapted;
    assert(getAlignSpaceMatrix({tf[0], tf[1], tf[2]}, {tu[0], tu[1], tu[2]}, {sf[0], sf[1], sf[2]},
                               {su[0], su[1], su[2]}, adapted) == MS::kSuccess);
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            assert(adapted[i][j] == (i < 3 && j < 3 ? r[i][j] : (i == j ? 1 : 0)));
    return r;
}
V unit(const V& v) {
    // Independent reference for bounded random vectors (no overflow expected).
    const double length = std::sqrt(dot(v, v));
    return {{v[0] / length, v[1] / length, v[2] / length}};
}
V projectedUp(const V& f, const V& u) {
    const double projection = dot(f, u);
    return unit({{u[0] - projection * f[0], u[1] - projection * f[1], u[2] - projection * f[2]}});
}
} // namespace

int main() {
    const V axes[] = {{{1, 0, 0}},  {{-1, 0, 0}}, {{0, 1, 0}},
                      {{0, -1, 0}}, {{0, 0, 1}},  {{0, 0, -1}}};
    unsigned cardinalCases = 0;
    for (const auto& sf : axes)
        for (const auto& su : axes) {
            if (dot(sf, su) != 0)
                continue;
            for (const auto& tf : axes)
                for (const auto& tu : axes) {
                    if (dot(tf, tu) != 0)
                        continue;
                    verify(sf, su, tf, tu, sf, su, tf, tu);
                    ++cardinalCases;
                }
        }
    assert(cardinalCases == 576);
    // Independent composition oracle: animate +Y around Z by +90deg, then
    // align source F=+Y/U=+X to target F=+Z/U=+Y. Result must be -Y.
    const R alignment =
        verify(axes[2], axes[0], axes[4], axes[2], axes[2], axes[0], axes[4], axes[2]);
    const R rz = {{{{0, 1, 0}}, {{-1, 0, 0}}, {{0, 0, 1}}}};
    near(multiply(multiply(axes[2], rz), alignment), axes[3]);

    std::mt19937 rng(6291);
    std::uniform_real_distribution<double> sample(-1, 1);
    for (unsigned i = 0; i < 1000; ++i) {
        const V sf = unit({{sample(rng), sample(rng), sample(rng)}});
        const V tf = unit({{sample(rng), sample(rng), sample(rng)}});
        const V su = {{sample(rng), sample(rng), sample(rng)}};
        const V tu = {{sample(rng), sample(rng), sample(rng)}};
        verify(sf, su, tf, tu, sf, projectedUp(sf, su), tf, projectedUp(tf, tu));
    }
    // Input scale must not affect the result, including denormals/max double.
    for (double scale : {std::numeric_limits<double>::denorm_min(), 1e-200, 1.0, 1e200,
                         std::numeric_limits<double>::max()}) {
        verify({{scale, 0, 0}}, {{scale, scale, 0}}, {{0, scale, 0}}, {{0, scale, scale}}, axes[0],
               axes[2], axes[2], axes[4]);
    }
    const V invalid[] = {{{0, 0, 0}},
                         {{std::numeric_limits<double>::infinity(), 1, 0}},
                         {{0, std::numeric_limits<double>::quiet_NaN(), 1}}};
    for (const auto& bad : invalid)
        for (unsigned index = 0; index < 4; ++index) {
            V v[] = {axes[0], axes[2], axes[4], axes[0]};
            v[index] = bad;
            R output = rz;
            assert(!dommy::math::makeFrameAlignment(v[0], v[1], v[2], v[3], output));
            assert(output == rz);
            MMatrix adapter;
            adapter[3][0] = 42;
            assert(getAlignSpaceMatrix({v[2][0], v[2][1], v[2][2]}, {v[3][0], v[3][1], v[3][2]},
                                       {v[0][0], v[0][1], v[0][2]}, {v[1][0], v[1][1], v[1][2]},
                                       adapter) == MS::kInvalidParameter);
            assert(adapter[3][0] == 42 && adapter[0][0] == 1);
        }
    for (double sign : {-1., 1.}) {
        R output = rz;
        const V parallel = {{sign, 0, 0}};
        const V almostParallel = {{sign, 1e-14, 0}};
        for (const V& up : {parallel, almostParallel}) {
            assert(!dommy::math::makeFrameAlignment(axes[0], up, axes[4], axes[0], output));
            assert(!dommy::math::makeFrameAlignment(axes[4], axes[0], axes[0], up, output));
            assert(output == rz);
        }
    }
    std::cout << "Frame alignment: 576 cardinal frame pairs, 1000 random pairs, composition, "
                 "extreme scales, invalid inputs and adapter transactions passed.\n";
}
