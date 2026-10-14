#include <cstdlib>
#include <new>
std::size_t failAllocationBytes = 0U;
void* operator new(std::size_t bytes) {
    if (bytes == failAllocationBytes && bytes != 0U) {
        failAllocationBytes = 0U;
        throw std::bad_alloc();
    }
    if (void* result = std::malloc(bytes == 0U ? 1U : bytes)) return result;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }

#include "DViewportAttributeConversion.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <type_traits>

using Mode = dommy::viewport::AttributeDisplayMode;
using InputValue = std::array<long double, 3>;
std::size_t attributeReads = 0U;

template <typename T> std::vector<T> typedValues(const std::vector<InputValue>& values) {
    using Components = dommy::numeric::ValueComponents<T>;
    std::vector<T> result;
    for (const auto& source : values) {
        T value{};
        for (unsigned int axis = 0U; axis < Components::count; ++axis)
            Components::get(value, axis) = static_cast<typename Components::Scalar>(source[axis]);
        result.push_back(value);
    }
    return result;
}

struct FakeGeometry {
    std::size_t points = 2U;
    bool hasAttribute = true;
    std::vector<InputValue> values{{{2, 3, 4}}, {{4, 5, 6}}};
    std::size_t numberOfPoints() const { return points; }
    bool isExistAttr(const std::string&) const { return hasAttribute; }
    template <typename T> std::vector<T> attrListValue(const std::string&) const {
        ++attributeReads;
        return typedValues<T>(values);
    }
};
struct FakeOwner {
    std::vector<std::shared_ptr<FakeGeometry>> geometries;
    std::vector<InputValue> values{{{2, 3, 4}}, {{4, 5, 6}}};
    FakeOwner() : geometries{std::make_shared<FakeGeometry>(), std::make_shared<FakeGeometry>()} {
        geometries[1]->points = 3U;
        geometries[1]->values.push_back({{6, 7, 8}});
    }
    std::size_t numberOfCurves() const { return geometries.size(); }
    std::size_t numberOfMesh() const { return geometries.size(); }
    auto beginCurves() const -> decltype(geometries.begin()) { return geometries.begin(); }
    auto endCurves() const -> decltype(geometries.end()) { return geometries.end(); }
    auto beginMeshes() const -> decltype(geometries.begin()) { return geometries.begin(); }
    auto endMeshes() const -> decltype(geometries.end()) { return geometries.end(); }
    template <typename T> std::vector<T> attrListValue(const std::string&) const {
        ++attributeReads;
        return typedValues<T>(values);
    }
};
using DCurveData = FakeOwner;
using DMeshData = FakeOwner;
#include "viewport_attribute_methods.inc"

// Independent expected layouts, including the original double luminance math.
template <typename T> std::vector<float> golden(const InputValue& source, Mode mode, bool black) {
    const T typed = typedValues<T>({source})[0];
    using Components = dommy::numeric::ValueComponents<T>;
    double c[3] = {0, 0, 0};
    for (unsigned int i = 0U; i < Components::count; ++i) c[i] = Components::get(typed, i);
    const float x = static_cast<float>(Components::get(typed, 0U));
    const float y = Components::count > 1U ? static_cast<float>(Components::get(typed, 1U)) : 0.0f;
    const float z = Components::count > 2U ? static_cast<float>(Components::get(typed, 2U)) : 0.0f;
    switch (mode) {
    case Mode::Scalar: return {x};
    case Mode::ScalarVector: return {0, x, 0};
    case Mode::Vector2: return {x, y};
    case Mode::Vector2XZ: return {x, 0, y};
    case Mode::Vector3: return {x, y, z};
    case Mode::ScalarColor: return {x, x, x, black ? x : 1.0f};
    case Mode::ScalarLuminanceColor:
        return {x, x, x, black ? static_cast<float>(0.2126*c[0]+0.7152*c[0]+0.0722*c[0]) : 1.0f};
    case Mode::Vector2Color:
        return {x, y, 0, black ? static_cast<float>(0.2126*c[0]+0.7152*c[1]) : 1.0f};
    case Mode::Vector3Color:
        return {x, y, z, black ? static_cast<float>(0.2126*c[0]+0.7152*c[1]+0.0722*c[2]) : 1.0f};
    }
    std::abort();
}
void append(std::vector<float>& output, const std::vector<float>& values) {
    output.insert(output.end(), values.begin(), values.end());
}
template <typename Run> void rejectedUnchanged(Run run) {
    std::vector<float> output{7.0f};
    bool rejected = false;
    try { run(output); } catch (const std::exception&) { rejected = true; }
    assert(rejected && output == std::vector<float>{7.0f});
}

