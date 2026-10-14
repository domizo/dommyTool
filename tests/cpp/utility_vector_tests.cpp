// Include the production header first to verify its own dependencies.
#include "Utility/3d/Math/DVec.h"
#include "Utility/DUtility.h"
#include "common/utility/DCheckedSize.h"
#include "numeric_locale_fixture.h"
#ifdef DOMMY_UTILITY_ALLOCATION_TESTS
#include "allocation_failure_fixture.h"
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <type_traits>
#include <vector>

namespace {

template <typename Index>
void checkIntegerIndexRange()
{
    const std::size_t maximumCount = std::numeric_limits<std::size_t>::max();
    const Index maximumIndex = std::numeric_limits<Index>::max();
    std::vector<Index> indices{Index(0), Index(1), Index(2),
        Index(maximumIndex - 1), maximumIndex};
    if (std::is_signed<Index>::value) {
        indices.push_back(Index(-1));
        indices.push_back(Index(-2));
        indices.push_back(std::numeric_limits<Index>::min());
        indices.push_back(Index(std::numeric_limits<Index>::min() + 1));
    }
    for (std::size_t count : {std::size_t(0), std::size_t(1), std::size_t(2),
            std::size_t(64), std::size_t(10000000), maximumCount / 2U,
            maximumCount - 1U, maximumCount}) {
        for (Index index : indices) {
            const bool nonnegative = !std::is_signed<Index>::value ||
                static_cast<std::intmax_t>(index) >= 0;
            const bool expected = nonnegative &&
                static_cast<std::uintmax_t>(index) < static_cast<std::uintmax_t>(count);
            assert(dommy::memory::indexInRange(index, count) == expected);
        }
    }
}

void checkIndexRange()
{
    checkIntegerIndexRange<char>();
    checkIntegerIndexRange<signed char>();
    checkIntegerIndexRange<unsigned char>();
    checkIntegerIndexRange<short>();
    checkIntegerIndexRange<unsigned short>();
    checkIntegerIndexRange<int>();
    checkIntegerIndexRange<unsigned int>();
    checkIntegerIndexRange<long>();
    checkIntegerIndexRange<unsigned long>();
    checkIntegerIndexRange<long long>();
    checkIntegerIndexRange<unsigned long long>();
    checkIntegerIndexRange<wchar_t>();
    checkIntegerIndexRange<char16_t>();
    checkIntegerIndexRange<char32_t>();
    checkIntegerIndexRange<std::size_t>();
    checkIntegerIndexRange<std::ptrdiff_t>();
    checkIntegerIndexRange<std::intmax_t>();
    checkIntegerIndexRange<std::uintmax_t>();
    // Exhaust every byte index and count around the complete byte domain.
    for (unsigned count = 0U; count <= 256U; ++count) {
        for (int index = std::numeric_limits<signed char>::min();
             index <= std::numeric_limits<signed char>::max(); ++index)
            assert(dommy::memory::indexInRange(static_cast<signed char>(index), count) ==
                (index >= 0 && static_cast<unsigned>(index) < count));
        for (unsigned index = 0U; index <= std::numeric_limits<unsigned char>::max(); ++index)
            assert(dommy::memory::indexInRange(static_cast<unsigned char>(index), count) ==
                (index < count));
    }
}

template <typename Exception, typename Operation>
void checkScalarException(Operation operation)
{
    bool rejected = false;
    try { operation(); }
    catch (const Exception&) { rejected = true; }
    catch (...) {} // A different exception is not the promised failure contract.
    assert(rejected);
}

template <typename T>
void checkSignedScalarBoundaries()
{
    const T minimum = std::numeric_limits<T>::min();
    const T maximum = std::numeric_limits<T>::max();
    checkScalarException<std::overflow_error>([&] { (void)D_ABS(minimum); });
    checkScalarException<std::overflow_error>([&] { (void)D_SIGN2(minimum, T(1)); });
    assert(D_SIGN2(minimum, T(-1)) == minimum);
    assert(D_ABS(T(-1)) == T(1) && D_ABS(maximum) == maximum);
    assert(!D_FEQ(minimum, maximum) && !D_FEQ(maximum, minimum));
    assert(D_FEQ(minimum, minimum) && D_FEQ(maximum, maximum));
    const std::array<T, 3> first{{minimum, 0, maximum}}, last{{maximum, 0, minimum}};
    assert(!D_VEC3_FEQ(first, last) && D_VEC3_FEQ(first, first));
}

void checkScalarTemplates()
{
    checkSignedScalarBoundaries<signed char>();
    checkSignedScalarBoundaries<short>();
    checkSignedScalarBoundaries<int>();
    checkSignedScalarBoundaries<long>();
    checkSignedScalarBoundaries<long long>();
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    assert(D_ABS(maximum) == maximum && D_SIGN2(maximum, std::uint64_t(0)) == maximum);
    assert(!D_FEQ(maximum, maximum - 1U));
    assert(D_IS_ZERO(std::uint64_t(0)) && !D_IS_ZERO(maximum));
    assert(D_IS_ZERO(false) && !D_IS_ZERO(true) && D_FEQ(true, true) && !D_FEQ(true, false));

    const std::array<std::uint64_t, 3> origin{{0, 0, 0}};
    const std::array<std::uint64_t, 3> one{{1, 0, 0}};
    assert(!D_VEC3_FEQ(origin, one, 1.0));
    assert(D_VEC3_FEQ(origin, one, std::nextafter(1.0, 2.0)));
    assert(!D_VEC3_FEQ(origin, origin, 0.0));
    assert(!D_VEC3_FEQ(origin, origin, -1.0));
    assert(!D_VEC3_FEQ(origin, origin, std::numeric_limits<double>::quiet_NaN()));
    assert(D_VEC3_FEQ(origin, one, std::numeric_limits<double>::infinity()));
    const std::array<std::uint64_t, 3> above53{{(std::uint64_t(1) << 53U) + 1U, 0, 0}};
    assert(!D_VEC3_FEQ(origin, above53, std::ldexp(1.0, 53)));
    assert(D_VEC3_FEQ(origin, above53, std::ldexp(1.0, 53) + 2.0));
    const std::array<std::uint64_t, 3> belowRoundedThreshold{{(std::uint64_t(1) << 53U) + 3U, 0, 0}};
    assert(D_VEC3_FEQ(origin, belowRoundedThreshold, std::ldexp(1.0, 53) + 4.0));
    const std::array<std::uint64_t, 3> full{{maximum, 0, 0}};
    assert(D_VEC3_FEQ(origin, full, std::ldexp(1.0, 64)));
    assert(!D_VEC3_FEQ(origin, full, std::nextafter(std::ldexp(1.0, 64), 0.0)));

    assert(D_ABS(-2.5) == 2.5 && D_SIGN2(-2.5, 1.0) == 2.5);
    assert(std::signbit(D_ABS(-0.0)) && std::signbit(D_SIGN2(-0.0, 1.0)));
    assert(D_FEQ(1.0, 1.0 + 0.5e-10) && !D_FEQ(1.0, 1.0 + 2.0e-10));
    assert(!D_FEQ(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()));
    assert(D_TO_DEGREES(1) == 57 && D_TO_RADIANS(180) == 3);
    assert(D_TO_DEGREES(1.25f) == static_cast<float>(1.25f * D_RADIANS_TO_DEGREES));
    assert(D_TO_RADIANS(90.0) == 90.0 * D_DEGREES_TO_RADIANS);
    assert(std::signbit(D_TO_DEGREES(-0.0)) && std::signbit(D_TO_RADIANS(-0.0)));
    checkScalarException<std::overflow_error>([] { (void)D_TO_DEGREES(std::numeric_limits<int>::max()); });
    checkScalarException<std::overflow_error>([] { (void)D_TO_DEGREES(std::numeric_limits<float>::max()); });
    checkScalarException<std::overflow_error>([] { (void)D_TO_DEGREES(std::numeric_limits<double>::max()); });
    for (double invalid : {std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        checkScalarException<std::domain_error>([&] { (void)D_TO_DEGREES(invalid); });
        checkScalarException<std::domain_error>([&] { (void)D_TO_RADIANS(invalid); });
    }
}

// Reference draws retain the historical polar Box-Muller algorithm. The
// affine reference uses explicit FMA, independent of compiler contraction.
double normalSampleReference()
{
    double first, second, square;
    do {
        first = 2.0 * DFunc::Frand() - 1.0;
        second = 2.0 * DFunc::Frand() - 1.0;
        square = first * first + second * second;
    } while (square >= 1.0 || square < 1E-30);
    return first * std::sqrt(-2.0 * std::log(square) / square);
}

void checkLegacyNormalRand()
{
    const auto initial = D_RANDOM_ENGINE();
    const double maximum = std::numeric_limits<double>::max();
    unsigned int finiteCancellations = 0U, overflows = 0U;
    for (unsigned int seed = 0U; seed < 128U; ++seed) {
        D_RANDOM_ENGINE().seed(seed);
        const double sample = normalSampleReference();
        const auto after = D_RANDOM_ENGINE();
        for (double scale : {-3.0, 0.0, 2.0}) {
            D_RANDOM_ENGINE().seed(seed);
            const double actual = DFunc::NormalRand(4.0, scale);
            const double expected = std::fma(sample, scale, 4.0);
            assert(std::fabs(actual - expected) <= 8.0 * std::numeric_limits<double>::epsilon() *
                   std::max(1.0, std::fabs(expected)));
            assert(D_RANDOM_ENGINE() == after);
        }
        if (sample > 1.0 && sample < 2.0) {
            D_RANDOM_ENGINE().seed(seed);
            double actual = 0.0;
            bool returned = false;
            try { actual = DFunc::NormalRand(-maximum, maximum); returned = true; }
            catch (const std::exception&) {}
            assert(returned && std::isfinite(actual) && actual == std::fma(sample, maximum, -maximum));
            assert(D_RANDOM_ENGINE() == after);
            ++finiteCancellations;
        }
        if (sample > 1.0e-6) {
            D_RANDOM_ENGINE().seed(seed);
            checkScalarException<std::overflow_error>([&] { (void)DFunc::NormalRand(maximum, maximum); });
            assert(D_RANDOM_ENGINE() == after);
            ++overflows;
        }
    }
    assert(finiteCancellations != 0U && overflows != 0U);
    for (double invalid : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()}) {
        const auto before = D_RANDOM_ENGINE();
        checkScalarException<std::domain_error>([&] { (void)DFunc::NormalRand(invalid, 1.0); });
        assert(D_RANDOM_ENGINE() == before);
        checkScalarException<std::domain_error>([&] { (void)DFunc::NormalRand(0.0, invalid); });
        assert(D_RANDOM_ENGINE() == before);
    }
    D_RANDOM_ENGINE() = initial;
}

struct CustomInterpolationValue { double value; };
CustomInterpolationValue operator+(CustomInterpolationValue a, CustomInterpolationValue b) { return {a.value + b.value}; }
CustomInterpolationValue operator-(CustomInterpolationValue a, CustomInterpolationValue b) { return {a.value - b.value}; }
CustomInterpolationValue operator*(double weight, CustomInterpolationValue value) { return {weight * value.value}; }

void checkLegacyMath()
{
    const auto initialEngine = D_RANDOM_ENGINE();
    const double maximum = std::numeric_limits<double>::max();
    const float huge = std::numeric_limits<DReal>::max();
    const float nan = std::numeric_limits<DReal>::quiet_NaN();
    const float infinity = std::numeric_limits<DReal>::infinity();
    for (unsigned int seed = 0U; seed < 64U; ++seed) {
        D_RANDOM_ENGINE().seed(seed);
        const double value = DFunc::Rand(maximum, -maximum);
        assert(std::isfinite(value) && value >= -maximum && value <= maximum);
        const auto afterScalar = D_RANDOM_ENGINE();
        D_RANDOM_ENGINE().seed(seed);
        (void)DFunc::Frand();
        assert(D_RANDOM_ENGINE() == afterScalar);
        D_RANDOM_ENGINE().seed(seed);
        const double reversed = DFunc::Rand(-maximum, maximum);
        assert(std::isfinite(reversed) && reversed >= -maximum && reversed <= maximum);
        const DTools::Vec2 two = DFunc::Rand(DTools::Vec2(huge), DTools::Vec2(-huge));
        const DTools::Vec3 three = DFunc::Rand(DTools::Vec3(huge), DTools::Vec3(-huge));
        for (int axis = 0; axis < 2; ++axis) assert(std::isfinite(two[axis]) && std::fabs(two[axis]) <= huge);
        for (int axis = 0; axis < 3; ++axis) assert(std::isfinite(three[axis]) && std::fabs(three[axis]) <= huge);
    }
    D_RANDOM_ENGINE().seed(5489U);
    const double expected = 12.0 * DFunc::Frand() - 3.0;
    const auto after = D_RANDOM_ENGINE();
    D_RANDOM_ENGINE().seed(5489U);
    assert(std::fabs(DFunc::Rand(9.0, -3.0) - expected) <= 4.0 * std::numeric_limits<double>::epsilon());
    assert(D_RANDOM_ENGINE() == after);
    // Keep the historical constructor argument structure (C++11 does not
    // specify axis evaluation order); verify draw counts and ordinary mapping.
    D_RANDOM_ENGINE().seed(5489U);
    const DTools::Vec3 expectedVector(DFunc::Frand(), 2.0 * DFunc::Frand(), 4.0 * DFunc::Frand());
    const auto afterVector = D_RANDOM_ENGINE();
    D_RANDOM_ENGINE().seed(5489U);
    assert(DFunc::Rand(DTools::Vec3(1.0f, 2.0f, 4.0f), DTools::Vec3(0.0f)) == expectedVector);
    assert(D_RANDOM_ENGINE() == afterVector);
    for (float invalid : {nan, infinity, -infinity}) {
        const auto prior = D_RANDOM_ENGINE();
        checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(invalid, 0.0); });
        checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(0.0, invalid); });
        checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(DTools::Vec2(0.0f), DTools::Vec2(invalid)); });
        checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(DTools::Vec3(invalid), DTools::Vec3(0.0f)); });
        assert(D_RANDOM_ENGINE() == prior);
        checkScalarException<std::domain_error>([&] { (void)DFunc::AverageVec(DTools::Vec3(invalid)); });
        for (int axis = 0; axis < 3; ++axis) {
            DTools::Vec3 point(0.0f); point[axis] = invalid;
            assert(!DFunc::IsPositive(point));
            checkScalarException<std::domain_error>([&] { (void)DFunc::AverageVec(point); });
        }
        for (int axis = 0; axis < 2; ++axis) {
            DTools::Vec2 invalidVector(0.0f); invalidVector[axis] = invalid;
            const auto before = D_RANDOM_ENGINE();
            checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(DTools::Vec2(1.0f), invalidVector); });
            assert(D_RANDOM_ENGINE() == before);
            checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(invalidVector, DTools::Vec2(0.0f)); });
            assert(D_RANDOM_ENGINE() == before);
            assert(!DFunc::InRange(invalidVector, DTools::Vec2(-1.0f), DTools::Vec2(1.0f)));
            assert(!DFunc::InRange(DTools::Vec2(0.0f), invalidVector, DTools::Vec2(1.0f)));
            assert(!DFunc::InRange(DTools::Vec2(0.0f), DTools::Vec2(-1.0f), invalidVector));
        }
        for (int axis = 0; axis < 3; ++axis) {
            DTools::Vec3 invalidVector(0.0f); invalidVector[axis] = invalid;
            const auto before = D_RANDOM_ENGINE();
            checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(invalidVector, DTools::Vec3(0.0f)); });
            assert(D_RANDOM_ENGINE() == before);
            checkScalarException<std::domain_error>([&] { (void)DFunc::Rand(DTools::Vec3(1.0f), invalidVector); });
            assert(D_RANDOM_ENGINE() == before);
        }
    }
    assert(DFunc::AverageVec(DTools::Vec3(huge)) == static_cast<double>(huge));
    assert(DFunc::AverageVec(DTools::Vec3(huge, 3.0f, -huge)) == 1.0);
    assert(DFunc::AverageVec(DTools::Vec3(1.0f, 2.0f, 3.0f)) == 2.0);
    assert(DFunc::IsPositive(DTools::Vec3(0.0f)));
    assert(DFunc::IsPositive(DTools::Vec3(-0.5e-10f)));
    assert(!DFunc::IsPositive(DTools::Vec3(-2.0e-10f)));
    assert(DFunc::InRange(DTools::Vec2(-1.0f, 1.0f), DTools::Vec2(-1.0f), DTools::Vec2(1.0f)));
    assert(!DFunc::InRange(DTools::Vec2(0.0f), DTools::Vec2(1.0f), DTools::Vec2(-1.0f)));
    D_RANDOM_ENGINE() = initialEngine;
}

