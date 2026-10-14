#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DMayaRandomValueGeneration.h"
#include "common/utility/DUniformSampling.h"

#include <array>
#include <cassert>
#include <climits>
#include <cfloat>
#include <iostream>

namespace {

template <typename Exception, typename Action>
void expectException(Action action) {
    bool caught = false;
    try { action(); } catch (const Exception&) { caught = true; }
    assert(caught);
}

void testSeeds() {
    using namespace dommy::random;
    int result = 17;
    assert(!offsetNonnegativeSeed(-1, 1U, result) && result == 17);
    expectException<std::invalid_argument>([] { requireOffsetNonnegativeSeed(-1, 0U); });
    const std::uint64_t modulus = static_cast<std::uint64_t>(INT_MAX) + 1U;
    for (int seed : {0, 1, INT_MAX - 1, INT_MAX}) {
        for (std::uint64_t offset : {std::uint64_t(0), std::uint64_t(1), std::uint64_t(2),
                std::uint64_t(50), modulus, std::numeric_limits<std::uint64_t>::max()}) {
            const int expected = static_cast<int>((offset % modulus + seed) % modulus);
            assert(offsetNonnegativeSeed(seed, offset, result) && result == expected);
            assert(requireOffsetNonnegativeSeed(seed, offset) == expected);
        }
    }
}

template <typename T>
void testRealValues() {
    using namespace dommy::random;
    for (std::uint32_t seed : {0U, 42U, UINT_MAX}) {
        for (bool inclusive : {false, true}) {
            const T minimum = T(-2.5), maximum = T(7.25);
            std::vector<T> values, expected(64U);
            std::mt19937 engine(seed);
            std::uniform_real_distribution<T> distribution(minimum,
                inclusive ? std::nextafter(maximum, std::numeric_limits<T>::max()) : maximum);
            for (T& value : expected) value = distribution(engine);
            generateValues(seed, maximum, minimum, 64U, values, inclusive);
            assert(values == expected);
        }
    }
    const T maximum = std::numeric_limits<T>::max();
    const std::vector<T> original(1U, T(17));
    std::vector<T> values = original;
    for (T invalid : {std::numeric_limits<T>::infinity(),
                      std::numeric_limits<T>::quiet_NaN()}) {
        expectException<std::invalid_argument>([&] {
            generateRealValues(1U, invalid, T(1), 3U, values);
        });
        assert(values == original);
    }
    expectException<std::invalid_argument>([&] {
        generateRealValues(1U, -maximum, maximum, 3U, values);
    });
    assert(values == original);
    // The original width fits, but the legacy inclusive upper step does not.
    expectException<std::invalid_argument>([&] {
        generateRealValues(1U, -maximum / T(2), maximum / T(2), 3U, values, true);
    });
    assert(values == original);
    expectException<std::length_error>([&] {
        generateRealValues(1U, T(0), T(1), dommy::generation::kMaxGeneratedPointCount + 1U, values);
    });
    assert(values == original);
    generateRealValues(1U, maximum, maximum, 3U, values, true);
    assert(values == std::vector<T>(3U, maximum));
    generateRealValues(1U, T(0), T(1), 0U, values);
    assert(values.empty());
}

void testTriples() {
    using namespace dommy::random;
    typedef std::array<float, 3> Vec;
    const std::array<double, 3> minimum = {{-2.0, 0.0, 9.0}};
    const std::array<double, 3> maximum = {{3.0, 0.0, 15.0}};
    std::vector<Vec> values;
    for (int seed : {1, 42, INT_MAX - 1, INT_MAX}) {
        generateRealTriples<float>(seed, minimum, maximum, 64U, values);
        for (unsigned int axis = 0U; axis < 3U; ++axis) {
            std::vector<float> expected;
            generateRealValues(static_cast<std::uint32_t>(requireOffsetNonnegativeSeed(seed, axis)),
                               float(minimum[axis]), float(maximum[axis]), 64U, expected, true);
            for (std::size_t i = 0U; i < values.size(); ++i) assert(values[i][axis] == expected[i]);
        }
    }
    const auto original = values;
    std::array<double, 3> invalid = maximum;
    invalid[2] = 1e100;
    expectException<std::invalid_argument>([&] {
        generateRealTriples<float>(1, invalid, invalid, 1U, values);
    });
    assert(values == original);
    invalid[2] = std::numeric_limits<double>::quiet_NaN();
    expectException<std::invalid_argument>([&] {
        generateRealTriples<float>(1, minimum, invalid, 1U, values);
    });
    assert(values == original);
    expectException<std::invalid_argument>([&] {
        generateRealTriples<float>(-1, minimum, maximum, 1U, values);
    });
    assert(values == original);
    generateRealTriples<float>(1, minimum, maximum, 0U, values);
    assert(values.empty());
}

enum class Failure { None, Allocate, ShortAllocate, Copy };
Failure arrayFailure = Failure::None;
struct DoubleArray {
    std::vector<double> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    MStatus setLength(unsigned int count) {
        if (arrayFailure == Failure::Allocate) return MS::kFailure;
        values.resize(arrayFailure == Failure::ShortAllocate && count ? count - 1U : count);
        return MS::kSuccess;
    }
    MStatus copy(const DoubleArray& other) {
        if (arrayFailure == Failure::Copy) return MS::kFailure;
        values = other.values;
        return MS::kSuccess;
    }
    double& operator[](unsigned int index) { return values.at(index); }
};

void testMayaAdapter() {
    DoubleArray output;
    output.values = {17.0};
    for (Failure failure : {Failure::Allocate, Failure::ShortAllocate, Failure::Copy}) {
        arrayFailure = failure;
        assert(!dommy::maya::generateRandomDoubleArray(1, -2.0, 7.0, 8U, output));
        assert(output.values == std::vector<double>({17.0}));
    }
    arrayFailure = Failure::None;
    assert(!dommy::maya::generateRandomDoubleArray(1, -DBL_MAX, DBL_MAX, 8U, output));
    assert(output.values == std::vector<double>({17.0}));
    assert(dommy::maya::generateRandomArray(1, 0.0, 1.0,
        std::size_t(UINT_MAX) + 1U, output) == MS::kInvalidParameter);
    assert(output.values == std::vector<double>({17.0}));
    for (bool inclusive : {false, true}) {
        assert(dommy::maya::generateRandomArray(42, -2.0, 7.0, 64U, output, inclusive) == MS::kSuccess);
        std::vector<double> expected;
        dommy::random::generateRealValues(42U, -2.0, 7.0, 64U, expected, inclusive);
        assert(output.values == expected);
    }
    assert(dommy::maya::generateRandomDoubleArray(1, 5.0, 5.0, 4U, output));
    assert(output.values == std::vector<double>(4U, 5.0));
    assert(dommy::maya::generateRandomDoubleArray(1, 0.0, 1.0, 0U, output));
    assert(output.values.empty());
}

void testUniformSamples() {
    using dommy::generation::generateUniformFloatSamples;
    const float maximum = std::numeric_limits<float>::max();
    for (const auto& range : std::vector<std::array<float, 2>>{
            {{0, 1}}, {{-maximum, maximum}}, {{1.25f, 1.5f}}, {{-3, -1}}, {{4, 4}}}) {
        for (std::size_t count : {0U, 1U, 2U, 3U, 32U}) {
            std::vector<float> output(1U, 17.0f);
            assert(generateUniformFloatSamples(range[0], range[1], count, output));
            assert(output.size() == count);
            if (count) assert(output.front() == range[0]);
            if (count > 1U) assert(output.back() == range[1]);
            for (std::size_t i = 0U; i < count; ++i) {
                assert(std::isfinite(output[i]) && output[i] >= range[0] && output[i] <= range[1]);
                if (i) assert(output[i] >= output[i - 1U]);
            }
        }
    }
    std::vector<float> output(1U, 17.0f);
    assert(!generateUniformFloatSamples(2, 1, 2U, output));
    assert(!generateUniformFloatSamples(0, std::numeric_limits<float>::quiet_NaN(), 2U, output));
    assert(!generateUniformFloatSamples(0, std::numeric_limits<float>::infinity(), 0U, output));
    assert(!generateUniformFloatSamples(0, 1, dommy::generation::kMaxGeneratedPointCount + 1U, output));
    assert(output == std::vector<float>({17.0f}));
}

} // namespace

void runRandomSamplingTests() {
    testSeeds();
    testRealValues<float>();
    testRealValues<double>();
    testTriples();
    testMayaAdapter();
    testUniformSamples();
    std::cout << "Random and uniform sampling tests: OK\n";
}
