#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMeshUVTransfer.h"

#include <array>
#include <cassert>
#include <iostream>

namespace {

template <typename T>
struct Point4 {
    T x = 0, y = 0, z = 0, w = 1;
    T operator[](unsigned int axis) const {
        const T values[] = {x, y, z, w};
        return values[axis];
    }
};

template <typename T>
struct Array {
    static int allocationFault;
    static int copyFault;
    std::vector<T> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    T& operator[](unsigned int i) { return values.at(i); }
    const T& operator[](unsigned int i) const { return values.at(i); }
    MStatus setLength(unsigned int count) {
        if (allocationFault == 1) return MS::kFailure;
        values.resize(count && allocationFault == 2 ? count - 1U : count);
        return MS::kSuccess;
    }
    MStatus copy(const Array& source) {
        if (copyFault == 1) return MS::kFailure;
        values = source.values;
        if (copyFault == 2 && !values.empty()) values.pop_back();
        return MS::kSuccess;
    }
};
template <typename T> int Array<T>::allocationFault = 0;
template <typename T> int Array<T>::copyFault = 0;

using FloatPoint = std::array<float, 3>;

template <typename Exception, typename Action>
void expectThrow(Action action) {
    bool caught = false;
    try { action(); } catch (const Exception&) { caught = true; }
    assert(caught);
}

template <typename T>
void testExport() {
    using dommy::geometry::copyPointsToMayaArray;
    const std::vector<FloatPoint> source = {{{1, 2, 3}}, {{-4, 5, 6}}};
    Array<Point4<T>> output;
    output.values.resize(1U);
    output[0].x = T(17);
    for (int fault : {1, 2}) {
        Array<Point4<T>>::allocationFault = fault;
        assert(copyPointsToMayaArray(source, output) != MS::kSuccess);
        assert(output.length() == 1U && output[0].x == T(17));
    }
    Array<Point4<T>>::allocationFault = 0;
    Array<Point4<T>>::copyFault = 1;
    assert(copyPointsToMayaArray(source, output) != MS::kSuccess);
    assert(output.length() == 1U && output[0].x == T(17));
    Array<Point4<T>>::copyFault = 2;
    assert(copyPointsToMayaArray(source, output) != MS::kSuccess);
    Array<Point4<T>>::copyFault = 0;
    assert(copyPointsToMayaArray(source, output) == MS::kSuccess);
    assert(output.length() == 2U);
    for (unsigned int i = 0; i < 2U; ++i) {
        for (unsigned int axis = 0; axis < 3U; ++axis) assert(output[i][axis] == source[i][axis]);
        assert(output[i].w == T(1));
    }
    for (float invalid : {std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()}) {
        std::vector<FloatPoint> bad = source;
        bad[1][2] = invalid;
        assert(copyPointsToMayaArray(bad, output) != MS::kSuccess);
        assert(output.length() == 2U && output[1].z == T(6));
    }
    assert(copyPointsToMayaArray(std::vector<FloatPoint>(), output) == MS::kSuccess);
    assert(output.length() == 0U);
}

void testReplacement() {
    using dommy::geometry::replacePointArray;
    std::vector<FloatPoint> output = {{{17, 18, 19}}};
    const auto original = output;
    Array<Point4<double>> source;
    source.values.resize(2U);
    source[0].x = 7;
    source[1].x = 1e100;
    expectThrow<std::invalid_argument>([&] { replacePointArray(source, source.length(), output); });
    assert(output == original);
    for (double invalid : {-1e100, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        source[1].x = invalid;
        expectThrow<std::invalid_argument>([&] { replacePointArray(source, source.length(), output); });
        assert(output == original);
    }
    source[1].x = std::numeric_limits<float>::max();
    source[1].y = -std::numeric_limits<float>::max();
    source[1].z = std::numeric_limits<float>::denorm_min();
    source[0].w = 2; // Preserve XYZ without dividing by the homogeneous component.
    replacePointArray(source, source.length(), output);
    assert(output.size() == 2U && output[0][0] == 7);
    assert(output[1][0] == std::numeric_limits<float>::max());
    assert(output[1][1] == -std::numeric_limits<float>::max());
    assert(output[1][2] == std::numeric_limits<float>::denorm_min());
    const auto accepted = output;
    replacePointArray(output, output.size(), output); // Self-aliasing input.
    assert(output == accepted);
    expectThrow<std::invalid_argument>([&] {
        replacePointArray(source, dommy::generation::kMaxGeneratedPointCount + 1U, output);
    });
    assert(output == accepted);
    // A staged read exception also leaves the original positions untouched.
    expectThrow<std::out_of_range>([&] { replacePointArray(source, 3U, output); });
    assert(output == accepted);
    std::vector<FloatPoint> bad = accepted;
    bad.back()[2] = std::numeric_limits<float>::quiet_NaN();
    expectThrow<std::invalid_argument>([&] { replacePointArray(bad, bad.size(), output); });
    assert(output == accepted);
    replacePointArray(std::vector<FloatPoint>(), 0U, output);
    assert(output.empty());
}

void testTopologyCopies() {
    Array<int> counts, ids, destination;
    counts.values = {3};
    ids.values = {0, 1, 2};
    for (int fault : {1, 2}) {
        Array<int>::copyFault = fault;
        assert(dommy::maya::copyCheckedArray(ids, destination) != MS::kSuccess);
    }
    Array<int>::copyFault = 0;
    assert(dommy::maya::copyCheckedArray(ids, destination) == MS::kSuccess);
    assert(destination.values == ids.values);

    Array<float> u, v, outputU, outputV;
    Array<int> outputCounts, outputIds;
    u.values = {0, 1, 0};
    v.values = {0, 0, 1};
    for (int fault : {1, 2}) {
        Array<float>::copyFault = fault;
        assert(dommy::mesh::copyMeshUVArrays(u, v, counts, ids,
            outputU, outputV, outputCounts, outputIds) != MS::kSuccess);
    }
    Array<float>::copyFault = 0;
    for (int fault : {1, 2}) {
        Array<int>::copyFault = fault;
        assert(dommy::mesh::copyMeshUVArrays(u, v, counts, ids,
            outputU, outputV, outputCounts, outputIds) != MS::kSuccess);
    }
    Array<int>::copyFault = 0;
    assert(dommy::mesh::copyMeshUVArrays(u, v, counts, ids,
        outputU, outputV, outputCounts, outputIds) == MS::kSuccess);
    assert(dommy::mesh::validFullMeshUVArrays(counts, outputU, outputV, outputCounts, outputIds));
}

} // namespace

void runPointStorageTests() {
    testExport<double>();
    testExport<float>();
    testReplacement();
    testTopologyCopies();
    std::cout << "Point storage and geometry copy boundary tests: OK\n";
}