template <typename Vector>
void checkVectorInterpolation()
{
    const Vector first(std::numeric_limits<DReal>::max()), last(-std::numeric_limits<DReal>::max());
    const Vector midpoint = D_LERP(first, last, 0.5f);
    assert(midpoint == Vector(0.0f));
    assert(D_LERP(first, last, 0.0f) == first && D_LERP(first, last, 1.0f) == last);
    checkScalarException<std::overflow_error>([&] { (void)D_LERP(first, last, 2.0f); });
}

void checkLegacyInterpolation()
{
    static_assert(std::is_same<decltype(D_LERP(0, 10, 0.25f)), float>::value, "Legacy float return type");
    static_assert(std::is_same<decltype(D_LERP(0, 10, 2)), int>::value, "Legacy integer return type");
    const double maximum = std::numeric_limits<double>::max();
    assert(D_LERP(maximum, -maximum, 0.0) == maximum);
    assert(D_LERP(maximum, -maximum, 1.0) == -maximum);
    assert(D_LERP(maximum, -maximum, 0.5) == 0.0);
    const float maxFloat = std::numeric_limits<float>::max();
    assert(D_LERP(maxFloat, -maxFloat, 0.5) == 0.0);
    assert(D_LERP(std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0.5) == -0.5);
    const std::int64_t minimum64 = std::numeric_limits<std::int64_t>::min();
    const std::int64_t maximum64 = std::numeric_limits<std::int64_t>::max();
    assert(D_LERP(minimum64, maximum64, 0.5) == -0.5);
    assert(D_LERP(std::int64_t(0), std::int64_t(1), maximum64) == maximum64);
    assert(D_LERP(std::int64_t(0), std::int64_t(1), minimum64) == minimum64);
    assert(D_LERP(0, 1, maximum64) == maximum64);
    assert(D_LERP(std::uint64_t(0), std::uint64_t(1), std::numeric_limits<std::uint64_t>::max()) ==
           std::numeric_limits<std::uint64_t>::max());
    assert(D_LERP(10, 20, -1) == 0 && D_LERP(10, 20, 2) == 30);
    assert(D_LERP(0, 10, 0.25f) == 2.5f);
    const double wideFirst = -std::ldexp(1.0, 63);
    assert(D_LERP(wideFirst, wideFirst + 1024.0, (std::int64_t(1) << 53U) + 1) == 1024.0);
    assert(D_LERP(-1, 1, std::uint64_t(1)) == 1U);
    checkScalarException<std::overflow_error>([] {
        (void)D_LERP(std::uint64_t(2), std::uint64_t(1), std::numeric_limits<std::uint64_t>::max());
    });
    assert(D_LERP(10.0, 20.0, -1.0) == 0.0 && D_LERP(10.0, 20.0, 2.0) == 30.0);
    assert(std::signbit(D_LERP(-0.0, 1.0, 0.0)));
    checkVectorInterpolation<DTools::Vec2>();
    checkVectorInterpolation<DTools::Vec3>();
    checkVectorInterpolation<DTools::Vec4>();
    assert(D_LERP(CustomInterpolationValue{1.0}, CustomInterpolationValue{3.0}, 0.5).value == 2.0);
    checkScalarException<std::overflow_error>([&] { (void)D_LERP(maximum, -maximum, 2.0); });
    checkScalarException<std::overflow_error>([&] { (void)D_LERP(std::int64_t(1), std::int64_t(2), maximum64); });
    checkScalarException<std::domain_error>([] {
        (void)D_LERP(0.0, 1.0, std::numeric_limits<double>::quiet_NaN());
    });
    for (int i = -64; i <= 64; ++i) {
        const double first = i * 0.125, last = (i + 3) * 0.125;
        for (double weight : {-1.0, 0.0, 0.25, 0.5, 0.75, 1.0, 2.0})
            assert(D_LERP(first, last, weight) == first + weight * (last - first));
    }
    for (int first = -8; first <= 8; ++first)
        for (int last = -8; last <= 8; ++last)
            for (int weight = -4; weight <= 4; ++weight)
                assert(D_LERP(first, last, weight) == first + weight * (last - first));
}