template <typename T, typename Run> void testRaw(Mode mode, bool black, bool appends, Run run) {
    const std::vector<InputValue> input{{{2, 3, 4}}, {{4, 5, 6}}};
    std::vector<float> expected = appends ? std::vector<float>{7.0f} : std::vector<float>{};
    for (const auto& value : input) append(expected, golden<T>(value, mode, black));
    std::vector<float> output{7.0f};
    run(typedValues<T>(input), output);
    assert(output == expected);
    output = {7.0f}; run({}, output);
    assert(output == (appends ? std::vector<float>{7.0f} : std::vector<float>{}));
    if (std::is_floating_point<typename dommy::numeric::ValueComponents<T>::Scalar>::value) {
        for (long double bad : {1.0e300L, -1.0e300L,
             std::numeric_limits<long double>::infinity(), std::numeric_limits<long double>::quiet_NaN()}) {
            // Only double test values use finite numbers outside float's range.
            if (sizeof(typename dommy::numeric::ValueComponents<T>::Scalar) == sizeof(float) && std::isfinite(bad)) continue;
            for (unsigned int axis = 0U; axis < dommy::numeric::ValueComponents<T>::count; ++axis) {
                auto invalid = input; invalid.back()[axis] = bad;
                rejectedUnchanged([&](std::vector<float>& out) { run(typedValues<T>(invalid), out); });
            }
        }
    }
}

template <typename T, typename Run> void testMarker(Mode mode, bool black, bool point, Run run) {
    FakeOwner input;
    std::vector<float> expected = point ? std::vector<float>{7.0f} : std::vector<float>{};
    for (std::size_t i = 0U; i < input.geometries.size(); ++i) {
        const auto& geometry = *input.geometries[i];
        if (point) for (const auto& value : geometry.values) append(expected, golden<T>(value, mode, black));
        else for (std::size_t p = 0U; p < (dommy::viewport::attributeTupleSize(mode) == 4U ? geometry.points : 1U); ++p)
            append(expected, golden<T>(input.values[i], mode, black));
    }
    std::vector<float> output{7.0f}; run(input, output); assert(output == expected);
    if (point) {
        input.geometries[1]->hasAttribute = false;
        expected = {7.0f};
        for (const auto& value : input.geometries[0]->values) append(expected, golden<T>(value, mode, black));
        std::vector<float> missing(dommy::viewport::attributeTupleSize(mode), 0.0f);
        if (missing.size() == 4U) missing[3] = black ? 0.0f : 1.0f;
        for (std::size_t p = 0U; p < input.geometries[1]->points; ++p) append(expected, missing);
        output = {7.0f}; run(input, output); assert(output == expected);
        input.geometries[1]->hasAttribute = true;
        input.geometries[1]->values.push_back({{8, 9, 10}});
        rejectedUnchanged([&](std::vector<float>& out) { run(input, out); });
        input.geometries[1]->values.pop_back();
    } else {
        input.values.push_back({{8, 9, 10}});
        rejectedUnchanged([&](std::vector<float>& out) { run(input, out); });
        input.values.pop_back();
    }
    if (std::is_floating_point<typename dommy::numeric::ValueComponents<T>::Scalar>::value) {
        for (long double bad : {1.0e300L, -1.0e300L,
             std::numeric_limits<long double>::infinity(), std::numeric_limits<long double>::quiet_NaN()}) {
            if (sizeof(typename dommy::numeric::ValueComponents<T>::Scalar) == sizeof(float) && std::isfinite(bad)) continue;
            for (unsigned int axis = 0U; axis < dommy::numeric::ValueComponents<T>::count; ++axis) {
                FakeOwner invalid;
                (point ? invalid.geometries.back()->values.back() : invalid.values.back())[axis] = bad;
                rejectedUnchanged([&](std::vector<float>& out) { run(invalid, out); });
            }
        }
    }
}
#include "viewport_attribute_cases.inc"

