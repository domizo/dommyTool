#pragma once

#include "DNumericValidation.h"
#include "DGenerationLimits.h"
#include "DProductSum.h"
#include "math/DBounds.h"
#include "math/DVectorNormalization.h"

#include <array>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace dommy {
namespace numeric {

template <typename Matrix>
bool isFiniteAffineTransform(const Matrix& matrix)
{
    for (std::size_t row = 0U; row < 4U; ++row)
        for (std::size_t column = 0U; column < 4U; ++column)
            if (!std::isfinite(matrix[row][column])) return false;
    return matrix[0][3] == 0.0 && matrix[1][3] == 0.0 &&
           matrix[2][3] == 0.0 && matrix[3][3] == 1.0;
}

// Compose a world-space affine operation around a world-space pivot using
// Maya's row-vector convention. The primitive matrix need not be invertible.
// Keep local geometry unchanged and publish only a complete finite matrix.
template <typename Matrix, typename Point>
bool composeWorldPivotTransform(const Matrix& primitive, const Point& center,
                                const Matrix& operation, Matrix& output)
{
    if (!isFiniteAffineTransform(primitive) || !isFiniteAffineTransform(operation)) return false;
    for (std::size_t axis = 0U; axis < 3U; ++axis)
        if (!std::isfinite(center[axis])) return false;
    Matrix candidate;
    for (std::size_t row = 0U; row < 4U; ++row) {
        for (std::size_t axis = 0U; axis < 3U; ++axis) {
            double value = 0.0;
            if (row == 3U) {
                // Expand (translation - pivot) * operation so even the
                // subtraction cannot overflow before a finite final result.
                const std::array<double, 8> left{{primitive[row][0], primitive[row][1], primitive[row][2],
                    -double(center[0]), -double(center[1]), -double(center[2]), operation[3][axis], double(center[axis])}};
                const std::array<double, 8> right{{operation[0][axis], operation[1][axis], operation[2][axis],
                    operation[0][axis], operation[1][axis], operation[2][axis], 1.0, 1.0}};
                if (!checkedProductSum(left, right, value)) return false;
            } else {
                const std::array<double, 3> left{{primitive[row][0], primitive[row][1], primitive[row][2]}};
                const std::array<double, 3> right{{operation[0][axis], operation[1][axis], operation[2][axis]}};
                if (!checkedProductSum(left, right, value)) return false;
            }
            if (!checkedNumericCast(value, candidate[row][axis])) return false;
        }
        candidate[row][3] = row == 3U ? 1.0 : 0.0;
    }
    output = candidate;
    return true;
}

// Maya row-vector convention, with the input point's homogeneous w = 1.
// The caller validates the complete matrix once before processing its points.
// Float-backed DMesh storage and double-backed Maya output use the same math.
template <typename Point, typename Matrix, typename Scalar>
bool checkedTransformPoint(const Point& point, const Matrix& matrix,
                           std::array<Scalar, 3>& output)
{
    std::array<Scalar, 3> candidate;
    const std::array<double, 4> left{{double(point[0]), double(point[1]), double(point[2]), 1.0}};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        const std::array<double, 4> right{{matrix[0][axis], matrix[1][axis], matrix[2][axis], matrix[3][axis]}};
        double value = 0.0;
        if (!checkedProductSum(left, right, value) || !checkedNumericCast(value, candidate[axis])) return false;
    }
    output = candidate;
    return true;
}

// Transform a direction with homogeneous w = 0: translation must not alter
// endpoint tangents. The caller validates the matrix once for its geometry.
template <typename Vector, typename Matrix, typename Scalar>
bool checkedTransformDirection(const Vector& direction, const Matrix& matrix,
                               std::array<Scalar, 3>& output)
{
    std::array<Scalar, 3> candidate;
    const std::array<double, 3> left{{double(direction[0]), double(direction[1]), double(direction[2])}};
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        const std::array<double, 3> right{{matrix[0][axis], matrix[1][axis], matrix[2][axis]}};
        double value = 0.0;
        if (!checkedProductSum(left, right, value) || !checkedNumericCast(value, candidate[axis])) return false;
    }
    output = candidate;
    return true;
}