void checkLegacyGradation()
{
    const double first[3] = {0, 0, 0}, last[3] = {1, 2, 3};
    const double maximum = std::numeric_limits<double>::max();
    double result[3] = {91, 92, 93};
    DFunc::Gradation(result, first, last, 0.0, -maximum, maximum);
    assert(result[0] == 0.5 && result[1] == 1.0 && result[2] == 1.5);
    DFunc::Gradation(result, first, last, maximum, -maximum, maximum);
    assert(result[0] == 1 && result[1] == 2 && result[2] == 3);
    DFunc::Gradation(result, first, last, 1.0, 2.0, 0.0);
    assert(result[0] == 0.5 && result[1] == 1.0 && result[2] == 1.5);
    DFunc::Gradation(result, first, last, maximum, 0.0, 1.0e-10);
    assert(result[0] == 1 && result[1] == 2 && result[2] == 3);
    for (double collapsed : {0.0, 0.5e-10, -0.5e-10}) {
        result[0] = 91; result[1] = 92; result[2] = 93;
        DFunc::Gradation(result, first, last, 0.0, 0.0, collapsed);
        assert(result[0] == 91 && result[1] == 92 && result[2] == 93);
    }
    const double invalid[3] = {1, 2, std::numeric_limits<double>::quiet_NaN()};
    checkScalarException<std::domain_error>([&] { DFunc::Gradation(result, first, invalid, 0.5); });
    assert(result[0] == 91 && result[1] == 92 && result[2] == 93);
    double overlap[4] = {1, 2, 3, 4};
    DFunc::Gradation(overlap + 1, overlap, first, 0.5);
    assert(overlap[0] == 1 && overlap[1] == 0.5 && overlap[2] == 1 && overlap[3] == 1.5);
    checkScalarException<std::invalid_argument>([&] { DFunc::Gradation(nullptr, first, last, 0.5); });
    checkScalarException<std::domain_error>([&] {
        DFunc::Gradation(result, first, last, std::numeric_limits<double>::infinity());
    });
    assert(result[0] == 91 && result[1] == 92 && result[2] == 93);
}