// Production point attributes return owned arrays by const reference. A
// derived vector makes accidental "const auto" copies observable here.
static int observedArrayCopies = 0;
struct ObservedValues : std::vector<double> {
    ObservedValues() : std::vector<double>{2.0, 4.0} {}
    ObservedValues(const ObservedValues& other) : std::vector<double>(other) {
        ++observedArrayCopies;
    }
};
struct ReferenceGeometry {
    ObservedValues values;
    std::size_t numberOfPoints() const { return values.size(); }
    bool isExistAttr(const std::string&) const { return true; }
    template <typename T> const ObservedValues& attrListValue(const std::string&) const {
        static_assert(std::is_same<T, double>::value, "reference fixture uses double");
        return values;
    }
};
void readOnlyReferenceTest() {
    std::vector<std::shared_ptr<ReferenceGeometry>> geometries{
        std::make_shared<ReferenceGeometry>(), std::make_shared<ReferenceGeometry>()};
    observedArrayCopies = 0;
    const auto result = dommy::viewport::makePointAttributeValues<double>(
        geometries.begin(), geometries.end(), "attribute", Mode::Scalar);
    assert(result == std::vector<float>({2, 4, 2, 4}) && observedArrayCopies == 0);
    assert(geometries[0]->values[0] == 2 && geometries[1]->values[1] == 4);
}

void budgetAndAllocationTests() {
    const int64_t midpoint = (int64_t(1) << 60) + (int64_t(1) << 36);
    for (int64_t value : {midpoint-1, midpoint, midpoint+1, -midpoint-1, -midpoint, -midpoint+1,
                         std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()}) {
        std::vector<float> result;
        ValueToValueFloat(std::vector<int64_t>{value}, result);
        assert(result == std::vector<float>{static_cast<float>(value)});
        SingleToColorFloat(std::vector<int64_t>{value}, true, result);
        assert(result.size() == 5U);
        for (float component : result) assert(component == static_cast<float>(value));
    }
    const double maximum = std::numeric_limits<float>::max();
    for (double valid : {maximum, -maximum, 0.0, std::numeric_limits<double>::denorm_min()}) {
        std::vector<float> result{7.0f};
        ValueToValueFloat(std::vector<double>{valid}, result);
        assert(result.size() == 1U && std::isfinite(result[0]) && result[0] == static_cast<float>(valid));
    }
    for (double invalid : {std::nextafter(maximum, std::numeric_limits<double>::infinity()),
                           std::nextafter(-maximum, -std::numeric_limits<double>::infinity())})
        rejectedUnchanged([&](std::vector<float>& out) { ValueToValueFloat(std::vector<double>{invalid}, out); });
    rejectedUnchanged([](std::vector<float>&) {
        (void)dommy::viewport::attributeTuple(2.0, Mode::Vector2, false);
    });
    rejectedUnchanged([](std::vector<float>&) {
        (void)dommy::viewport::attributeScalarCount(dommy::viewport::kMaxViewportVertexCount + 1U, 1U);
    });
    rejectedUnchanged([](std::vector<float>&) {
        (void)dommy::viewport::attributeScalarCount(std::numeric_limits<std::size_t>::max(), 4U);
    });
    FakeOwner input;
    input.geometries[1]->points = dommy::viewport::kMaxViewportVertexCount;
    attributeReads = 0U;
    rejectedUnchanged([&](std::vector<float>& out) { setPointSingleValueColor<double>(input, "attribute", false, out); });
    assert(attributeReads == 0U);
    input.geometries[1]->points = 11U; // 13 * 3 float scalars, unique allocation size.
    input.geometries[1]->values.resize(11U, {{2, 3, 4}});
    failAllocationBytes = 13U * 3U * sizeof(float);
    rejectedUnchanged([&](std::vector<float>& out) { setMeshPointSimpleVectorValue<Imath::V3d>(input, "attribute", out); });
    assert(failAllocationBytes == 0U);
    auto vertices = input.beginCurves();
    auto end = input.endCurves();
    rejectedUnchanged([&](std::vector<float>&) {
        (void)dommy::viewport::makePrimitiveAttributeValues(
            std::vector<double>{2, 3}, 3U, vertices, end, Mode::Scalar);
    });
    input.geometries[1].reset();
    rejectedUnchanged([&](std::vector<float>& out) { setPrimSimpleSingleValue<double>(input, "attribute", out); });
}

int main() {
    readOnlyReferenceTest();
    cases1<bool>(); cases1<int32_t>(); cases1<uint32_t>(); cases1<int64_t>(); cases1<float>(); cases1<double>();
    cases2<Imath::V2i>(); cases2<Imath::V2f>(); cases2<Imath::V2d>();
    cases3<Imath::V3i>(); cases3<Imath::V3f>(); cases3<Imath::V3d>();
    budgetAndAllocationTests();
    std::cout << "40 production viewport conversion adapters passed (host data/Imath boundaries shaped).\n";
}
