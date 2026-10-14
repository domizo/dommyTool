#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DMayaCurveKnots.h"
#include "DPoint/math/DVectorMath.h"
#include "common/utility/DAttributeLookup.h"

#include <cassert>
#include <climits>
#include <iostream>
#include <type_traits>

namespace {

// API-shaped fault injection, not a model of Maya's allocator implementation.
template <typename T>
struct TestArray {
    static int allocationFault;
    static int copyFault;
    static int allocationCalls, copyCalls, failAllocationAt, failCopyAt;
    std::vector<T> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    T& operator[](unsigned int i) { return values.at(i); }
    const T& operator[](unsigned int i) const { return values.at(i); }
    MStatus setLength(unsigned int count) {
        if (++allocationCalls == failAllocationAt) return MS::kFailure;
        if (allocationFault == 1) return MS::kFailure;
        values.resize(count && allocationFault == 2 ? count - 1U : count);
        return MS::kSuccess;
    }
    MStatus copy(const TestArray& source) {
        if (++copyCalls == failCopyAt) return MS::kFailure;
        if (copyFault == 1) return MS::kFailure;
        values = source.values;
        if (copyFault == 2 && !values.empty()) values.pop_back();
        return MS::kSuccess;
    }
};
template <typename T> int TestArray<T>::allocationFault = 0;
template <typename T> int TestArray<T>::copyFault = 0;
template <typename T> int TestArray<T>::allocationCalls = 0;
template <typename T> int TestArray<T>::copyCalls = 0;
template <typename T> int TestArray<T>::failAllocationAt = 0;
template <typename T> int TestArray<T>::failCopyAt = 0;

struct TestString {
    static bool failSet;
    std::string value;
    const char* asChar() const { return value.c_str(); }
    MStatus set(const char* text) {
        if (failSet) return MS::kFailure;
        value = text;
        return MS::kSuccess;
    }
};
bool TestString::failSet = false;

// Like MStringArray: status-returning set(), but no copy() method.
struct TestStringArray {
    static int failAllocationAt;
    static int allocations;
    static bool failElement;
    std::vector<TestString> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    TestString& operator[](unsigned int i) { return values.at(i); }
    TestString operator[](unsigned int i) const { return values.at(i); }
    MStatus setLength(unsigned int count) {
        if (++allocations == failAllocationAt) return MS::kFailure;
        values.resize(count);
        return MS::kSuccess;
    }
    MStatus set(const TestString& value, unsigned int i) {
        if (failElement) return MS::kFailure;
        values.at(i) = value;
        return MS::kSuccess;
    }
};
int TestStringArray::failAllocationAt = 0;
int TestStringArray::allocations = 0;
bool TestStringArray::failElement = false;

template <typename T>
DPAttributeData::ConstPtr attribute(const std::vector<T>& values) {
    return std::make_shared<TypedDPAttributeData<T> >(values, T());
}

void testNumericAccess() {
    using namespace dommy::attribute;
    TestArray<int> integers;
    integers.values = {17};
    auto source = attribute<std::int64_t>({INT_MIN, INT_MAX});
    assert((copyTypedMayaArray<std::int64_t, int>(source, 2U, integers) == MS::kSuccess));
    assert(integers.values == std::vector<int>({INT_MIN, INT_MAX}));
    const auto original = integers.values;
    assert((copyTypedMayaArray<std::int64_t, int>(source, 1U, integers) != MS::kSuccess));
    assert((copyTypedMayaArray<float, int>(source, 2U, integers) != MS::kSuccess));
    assert((copyTypedMayaArray<int, int>(DPAttributeData::ConstPtr(), 0U, integers) != MS::kSuccess));
    assert((copyTypedMayaArray<std::uint32_t, int>(attribute<std::uint32_t>({UINT_MAX}),
        1U, integers) != MS::kSuccess));
    assert((copyTypedMayaArray<std::int64_t, int>(attribute<std::int64_t>({
        std::numeric_limits<std::int64_t>::max()}), 1U, integers) != MS::kSuccess));
    assert(integers.values == original);
    assert((copyTypedMayaArray<bool, int>(attribute<bool>({true, false}), 2U, integers) == MS::kSuccess));
    assert(integers.values == std::vector<int>({1, 0}));

    TestArray<float> floats;
    floats.values = {17.0f};
    for (double invalid : {1e100, -1e100, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        assert((copyTypedMayaArray<double, float>(attribute<double>({1, invalid}),
            2U, floats) != MS::kSuccess));
        assert(floats.values == std::vector<float>({17.0f}));
    }
    assert((copyTypedMayaArray<double, float>(attribute<double>({}), 0U, floats) == MS::kSuccess));
    assert(floats.values.empty());

    std::vector<Imath::V3f> vectors(1U, Imath::V3f(17, 17, 17));
    assert(!convertTypedArray<Imath::V3d>(attribute<Imath::V3d>({Imath::V3d(1, 2, 1e100)}), 1U, vectors));
    assert(vectors[0][0] == 17 && vectors[0][2] == 17);
    assert(convertTypedArray<Imath::V2d>(attribute<Imath::V2d>({Imath::V2d(2, 3)}), 1U, vectors));
    assert(vectors[0][0] == 2 && vectors[0][1] == 3 && vectors[0][2] == 0);
    // UV-pair callers also accept Vec3, dropping only the already-checked Z.
    std::vector<Imath::V3d> uv;
    assert(convertTypedArray<Imath::V3d>(attribute<Imath::V3d>({Imath::V3d(2, 3, 4)}), 1U, uv));
    assert(uv[0][0] == 2 && uv[0][1] == 3);
    bool threw = false;
    try { requireTypedArray<Imath::V3d>(source, 2U, vectors); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw && vectors[0][0] == 2);
    assert(!validArrayConversionBudget(dommy::generation::kMaxGeneratedPointCount + 1U, 1U));
    assert(!validArrayConversionBudget(1000000U, sizeof(MMatrix)));
}

void testMayaFailures() {
    using namespace dommy::attribute;
    TestArray<double> values;
    values.values = {17};
    const auto source = attribute<double>({1, 2});
    for (int fault : {1, 2}) {
        TestArray<double>::allocationFault = fault;
        assert((copyTypedMayaArray<double, double>(source, 2U, values) != MS::kSuccess));
        assert(values.values == std::vector<double>({17}));
    }
    TestArray<double>::allocationFault = 0;
    TestArray<double>::copyFault = 1;
    assert((copyTypedMayaArray<double, double>(source, 2U, values) != MS::kSuccess));
    assert(values.values == std::vector<double>({17}));
    TestArray<double>::copyFault = 2;
    assert((copyTypedMayaArray<double, double>(source, 2U, values) != MS::kSuccess));
    // A failed/short final copy can modify output; only failure is promised.
    TestArray<double>::copyFault = 0;
    assert((copyTypedMayaArray<double, double>(source, 2U, values) == MS::kSuccess));
    assert(values.values == std::vector<double>({1, 2}));

    const std::vector<double> prefix = {4, 5};
    const auto identity = [](double value) { return value; };
    values.values = {17, 18, 19};
    assert(copyMayaArrayPrefix(prefix, identity, values) == MS::kSuccess);
    assert(values.values == std::vector<double>({4, 5, 19}));
    values.values = {17};
    assert(copyMayaArrayPrefix(prefix, identity, values) == MS::kSuccess);
    assert(values.values == std::vector<double>({4}));
    values.values.clear();
    assert(copyMayaArrayPrefix(prefix, identity, values) == MS::kSuccess);
    assert(values.values == prefix);
    assert(copyMayaArrayPrefix(std::vector<double>(), identity, values) == MS::kSuccess);
    assert(values.values == prefix);
}

void testNonNumericAccess() {
    using namespace dommy::attribute;
    TestArray<std::string> strings;
    strings.values = {"original"};
    assert((copyTypedMayaArray<std::string, std::string>(
        attribute<std::string>({std::string("a\0b", 3U)}), 1U, strings) != MS::kSuccess));
    assert(strings.values[0] == "original");
    assert((copyTypedMayaArray<std::string, std::string>(
        attribute<std::string>({"", "hello"}), 2U, strings) == MS::kSuccess));
    assert(strings.values == std::vector<std::string>({"", "hello"}));

    TestArray<MMatrix> matrices;
    MMatrix matrix;
    matrix[2][3] = 7;
    assert((copyTypedMayaArray<MMatrix, MMatrix>(attribute<MMatrix>({matrix}), 1U, matrices) == MS::kSuccess));
    assert(matrices[0][2][3] == 7);
    matrix[2][3] = std::numeric_limits<double>::quiet_NaN();
    assert((copyTypedMayaArray<MMatrix, MMatrix>(attribute<MMatrix>({matrix}), 1U, matrices) != MS::kSuccess));
    assert(matrices[0][2][3] == 7);
}

void testKnots() {
    using dommy::maya::buildCurveKnots;
    TestArray<double> knots;
    assert(buildCurveKnots(4U, 3, false, knots) == MS::kSuccess);
    assert(knots.values == std::vector<double>({0, 0, 0, 1, 1, 1}));
    assert(buildCurveKnots(6U, 3, false, knots) == MS::kSuccess);
    assert(knots.values == std::vector<double>({0, 0, 0, 1, 2, 3, 3, 3}));
    assert(buildCurveKnots(4U, 1, false, knots) == MS::kSuccess);
    assert(knots.values == std::vector<double>({0, 1, 2, 3}));
    assert(buildCurveKnots(4U, 3, true, knots) == MS::kSuccess);
    assert(knots.values == std::vector<double>({0, 1, 2, 3, 4, 5}));
    for (int fault : {1, 2}) {
        TestArray<double>::allocationFault = fault;
        assert(buildCurveKnots(4U, 3, false, knots) != MS::kSuccess);
        assert(knots.length() == 6U && knots[5] == 5);
    }
    TestArray<double>::allocationFault = 0;
    TestArray<double>::copyFault = 1;
    assert(buildCurveKnots(4U, 3, false, knots) != MS::kSuccess);
    TestArray<double>::copyFault = 0;
    assert(buildCurveKnots(3U, 3, false, knots) == MS::kSuccess && knots.length() == 0U);
    assert(buildCurveKnots(std::numeric_limits<std::size_t>::max(), 3, false, knots) == MS::kSuccess);
    assert(knots.length() == 0U);
}

void testStringStatusAccess() {
    using dommy::attribute::copyStringMayaArray;
    TestStringArray output;
    output.values.resize(3U);
    for (auto& value : output.values) value.value = "original";
    const auto input = attribute<std::string>({"hello", "world"});
    TestString::failSet = true;
    assert(copyStringMayaArray(input, 2U, output) != MS::kSuccess);
    assert(output.length() == 3U && output[0].value == "original");
    TestString::failSet = false;
    for (int failAt : {1, 2}) {
        TestStringArray::allocations = 0;
        TestStringArray::failAllocationAt = failAt;
        assert(copyStringMayaArray(input, 2U, output) != MS::kSuccess);
        assert(output.length() == 3U && output[0].value == "original");
    }
    TestStringArray::failAllocationAt = 0;
    TestStringArray::failElement = true;
    assert(copyStringMayaArray(input, 2U, output) != MS::kSuccess);
    TestStringArray::failElement = false;
    output.values.resize(3U);
    output[2].value = "tail";
    assert(copyStringMayaArray(input, 2U, output, true) == MS::kSuccess);
    assert(output.length() == 3U && output[0].value == "hello" && output[2].value == "tail");
    assert(copyStringMayaArray(input, 2U, output) == MS::kSuccess);
    assert(output.length() == 2U && output[1].value == "world");
    assert(dommy::maya::copyCheckedArray(output, output) == MS::kSuccess);
    assert(copyStringMayaArray(attribute<std::string>({}), 0U, output) == MS::kSuccess);
    assert(output.length() == 0U);
}

void testCurveWeights() {
    using dommy::attribute::convertWeightAttribute;
    std::vector<double> values{17.0};
    assert(convertWeightAttribute(attribute<bool>({true, false}), 2U, values));
    assert(values == std::vector<double>({1.0, 0.0}));
    assert(convertWeightAttribute(attribute<double>({16777217.25, -2.5}), 2U, values));
    assert(values == std::vector<double>({16777217.25, -2.5}));
    assert(convertWeightAttribute(attribute<std::uint32_t>({UINT_MAX}), 1U, values));
    assert(values[0] == double(UINT_MAX));
    assert(convertWeightAttribute(attribute<std::int64_t>({std::numeric_limits<std::int64_t>::max()}), 1U, values));
    assert(convertWeightAttribute(attribute<Imath::V2d>({Imath::V2d(2.5, 99)}), 1U, values));
    assert(values[0] == 2.5);
    assert(convertWeightAttribute(attribute<Imath::V3f>({Imath::V3f(-3.5f, 99, 100)}), 1U, values));
    assert(values[0] == -3.5);
    const auto before = values;
    assert(!convertWeightAttribute(attribute<std::string>({"weight"}), 1U, values));
    assert(!convertWeightAttribute(attribute<MMatrix>({MMatrix()}), 1U, values));
    assert(!convertWeightAttribute(DPAttributeData::ConstPtr(), 1U, values));
    assert(!convertWeightAttribute(attribute<float>({1.0f}), 2U, values));
    assert(!convertWeightAttribute(attribute<double>({std::numeric_limits<double>::infinity()}), 1U, values));
    assert(!convertWeightAttribute(attribute<Imath::V3d>({Imath::V3d(std::numeric_limits<double>::quiet_NaN(), 0, 0)}), 1U, values));
    assert(values == before);
    assert(convertWeightAttribute(attribute<double>({}), 0U, values) && values.empty());
}

void testCurveUvArrays() {
    using dommy::attribute::copyTypedMayaUvArrays;
    using Array = TestArray<double>;
    auto input = attribute<Imath::V2d>({Imath::V2d(16777217.25, 2), Imath::V2d(3, 4)});
    Array first, second;
    first.values = {17}; second.values = {19};
    for (int failure = 1; failure <= 2; ++failure) {
        Array::allocationCalls = Array::copyCalls = 0;
        Array::failAllocationAt = failure;
        assert(copyTypedMayaUvArrays<Imath::V2d>(input, 2U, first, second) == MS::kFailure);
        assert(first.values[0] == 17 && second.values[0] == 19);
    }
    Array::failAllocationAt = 0;
    for (int failure = 1; failure <= 4; ++failure) {
        Array::allocationCalls = Array::copyCalls = 0;
        Array::failCopyAt = failure;
        first.values = {17}; second.values = {19};
        assert(copyTypedMayaUvArrays<Imath::V2d>(input, 2U, first, second) == MS::kFailure);
        if (failure <= 3) assert(first.values[0] == 17 && second.values[0] == 19);
    }
    Array::failCopyAt = 0;
    assert(copyTypedMayaUvArrays<Imath::V2d>(input, 2U, first, second) == MS::kSuccess);
    assert(first.values == std::vector<double>({16777217.25, 3}));
    assert(second.values == std::vector<double>({2, 4}));
    const auto originalFirst = first.values, originalSecond = second.values;
    assert(copyTypedMayaUvArrays<Imath::V2d>(input, 1U, first, second) != MS::kSuccess);
    assert(copyTypedMayaUvArrays<Imath::V2d>(input, 2U, first, first) != MS::kSuccess);
    assert(copyTypedMayaUvArrays<Imath::V3d>(attribute<Imath::V3d>({
        Imath::V3d(1, 2, std::numeric_limits<double>::infinity())}), 1U, first, second) != MS::kSuccess);
    assert(first.values == originalFirst && second.values == originalSecond);
    assert(copyTypedMayaUvArrays<Imath::V3d>(attribute<Imath::V3d>({Imath::V3d(1, 2, 3)}),
        1U, first, second) == MS::kSuccess);
    assert(first.values[0] == 1 && second.values[0] == 2);
    assert(copyTypedMayaUvArrays<Imath::V2d>(attribute<Imath::V2d>({}),
        0U, first, second) == MS::kSuccess && first.values.empty() && second.values.empty());
}

void testCurveOffsetVectors() {
    using dommy::attribute::convertVector3Attribute;
    std::vector<Imath::V3d> values;
    assert(convertVector3Attribute(attribute<Imath::V3i>({Imath::V3i(1, -2, 3)}), 1U, values));
    assert(values[0][0] == 1 && values[0][1] == -2 && values[0][2] == 3);
    assert(convertVector3Attribute(attribute<Imath::V3f>({Imath::V3f(0.5f, 2, 3)}), 1U, values));
    assert(values[0][0] == 0.5 && values[0][1] == 2 && values[0][2] == 3);
    assert(convertVector3Attribute(attribute<Imath::V3d>({Imath::V3d(16777217.25, 2, 3)}), 1U, values));
    assert(values[0][0] == 16777217.25);
    const auto before = values;
    assert(!convertVector3Attribute(DPAttributeData::ConstPtr(), 1U, values));
    assert(!convertVector3Attribute(attribute<double>({1.0}), 1U, values));
    assert(!convertVector3Attribute(attribute<Imath::V2d>({Imath::V2d(1, 2)}), 1U, values));
    assert(!convertVector3Attribute(attribute<Imath::V3d>({Imath::V3d(1, 2, 3)}), 2U, values));
    assert(!convertVector3Attribute(attribute<Imath::V3d>({
        Imath::V3d(1, std::numeric_limits<double>::quiet_NaN(), 3)}), 1U, values));
    assert(values.size() == before.size());
    for (unsigned int axis = 0U; axis < 3U; ++axis) assert(values[0][axis] == before[0][axis]);
    assert(convertVector3Attribute(attribute<Imath::V3d>({}), 0U, values) && values.empty());
}

void testFurAndRampBoundaries() {
    const double maximum = std::numeric_limits<double>::max();
    double x = 0, y = 0, z = 0;
    assert(dommy::math::normalized3OrZero(maximum, maximum, maximum, x, y, z, 1e-12));
    assert(std::abs(x - 1 / std::sqrt(3.0)) < 1e-15 && x == y && y == z);
    assert(!dommy::math::normalized3OrZero(1e-12, 0.0, 0.0, x, y, z, 1e-12));
    assert(!dommy::math::normalized3OrZero(0.5e-12, 0.5e-12, 0.5e-12, x, y, z, 1e-12));
    assert(dommy::math::normalized3OrZero(0.8e-12, 0.8e-12, 0.8e-12, x, y, z, 1e-12));
    assert(!dommy::math::normalized3OrZero(0.0, 0.0, 0.0, x, y, z, 1e-12));
    assert(!dommy::math::normalized3OrZero(maximum, std::numeric_limits<double>::infinity(),
        0.0, x, y, z, 1e-12));
    float result = 17;
    const double floatMaximum = std::numeric_limits<float>::max();
    assert(!dommy::numeric::checkedFloatCast(0.0 + floatMaximum * 2.0, result));
    assert(result == 17);
    assert(dommy::numeric::checkedFloatCast(0.0 + floatMaximum * 1.0, result));
    assert(result == std::numeric_limits<float>::max());
}

} // namespace

namespace {
template <typename T> bool sameAttributeValue(const T& a, const T& b) { return a == b; }
template <typename T>
bool sameAttributeValue(const Imath::Vec2<T>& a, const Imath::Vec2<T>& b) {
    return a[0] == b[0] && a[1] == b[1];
}
template <typename T>
bool sameAttributeValue(const Imath::Vec3<T>& a, const Imath::Vec3<T>& b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}
bool sameAttributeValue(const MMatrix& a, const MMatrix& b) {
    for (size_t row = 0; row != 4; ++row)
        for (size_t column = 0; column != 4; ++column)
            if (a[row][column] != b[row][column]) return false;
    return true;
}

template <typename T>
void testOwnedAttributeType(DP_ATTR_DATA_TYPE tag, const char* name,
                            const T& zero, const T& sample) {
    using Field = TypedDPAttributeData<T>;
    static_assert(std::is_same<decltype(std::declval<const Field&>().defaultValue()),
                               const T&>::value, "Borrowed default API changed");
    static_assert(std::is_same<decltype(std::declval<const Field&>().getDefaultValue()),
                               T>::value, "By-value default API changed");
    static_assert(std::is_same<decltype(std::declval<const Field&>().value()),
                               const std::vector<T>&>::value, "Borrowed array API changed");
    static_assert(!std::is_copy_assignable<Field>::value, "Base copy-assignment boundary changed");
    Field empty;
    assert(empty.attrSize() == 0 && empty.getAttrType() == tag);
    assert(sameAttributeValue(empty.getDefaultValue(), zero));
    assert(sameAttributeValue(typeDefaultVal<T>(), zero));
    assert(std::string(typeNameAsString<T>()) == name && empty.typeName() == name);
    const auto factory = std::dynamic_pointer_cast<Field>(Field::createDPAttribute());
    assert(factory && factory->str() == std::string(name) + "[0]");
    Field defaultOnly(sample);
    assert(defaultOnly.value().empty() && sameAttributeValue(defaultOnly.defaultValue(), sample));
    Field repeated(sample, zero, 3U);
    assert(repeated.numberOfAttr() == 3U && sameAttributeValue(repeated.getValue(2), sample));
    const std::vector<T> rows{sample, zero, sample};
    Field field(rows, sample);
    Field implicitDefault(rows);
    assert(sameAttributeValue(implicitDefault.defaultValue(), zero));
    assert(field.str() == std::string(name) + "[3]");
    auto clone = std::dynamic_pointer_cast<Field>(field.copy());
    assert(clone && clone.get() != &field && &clone->value() != &field.value());
    assert(sameAttributeValue(clone->defaultValue(), sample));
    assert(clone->setValueAt(0, zero) && sameAttributeValue(field.getValue(0), sample));
    assert(!field.replaceValues(rows, 2U) && !field.setValueAt(3U, sample));
    bool rejected = false;
    try { (void)field.getValue(3U); } catch (const std::out_of_range&) { rejected = true; }
    assert(rejected);
    assert(!field.cropArrayFromIds(DPIdList{true}));
    assert(!field.sortArrayFromIds(DPIndexList{0, 1, 3}));
    assert(!field.remapFromIds(DPIndexList{3}));
    assert(field.value().size() == 3U && sameAttributeValue(field.getValue(1), zero));
    assert(field.sortArrayFromIds(DPIndexList{2, 0, 1}));
    assert(sameAttributeValue(field.getValue(0), sample) && sameAttributeValue(field.getValue(2), zero));
    assert(field.remapFromIds(DPIndexList{2, 2, 0, 0}));
    assert(field.attrSize() == 4 && sameAttributeValue(field.getValue(1), zero));
    assert(field.deleteArrayFromIds(DPIdList{true, false, true, false}));
    assert(field.attrSize() == 2 && sameAttributeValue(field.getValue(0), zero));
    assert(field.cropArrayFromIds(DPIdList{false, true}));
    assert(field.attrSize() == 1 && sameAttributeValue(field.getValue(0), sample));
    field.init(rows, zero);
    field.insertDefaultValue(2, 1);
    assert(field.attrSize() == 5 && sameAttributeValue(field.getValue(1), zero));
    field.insertDefaultValue(1, 99);
    assert(field.attrSize() == 5);
    field.incrementIndexElement(0);
    field.incrementIndexElement(99);
    assert(sameAttributeValue(field.getValue(5), sample) && sameAttributeValue(field.getValue(6), zero));
    field.expandAttrSize(1, true);
    field.incrementDefaultValue(1);
    assert(field.attrSize() == 9 && sameAttributeValue(field.getValue(0), zero));
    field.init(rows, sample);
    field.generateDefaultVal();
    assert(sameAttributeValue(field.getDefaultValue(), zero));
    assert(field.replaceValues(field.value(), 3)); // Aliased replacement.
    assert(field.appendValues(field)); // Aliased append must not use invalid vector ranges.
    assert(field.attrSize() == 6 && sameAttributeValue(field.getValue(3), sample));
    auto shared = std::make_shared<Field>(rows, sample);
    shared->appendOtherAttr(shared);
    shared->appendOtherAttr(typename Field::ConstPtr());
    assert(shared->attrSize() == 6);
    Field copied;
    copied.copy(field);
    copied.copy(copied);
    assert(copied.attrSize() == 6 && sameAttributeValue(copied.defaultValue(), zero));
    DPAttrList fields;
    fields["my field"] = shared;
    using dommy::attribute::findTypedAttribute;
    assert(findTypedAttribute<Field>(fields, "my field") == shared);
    assert(findTypedAttribute<const Field>(fields, "my field") == shared);
    assert(!findTypedAttribute<Field>(fields, "myfield"));
    field.setAllValue(sample);
    for (size_t i = 0; i != field.attrSize(); ++i) assert(sameAttributeValue(field.getValue(i), sample));
    field.init(std::vector<T>());
    assert(field.attrSize() == 0 && sameAttributeValue(field.defaultValue(), zero));
}

void testOwnedAttributeArrays() {
    testOwnedAttributeType<bool>(DP_ATTR_BOOL, "bool", false, true);
    testOwnedAttributeType<int32_t>(DP_ATTR_INT, "int", 0, -17);
    testOwnedAttributeType<uint32_t>(DP_ATTR_UINT32, "uint32", 0, 4000000000U);
    testOwnedAttributeType<int64_t>(DP_ATTR_INT64, "int64", 0, INT64_MAX);
    testOwnedAttributeType<float>(DP_ATTR_FLOAT, "float", 0, 1.25f);
    testOwnedAttributeType<double>(DP_ATTR_DOUBLE, "double", 0, 1.25);
    testOwnedAttributeType<Imath::V2i>(DP_ATTR_VEC2I, "vec2i", {0, 0}, {2, -3});
    testOwnedAttributeType<Imath::V2f>(DP_ATTR_VEC2F, "vec2f", {0, 0}, {2.5f, -3});
    testOwnedAttributeType<Imath::V2d>(DP_ATTR_VEC2D, "vec2d", {0, 0}, {2.5, -3});
    testOwnedAttributeType<Imath::V3i>(DP_ATTR_VEC3I, "vec3i", {0, 0, 0}, {2, -3, 4});
    testOwnedAttributeType<Imath::V3f>(DP_ATTR_VEC3F, "vec3f", {0, 0, 0}, {2.5f, -3, 4});
    testOwnedAttributeType<Imath::V3d>(DP_ATTR_VEC3D, "vec3d", {0, 0, 0}, {2.5, -3, 4});
    testOwnedAttributeType<std::string>(DP_ATTR_STRING, "string", {}, std::string(100, 'x'));
    MMatrix transformed; transformed[0][3] = 7;
    testOwnedAttributeType<MMatrix>(DP_ATTR_MATRIX, "matrix", MMatrix(), transformed);
    testOwnedAttributeType<DIntArray>(DP_ATTR_INT_ARRAY, "intarray", {}, {1, -2, 3});
    struct Mislabelled : DPIntAttr {
        std::string typeName() const override { return DPFloatAttr::staticTypeName(); }
    };
    DPAttrList fields;
    fields["fake"] = std::make_shared<Mislabelled>();
    assert(!dommy::attribute::findTypedAttribute<DPFloatAttr>(fields, "fake"));
    DPIntAttr integers({1, 2}, 3);
    DPFloatAttr floats({4, 5}, 6);
    integers.copy(floats);
    assert(integers.value() == std::vector<int32_t>({1, 2}) && !integers.appendValues(floats));
}
} // namespace

void testAttributeExpansionBudget() {
    using namespace dommy::attribute;
    DPAttrList attrs;
    attrs["value"] = std::make_shared<TypedDPAttributeData<int> >(std::vector<int>{1,2},3);
    DPIndexList indices{1,0,1};
    ValidationBudget valid;
    assert(validateAttributeExpansion(attrs,2,3,&indices,valid));
    assert(valid.workItems==5);
    ValidationBudget defaults;
    assert(validateAttributeExpansion(attrs,2,3,nullptr,defaults));
    ValidationBudget invalid;
    assert(!validateAttributeExpansion(attrs,2,2,&indices,invalid));
    indices={2}; invalid=ValidationBudget();
    assert(!validateAttributeExpansion(attrs,2,1,&indices,invalid));
    invalid=ValidationBudget();
    assert(!validateAttributeExpansion(attrs,2,dommy::generation::kMaxWorkItemCount,nullptr,invalid));
    attrs.clear();
    attrs["text"] = std::make_shared<TypedDPAttributeData<std::string> >(
        std::vector<std::string>{"abc","d"},std::string(1024,'x'));
    indices={0,0,1}; valid=ValidationBudget();
    assert(validateAttributeExpansion(attrs,2,3,&indices,valid));
    assert(valid.stringBytes==4+1024+4+7); // name, default, existing values, appended values
    invalid=ValidationBudget();
    assert(!validateAttributeExpansion(attrs,2,dommy::generation::kMaxGeneratedStringByteCount/1024,
        nullptr,invalid));
    attrs.clear();
    attrs["nested"] = std::make_shared<TypedDPAttributeData<DIntArray> >(
        std::vector<DIntArray>{DIntArray(3,1)},DIntArray(10,2));
    indices={0,0}; valid=ValidationBudget();
    assert(validateAttributeExpansion(attrs,1,2,&indices,valid));
    invalid=ValidationBudget();
    assert(!validateAttributeExpansion(attrs,1,dommy::generation::kMaxWorkItemCount/10,nullptr,invalid));
}

void runAttributeAccessTests() {
    testOwnedAttributeArrays();
#ifndef DOMMY_ATTRIBUTE_API_ONLY
    testAttributeExpansionBudget();
    testCurveWeights();
    testCurveUvArrays();
    testCurveOffsetVectors();
    testNumericAccess();
    testMayaFailures();
    testNonNumericAccess();
    testStringStatusAccess();
    testKnots();
    testFurAndRampBoundaries();
#endif
    std::cout << "Attribute access and fur generation boundary tests: OK\n";
}

#ifdef DOMMY_ATTRIBUTE_API_ONLY
int main() { runAttributeAccessTests(); }
#endif