struct FailedTextInsertion { bool bad; };
std::ostream& operator<<(std::ostream& output, const FailedTextInsertion& value)
{
    output << "partial";
    output.setstate(value.bad ? std::ios::badbit : std::ios::failbit);
    return output;
}

void checkLegacyStringConversion()
{
    for (bool bad : {false, true})
        checkScalarException<std::runtime_error>([&] { (void)D_TO_STRING(FailedTextInsertion{bad}); });
    assert(D_TO_STRING(std::string("complete")) == "complete");
    {
        dommy_test::ScopedNumericLocale locale(',', '.');
        std::ostringstream reference;
        reference << 1234.5;
        assert(D_TO_STRING(1234.5) == reference.str());
        assert(std::locale() == locale.selected());
    }
#ifdef DOMMY_UTILITY_ALLOCATION_TESTS
    const std::string payload(4096U, 'x');
    assert(D_TO_STRING(payload) == payload);
    unsigned int faults = 0U, rejections = 0U;
    for (std::size_t selected = 1U; selected <= 64U; ++selected) {
        std::string result("prior");
        bool returned = false;
        fault::arm(selected);
        try { result = D_TO_STRING(payload); returned = true; }
        catch (const std::exception&) {}
        fault::armed = false;
        if (fault::triggered) ++faults;
        if (returned) assert(result == payload);
        else { ++rejections; assert(fault::triggered && result == "prior"); }
    }
    assert(faults != 0U && rejections != 0U);
#endif
}

