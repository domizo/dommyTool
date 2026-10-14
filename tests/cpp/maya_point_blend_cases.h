#pragma once

// Runs with both the small host-free MPoint stub and the mesh API double.
// Expectations are scalar arithmetic/Cartesian conversion, not fake operators.
#include <cassert>
#include <cmath>
#include <limits>

namespace {
inline bool samePointBits(const MPoint& first, const MPoint& second)
{
    const double a[] = {first.x, first.y, first.z, first.w};
    const double b[] = {second.x, second.y, second.z, second.w};
    for (unsigned axis = 0; axis < 4; ++axis) {
        if (a[axis] != b[axis] || std::signbit(a[axis]) != std::signbit(b[axis])) return false;
    }
    return true;
}

inline unsigned checkMayaPointBlendCases()
{
    using dommy::maya::checkedPointBlend;
    using dommy::maya::PointBlendRounding;
    const auto legacy = PointBlendRounding::LegacyDisplacement;
    const double maximum = std::numeric_limits<double>::max();
    const double tiny = std::numeric_limits<double>::denorm_min();
    unsigned cases = 0;
    const MPoint sentinel(31, 32, 33);
    MPoint output = sentinel;

    // Full-weight cancellation and subnormal endpoints in all three axes.
    for (double large : {1e20, -1e20, maximum, -maximum}) {
        for (double small : {1.0, -1.0, tiny, -tiny}) {
            const MPoint source(large, -large, large), target(small, -small, small);
            for (double weight : {0.0, 1.0}) {
                assert(checkedPointBlend(source, target, weight, output, legacy));
                assert(samePointBits(output, weight == 0 ? source : target));
                ++cases;
            }
        }
    }
    MPoint zeroSource(0.0, -0.0, 0.0), zeroTarget(-0.0, 0.0, -0.0);
    assert(checkedPointBlend(zeroSource, zeroTarget, 1, output, legacy));
    assert(samePointBits(output, zeroTarget));
    assert(checkedPointBlend(zeroSource, zeroTarget, 0, output, legacy));
    assert(samePointBits(output, zeroSource));
    cases += 2;

    bool roundingDifference = false;
    for (double first : {-4.3, 1.23456789012345, 1e-20, 1e20}) {
        for (double second : {2.34567890123456, -1e20, -1e-20, 8.7}) {
            for (double weight : {-1.25, 0.125, 0.3, 0.625, 1.25}) {
                const MPoint source(first, second, -first), target(second, -first, first);
                const MPoint expected(first + (second-first)*weight,
                                      second + (-first-second)*weight,
                                      -first + (first+first)*weight);
                assert(checkedPointBlend(source, target, weight, output, legacy));
                assert(samePointBits(output, expected));
                MPoint checked;
                assert(checkedPointBlend(source, target, weight, checked));
                double x, y, z;
                assert(dommy::attribute::checkedLinearBlend(first, second, weight, x));
                assert(dommy::attribute::checkedLinearBlend(second, -first, weight, y));
                assert(dommy::attribute::checkedLinearBlend(-first, first, weight, z));
                assert(samePointBits(checked, MPoint(x, y, z)));
                roundingDifference |= !samePointBits(output, checked);
                ++cases;
            }
        }
    }
    assert(roundingDifference); // Detect silently changing the mesh rounding policy.

    // Recover a finite midpoint despite an overflowing displacement.
    assert(checkedPointBlend(MPoint(-maximum, maximum, -maximum),
                             MPoint(maximum, -maximum, maximum), 0.5, output, legacy));
    assert(output.x == 0 && output.y == 0 && output.z == 0 && output.w == 1);
    ++cases;

    // Direct division is essential: 1/w overflows for subnormal w.
    for (double w : {2.0, -2.0, tiny, -tiny}) {
        MPoint source(w, -w, w), target(-w, w, -w);
        source.w = target.w = w;
        for (double weight : {0.0, 0.125, 1.0, 1.25}) {
            const MPoint expected(1.0 + (-1.0-1.0)*weight,
                                  -1.0 + (1.0+1.0)*weight,
                                  1.0 + (-1.0-1.0)*weight);
            assert(checkedPointBlend(source, target, weight, output, legacy));
            assert(samePointBits(output, expected));
            MPoint alias = source;
            assert(checkedPointBlend(alias, target, weight, alias, legacy));
            assert(samePointBits(alias, expected));
            alias = target;
            assert(checkedPointBlend(source, alias, weight, alias, legacy));
            assert(samePointBits(alias, expected));
            ++cases;
        }
    }

    for (auto policy : {PointBlendRounding::CheckedInterpolation, legacy}) {
        const auto rejected = [&](const MPoint& source, const MPoint& target, double weight) {
            output = sentinel;
            assert(!checkedPointBlend(source, target, weight, output, policy));
            assert(samePointBits(output, sentinel));
            MPoint alias = source;
            assert(!checkedPointBlend(alias, target, weight, alias, policy));
            // Invalid values themselves may contain NaN; unchanged storage is
            // verified via each finite/non-finite component below instead.
            const double a[] = {alias.x, alias.y, alias.z, alias.w};
            const double b[] = {source.x, source.y, source.z, source.w};
            for (unsigned axis = 0; axis < 4; ++axis) {
                assert((std::isnan(a[axis]) && std::isnan(b[axis])) ||
                       (a[axis] == b[axis] && std::signbit(a[axis]) == std::signbit(b[axis])));
            }
            ++cases;
        };
        for (double invalid : {NAN, INFINITY, -INFINITY}) {
            rejected(MPoint(1, 2, 3), MPoint(4, 5, 6), invalid);
            for (unsigned axis = 0; axis < 3; ++axis) {
                MPoint bad(1, 2, 3);
                double* components[] = {&bad.x, &bad.y, &bad.z};
                *components[axis] = invalid;
                rejected(bad, MPoint(4, 5, 6), 0.5);
                rejected(MPoint(4, 5, 6), bad, 0.5);
            }
        }
        for (double w : {0.0, -0.0, double(NAN), double(INFINITY), -double(INFINITY)}) {
            MPoint bad(1, 2, 3); bad.w = w;
            rejected(bad, MPoint(4, 5, 6), 0.5);
            rejected(MPoint(4, 5, 6), bad, 0.5);
        }
        MPoint bad(0, 0, 1); bad.w = tiny;
        rejected(bad, MPoint(4, 5, 6), 0.5); // Failure on the last Cartesian axis.
        rejected(MPoint(4, 5, 6), bad, 0.5);
        rejected(MPoint(1, 2, -maximum), MPoint(3, 4, maximum), 2.0);
    }
    return cases;
}
} // namespace