// Matrices are validated by the owning Maya data boundary. Failed conversion
// must not expose a shortened or partially transformed array to exporters.
template <typename Destination, typename Points, typename Matrix>
bool checkedTransformPoints(const Points& points, const Matrix& matrix,
                             std::vector<Destination>& output) {
    if (points.size() > generation::kMaxGeneratedPointCount) return false;
    std::vector<Destination> candidate(points.size());
    using Scalar = typename std::decay<decltype(candidate[0][0])>::type;
    for (std::size_t index = 0U; index < points.size(); ++index) {
        std::array<Scalar, 3> transformed;
        if (!checkedTransformPoint(points[index], matrix, transformed)) return false;
        candidate[index] = Destination(transformed[0], transformed[1], transformed[2]);
    }
    output.swap(candidate);
    return true;
}

namespace detail {

// Called only after complete finite-affine/input admission. Six triple products
// cover each determinant/cofactor dot product. 200 words hold all 6294 input
// bits and carry, without narrowing huge or tiny intermediate values to double.
template <typename Vector, typename Matrix>
bool exactNormalDirection(const Vector& normal, const Matrix& matrix,
                          std::array<double, 3>& output)
{
    using Accumulator = BinaryAccumulator<200U, -3222>;
    int determinantSign = 0;
    {
        Accumulator determinant;
        for (std::size_t column = 0U; column < 3U; ++column) {
            const std::size_t next = (column + 1U) % 3U, last = (column + 2U) % 3U;
            if (!determinant.addProduct3(matrix[0][column], matrix[1][next], matrix[2][last]) ||
                !determinant.addProduct3(-double(matrix[0][column]), matrix[1][last], matrix[2][next]))
                return false;
        }
        determinantSign = determinant.sign();
    }
    if (determinantSign == 0) return false;
    std::array<double, 3> fractions;
    std::array<int, 3> exponents;
    int commonExponent = std::numeric_limits<int>::min();
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        Accumulator component;
        for (std::size_t row = 0U; row < 3U; ++row) {
            const std::size_t a = (row + 1U) % 3U, b = (row + 2U) % 3U;
            const std::size_t c = (axis + 1U) % 3U, d = (axis + 2U) % 3U;
            if (!component.addProduct3(normal[row], matrix[a][c], matrix[b][d]) ||
                !component.addProduct3(-double(normal[row]), matrix[a][d], matrix[b][c])) return false;
        }
        component.scaledValue(fractions[axis], exponents[axis]);
        if (fractions[axis] != 0.0) commonExponent = std::max(commonExponent, exponents[axis]);
    }
    if (commonExponent == std::numeric_limits<int>::min()) return false;
    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        if (fractions[axis] == 0.0) continue;
        const int shift = exponents[axis] - commonExponent;
        // Such a relative component rounds to zero even before unit-length
        // normalization. Avoid an unnecessary underflow conversion here.
        fractions[axis] = shift < -1074 ? std::copysign(0.0, fractions[axis])
                                        : std::ldexp(fractions[axis], shift);
    }
    std::array<double, 3> candidate;
    if (!math::normalized3OrZero(fractions[0], fractions[1], fractions[2],
                                candidate[0], candidate[1], candidate[2])) return false;
    for (double& value : candidate) value *= determinantSign;
    output = candidate;
    return true;
}

} // namespace detail