template <typename Vector>
void checkValueEquality(int dimensions)
{
    static_assert(!std::is_convertible<Vector, DReal*>::value,
                  "Raw-buffer access must be explicit");
    static_assert(!std::is_convertible<const Vector&, const DReal*>::value,
                  "Const vectors must not silently compare buffer addresses");
    static_assert(!std::is_convertible<Vector, bool>::value,
                  "Vectors must not silently become pointer truth values");
    static_assert(std::is_standard_layout<Vector>::value,
                  "The historical vector storage layout must remain standard");
    assert(sizeof(Vector) == static_cast<std::size_t>(dimensions) * sizeof(DReal));

    Vector a(1.0f), b(1.0f);
    const Vector& ca = a;
    const Vector& cb = b;
    assert(a == b && !(a != b));
    assert(ca == cb && !(ca != cb));
    assert(a == cb && ca == b);
    assert(static_cast<DReal*>(a) == a.data);
    assert(static_cast<const DReal*>(ca) == ca.data);
    b[dimensions - 1] = 2.0f;
    assert(a != b && !(a == b));
    assert(ca != cb && !(ca == cb));
    a = 0.0f;
    b = -0.0f;
    assert(ca == cb && !(ca != cb));
    a[0] = b[0] = std::numeric_limits<DReal>::quiet_NaN();
    assert(!(ca == cb) && ca != cb);
}

template <typename Vector>
void checkStrictWeakOrder(std::vector<Vector> values)
{
    const auto equivalent = [](const Vector& a, const Vector& b) {
        return !(a < b) && !(b < a);
    };
    for (const Vector& a : values) {
        assert(!(a < a));
        for (const Vector& b : values) {
            assert(!(a < b && b < a));
            assert((a > b) == (b < a));
            assert((a <= b) == !(b < a));
            assert((a >= b) == !(a < b));
            for (const Vector& c : values) {
                if (a < b && b < c) assert(a < c);
                if (equivalent(a, b) && equivalent(b, c))
                    assert(equivalent(a, c));
            }
        }
    }
    std::sort(values.begin(), values.end());
    assert(std::is_sorted(values.begin(), values.end()));
    const std::set<Vector> unique(values.begin(), values.end());
    assert(!unique.empty());
    assert(std::is_sorted(unique.begin(), unique.end()));
}

void checkOrdering()
{
    const DReal infinity = std::numeric_limits<DReal>::infinity();
    const DReal nan = std::numeric_limits<DReal>::quiet_NaN();
    const DReal maximum = std::numeric_limits<DReal>::max();
    const DTools::Vec3 a(0.0f, 0.0f, 0.0f);
    const DTools::Vec3 b(0.75e-10f, 0.0f, 0.0f);
    const DTools::Vec3 c(1.5e-10f, 0.0f, 0.0f);
    assert(a < b && b < c && a < c);
    // Approximate matching intentionally need not be transitive; ordering must.
    assert(D_VEC3_FEQ(a, b) && D_VEC3_FEQ(b, c) && !D_VEC3_FEQ(a, c));
    const std::vector<DTools::Vec3> values3 = {
        DTools::Vec3(-infinity, 0.0f, 0.0f), DTools::Vec3(-1.0f, 0.0f, 0.0f),
        DTools::Vec3(-0.0f, 0.0f, 0.0f), a, b, c,
        DTools::Vec3(0.0f, 1.0f, 0.0f), DTools::Vec3(0.0f, 0.0f, 1.0f),
        DTools::Vec3(infinity, 0.0f, 0.0f), DTools::Vec3(nan, 0.0f, 0.0f),
        DTools::Vec3(nan, 1.0f, 0.0f), DTools::Vec3(nan, 0.0f, 0.0f)};
    checkStrictWeakOrder(values3);
    assert(std::set<DTools::Vec3>(values3.begin(), values3.end()).size() == 10U);
    assert(DTools::Vec3(infinity, 0.0f, 0.0f) < DTools::Vec3(nan, 0.0f, 0.0f));
    assert(DTools::Vec3(0.0f, infinity, 0.0f) < DTools::Vec3(0.0f, nan, 0.0f));
    assert(DTools::Vec3(0.0f, 0.0f, infinity) < DTools::Vec3(0.0f, 0.0f, nan));

    const DTools::Vec2 halfMaximum(maximum * 0.5f, 0.0f);
    const DTools::Vec2 fullMaximum(maximum, 0.0f);
    assert(halfMaximum < fullMaximum);
    assert(DTools::Vec2(1.0e-30f, 0.0f) < DTools::Vec2(2.0e-30f, 0.0f));
    // Retain magnitude order, not a new lexicographical order for Vec2.
    assert(DTools::Vec2(0.0f, 2.0f) > DTools::Vec2(1.0f, 0.0f));
    assert(!(DTools::Vec2(1.0f, 0.0f) < DTools::Vec2(0.0f, 1.0f)));
    const std::vector<DTools::Vec2> values2 = {
        DTools::Vec2(-0.0f, 0.0f), DTools::Vec2(0.0f, 0.0f),
        DTools::Vec2(1.0f, 0.0f), DTools::Vec2(0.0f, 1.0f),
        DTools::Vec2(-1.0f, 0.0f), fullMaximum, halfMaximum,
        DTools::Vec2(infinity, 0.0f), DTools::Vec2(nan, 0.0f),
        DTools::Vec2(0.0f, nan), DTools::Vec2(1.0e-30f, 0.0f),
        DTools::Vec2(2.0e-30f, 0.0f)};
    checkStrictWeakOrder(values2);
    assert(std::set<DTools::Vec2>(values2.begin(), values2.end()).size() == 8U);
}