// Row-vector inverse-transpose normal direction, without forming an inverse.
// Ordinary exponent spans use scaled cofactors; wide spans/cancellation reuse
// exact binary triple products. A zero normal is the automatic-normal marker.
template <typename Vector, typename Matrix, typename Scalar>
bool checkedTransformNormal(const Vector& normal, const Matrix& matrix,
                            std::array<Scalar, 3>& output) {
    if (!isFiniteAffineTransform(matrix)) return false;
    for (int i = 0; i < 3; ++i) if (!std::isfinite(normal[i])) return false;
    if (normal[0] == 0 && normal[1] == 0 && normal[2] == 0) {
        output = {{0, 0, 0}};
        return true;
    }
    double matrixScale = 0.0, normalScale = 0.0;
    int matrixLow = 1024, matrixHigh = -1074, normalLow = 1024, normalHigh = -1074;
    for (int i = 0; i < 3; ++i) {
        if (normal[i] != 0) {
            int exponent = 0; std::frexp(double(normal[i]), &exponent);
            normalLow = std::min(normalLow, exponent); normalHigh = std::max(normalHigh, exponent);
            normalScale = std::max(normalScale, std::fabs(double(normal[i])));
        }
        for (int j = 0; j < 3; ++j) if (matrix[i][j] != 0) {
            int exponent = 0; std::frexp(double(matrix[i][j]), &exponent);
            matrixLow = std::min(matrixLow, exponent); matrixHigh = std::max(matrixHigh, exponent);
            matrixScale = std::max(matrixScale, std::fabs(double(matrix[i][j])));
        }
    }
    if (matrixScale == 0.0) return false;
    bool exact = matrixHigh - matrixLow > 200 || normalHigh - normalLow > 200;
    std::array<double, 3> direction;
    if (!exact) {
        double a[3][3], cofactor[3][3], cofactorWork[3][3], source[3];
        // Bound error against the terms BEFORE cancellation. The factor 32
        // covers scaling divisions, products, subtraction and three-term sums
        // (well above their binary64 roundoff bound in this normal band).
        const double errorFactor = 32.0 * std::numeric_limits<double>::epsilon();
        for (int i = 0; i < 3; ++i) {
            source[i] = double(normal[i]) / normalScale;
            for (int j = 0; j < 3; ++j) a[i][j] = matrix[i][j] / matrixScale;
        }
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
            const double first = a[(i+1)%3][(j+1)%3] * a[(i+2)%3][(j+2)%3];
            const double second = a[(i+1)%3][(j+2)%3] * a[(i+2)%3][(j+1)%3];
            cofactor[i][j] = first - second;
            cofactorWork[i][j] = std::fabs(first) + std::fabs(second);
        }
        double determinant = 0.0, determinantWork = 0.0;
        for (int i = 0; i < 3; ++i) {
            const double term = a[0][i] * cofactor[0][i];
            determinant += term;
            determinantWork += std::fabs(a[0][i]) * cofactorWork[0][i];
        }
        exact = std::fabs(determinant) <= determinantWork * errorFactor;
        std::array<double, 3> value{{0.0, 0.0, 0.0}};
        double directionScale = 0.0, largestError = 0.0;
        for (int j = 0; j < 3; ++j) {
            double work = 0.0;
            for (int i = 0; i < 3; ++i) {
                const double term = source[i] * cofactor[i][j];
                value[j] += term;
                work += std::fabs(source[i]) * cofactorWork[i][j];
            }
            directionScale = std::max(directionScale, std::fabs(value[j]));
            largestError = std::max(largestError, work * errorFactor);
        }
        // Component error <= 1e-13*max|v| bounds the unit-vector perturbation
        // below 4e-13. A determinant sign alone cannot certify the direction
        // when cofactors nearly cancel, even if no component is near zero.
        exact = exact || directionScale == 0.0 || largestError > directionScale * 1.0e-13;
        if (!exact) {
            if (!math::normalized3OrZero(value[0], value[1], value[2],
                                        direction[0], direction[1], direction[2])) return false;
            if (determinant < 0.0) for (double& component : direction) component = -component;
        }
    }
    if (exact && !detail::exactNormalDirection(normal, matrix, direction)) return false;
    std::array<Scalar, 3> candidate;
    for (int j = 0; j < 3; ++j)
        if (!checkedNumericCast(direction[j], candidate[j])) return false;
    output = candidate;
    return true;
}

// Share the double transformed bounds path between primitive data and color
// nodes. Append transactionally so a bad later primitive cannot truncate bounds.
template <typename Points, typename Matrix>
bool appendTransformedBounds(const Points& points, const Matrix& matrix, math::Bounds3d& output) {
    std::size_t count = 0U;
    if (!generation::checkedSum(output.count(), points.size(),
                                 generation::kMaxGeneratedPointCount, count)) return false;
    math::Bounds3d candidate(output);
    for (const auto& point : points) {
        std::array<double, 3> transformed;
        if (!checkedTransformPoint(point, matrix, transformed) ||
            !candidate.add(transformed[0], transformed[1], transformed[2])) return false;
    }
    output = candidate;
    return true;
}

} // namespace numeric
} // namespace dommy