template <typename Vector>
void checkNormalization(int dimensions)
{
    const DReal maximum = std::numeric_limits<DReal>::max();
    const DReal magnitudes[] = {1.0f, -1.0f, 1.0e30f, -1.0e30f,
        std::numeric_limits<DReal>::min(), std::numeric_limits<DReal>::denorm_min(),
        maximum, -maximum};
    for (DReal magnitude : magnitudes) {
        Vector value(magnitude);
        const DReal originalLength = DTools::norm(value);
        const double expectedLength = std::fabs(static_cast<double>(magnitude)) *
                                      std::sqrt(static_cast<double>(dimensions));
        if (expectedLength > maximum) assert(std::isinf(originalLength));
        else {
            assert(std::isfinite(originalLength) && originalLength > 0.0f);
            // Permit one subnormal rounding unit, not a flush-to-zero result.
            const double tolerance = std::max(expectedLength * 1.0e-6,
                static_cast<double>(std::numeric_limits<DReal>::denorm_min()));
            assert(std::fabs(originalLength - expectedLength) <= tolerance);
        }
        const DReal returnedLength = DTools::unitize(value);
        assert(returnedLength == originalLength);
        const double expectedComponent = std::copysign(
            1.0 / std::sqrt(static_cast<double>(dimensions)), magnitude);
        for (int axis = 0; axis < dimensions; ++axis) {
            assert(std::isfinite(value[axis]));
            assert(std::fabs(value[axis] - expectedComponent) < 1.0e-6);
        }
        assert(std::fabs(DTools::norm(value) - 1.0f) < 1.0e-6);
        Vector viaNormalize(magnitude);
        assert(DTools::normalize(viaNormalize) == originalLength);
        assert(viaNormalize == value);
    }

    Vector zero(0.0f);
    assert(DTools::unitize(zero) == 0.0f && zero == Vector(0.0f));
    const DReal invalid[] = {std::numeric_limits<DReal>::infinity(),
        -std::numeric_limits<DReal>::infinity(), std::numeric_limits<DReal>::quiet_NaN()};
    for (int axis = 0; axis < dimensions; ++axis) {
        for (DReal component : invalid) {
            Vector value(1.0f);
            value[axis] = component;
            assert(DTools::unitize(value) == 0.0f && value == Vector(0.0f));
        }
    }
}

void checkUnitFunctions()
{
    const DReal magnitudes[] = {0.0f, 1.0f, -1.0f, 1.0e30f,
        std::numeric_limits<DReal>::max(), std::numeric_limits<DReal>::denorm_min(),
        std::numeric_limits<DReal>::infinity(), std::numeric_limits<DReal>::quiet_NaN()};
    for (DReal magnitude : magnitudes) {
        const DTools::Vec3 source3(magnitude);
        DTools::Vec3 expected3(source3);
        DTools::unitize(expected3);
        assert(DTools::Unit(source3) == expected3);
        assert(DTools::Unit(magnitude, magnitude, magnitude) == expected3);
        const DTools::Vec2 source2(magnitude);
        DTools::Vec2 expected2(source2);
        DTools::unitize(expected2);
        assert(DTools::Unit(source2) == expected2);
        assert(DTools::Unit(magnitude, magnitude) == expected2);
    }
    DTools::Vec3 ordinary(3.0f, 4.0f, 0.0f);
    assert(DTools::norm2(ordinary) == 25.0f && DTools::length(ordinary) == 5.0f);
    assert(DTools::unitize(ordinary) == 5.0f);
    assert(std::fabs(ordinary[0] - 0.6f) < 1.0e-6f);
    assert(std::fabs(ordinary[1] - 0.8f) < 1.0e-6f);
    DTools::Vec4 small(1.0e-6f);
    assert(std::fabs(DTools::unitize(small) - 2.0e-6f) < 1.0e-12f);
    assert(small == DTools::Vec4(0.5f));
    // Finite components do not lose their direction when length exceeds float.
    DTools::Vec3 huge(std::numeric_limits<DReal>::max());
    assert(std::isinf(DTools::unitize(huge)));
    assert(std::fabs(huge[0] - 1.0f / std::sqrt(3.0f)) < 1.0e-6f);
}

template <typename Vector>
void checkProjection(int dimensions)
{
    Vector source(0.0f);
    for (int axis = 0; axis < dimensions; ++axis) source[axis] = 3.0f + axis;
    Vector expected(source);
    expected[0] = 0.0f;
    const DReal scales[] = {1.0f, -1.0f, 1.0e30f, -1.0e30f, 1.0e-30f,
        std::numeric_limits<DReal>::min(), std::numeric_limits<DReal>::denorm_min(),
        std::numeric_limits<DReal>::max()};
    for (DReal scale : scales) {
        Vector direction(0.0f);
        direction[0] = scale;
        assert(DTools::remCompB(source, direction) == expected);
        Vector inPlace(source);
        DTools::remCompB2(inPlace, direction);
        assert(inPlace == expected);

        // A rotated direction, with no dependence on its magnitude/sign.
        direction[1] = scale;
        const Vector projected = DTools::remCompB(source, direction);
        assert(std::fabs(projected[0] + 0.5f) < 1.0e-6f);
        assert(std::fabs(projected[1] - 0.5f) < 1.0e-6f);
        for (int axis = 2; axis < dimensions; ++axis)
            assert(projected[axis] == source[axis]);
        Vector parallel(scale);
        assert(DTools::remCompB(parallel, parallel) == Vector(0.0f));
        DTools::remCompB2(parallel, parallel);
        assert(parallel == Vector(0.0f));
    }
    assert(DTools::remCompB(source, Vector(0.0f)) == source);
    Vector orthogonal(0.0f), direction(0.0f);
    orthogonal[1] = 7.0f;
    direction[0] = 2.0f;
    assert(DTools::remCompB(orthogonal, direction) == orthogonal);

    const DReal invalid[] = {std::numeric_limits<DReal>::infinity(),
        -std::numeric_limits<DReal>::infinity(), std::numeric_limits<DReal>::quiet_NaN()};
    for (int axis = 0; axis < dimensions; ++axis) {
        for (DReal value : invalid) {
            Vector badDirection(1.0f);
            badDirection[axis] = value;
            bool rejected = false;
            try { (void)DTools::remCompB(source, badDirection); }
            catch (const std::domain_error&) { rejected = true; }
            assert(rejected);
            Vector inPlace(source);
            rejected = false;
            try { DTools::remCompB2(inPlace, badDirection); }
            catch (const std::domain_error&) { rejected = true; }
            assert(rejected && inPlace == source);
            Vector badSource(source);
            badSource[axis] = value;
            Vector original(badSource);
            rejected = false;
            try { DTools::remCompB2(badSource, Vector(0.0f)); }
            catch (const std::domain_error&) { rejected = true; }
            assert(rejected && std::memcmp(badSource.data, original.data, sizeof(badSource.data)) == 0);
        }
    }

    // A valid orthogonal projection can exceed the float component range.
    Vector huge(0.0f), skew(0.0f);
    huge[0] = huge[1] = std::numeric_limits<DReal>::max();
    skew[0] = 1.0f;
    skew[1] = -0.5f;
    const Vector original(huge);
    bool rejected = false;
    try { DTools::remCompB2(huge, skew); }
    catch (const std::domain_error&) { rejected = true; }
    assert(rejected && huge == original);
    skew[1] = 0.5f;
    const Vector representable = DTools::remCompB(original, skew);
    const double maximum = std::numeric_limits<DReal>::max();
    assert(std::fabs(representable[0] + 0.2 * maximum) < 1.0e-6 * maximum);
    assert(std::fabs(representable[1] - 0.4 * maximum) < 1.0e-6 * maximum);
    Vector nonUniform(std::numeric_limits<DReal>::max());
    nonUniform[0] *= 0.3f;
    nonUniform[1] *= 0.7f;
    assert(DTools::remCompB(nonUniform, nonUniform) == Vector(0.0f));
    DTools::remCompB2(nonUniform, nonUniform);
    assert(nonUniform == Vector(0.0f));

    // Independent wide reference for nonsymmetric directions and ordinary
    // rounding. Use the actual float direction after scaling, not idealized
    // ratios which float storage may have already changed.
    const DReal referenceScales[] = {1.0f, 1.0e30f, 1.0e-30f};
    for (int sample = 0; sample < 512; ++sample) {
        Vector a(0.0f), b(0.0f);
        for (int axis = 0; axis < dimensions; ++axis) {
            a[axis] = static_cast<DReal>((sample * 53 + axis * 127) % 1001 - 500) * 0.25f;
            b[axis] = static_cast<DReal>((sample * 17 + axis * 23) % 61 - 30) * 0.125f;
        }
        for (DReal scale : referenceScales) {
            const Vector direction = b * scale;
            long double numerator = 0.0L, denominator = 0.0L;
            for (int axis = 0; axis < dimensions; ++axis) {
                numerator += static_cast<long double>(a[axis]) * direction[axis];
                denominator += static_cast<long double>(direction[axis]) * direction[axis];
            }
            const Vector result = DTools::remCompB(a, direction);
            for (int axis = 0; axis < dimensions; ++axis) {
                const long double expectedValue = denominator == 0.0L ? a[axis] :
                    a[axis] - static_cast<long double>(direction[axis]) * (numerator / denominator);
                assert(std::isfinite(result[axis]));
                assert(std::fabs(result[axis] - expectedValue) <=
                    1.0e-6L * std::max(1.0L, std::fabs(expectedValue)));
            }
        }
    }
}

} // namespace

void runUtilityVectorTests()
{
    checkIndexRange();
    checkScalarTemplates();
    checkLegacyInterpolation();
    checkLegacyGradation();
    checkLegacyStringConversion();
    checkLegacyMath();
    checkLegacyNormalRand();
    checkValueEquality<DTools::Vec2>(2);
    checkValueEquality<DTools::Vec3>(3);
    checkValueEquality<DTools::Vec4>(4);
    checkOrdering();
    checkNormalization<DTools::Vec2>(2);
    checkNormalization<DTools::Vec3>(3);
    checkNormalization<DTools::Vec4>(4);
    checkUnitFunctions();
    checkProjection<DTools::Vec2>(2);
    checkProjection<DTools::Vec3>(3);
}

#ifdef DOMMY_UTILITY_VECTOR_TEST_MAIN
int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "index-range") == 0) checkIndexRange();
    else if (argc == 2 && std::strcmp(argv[1], "lerp") == 0) checkLegacyInterpolation();
    else if (argc == 2 && std::strcmp(argv[1], "gradation") == 0) checkLegacyGradation();
    else if (argc == 2 && std::strcmp(argv[1], "format") == 0) checkLegacyStringConversion();
    else if (argc == 2 && std::strcmp(argv[1], "legacy-math") == 0) checkLegacyMath();
    else if (argc == 2 && std::strcmp(argv[1], "normal") == 0) checkLegacyNormalRand();
    else runUtilityVectorTests();
}
#endif
