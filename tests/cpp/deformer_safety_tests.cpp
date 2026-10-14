#include "common/utility/DDeformerMath.h"
#include "noise/PerlinNoise.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <map>
#include <vector>

namespace MS {
enum Code { kSuccess = 1, kFailure = 0, kInvalidParameter = -1 };
}
struct MStatus {
    int code;
    MStatus(int value = MS::kSuccess) : code(value) {}
    explicit operator bool() const {
        return code == MS::kSuccess;
    }
    bool operator==(const MStatus& other) const {
        return code == other.code;
    }
};
#define CHECK_MSTATUS_AND_RETURN_IT(status)                                                        \
    do {                                                                                           \
        if (!(status))                                                                             \
            return status;                                                                         \
    } while (false)

namespace MSpace {
enum Space { kObject };
}
struct MPoint {
    double x, y, z, w;
    MPoint(double a = 0.0, double b = 0.0, double c = 0.0) : x(a), y(b), z(c), w(1.0) {}
    double& operator[](unsigned int i) {
        assert(i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    double operator[](unsigned int i) const {
        assert(i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    bool operator==(const MPoint& p) const {
        return x == p.x && y == p.y && z == p.z && w == p.w;
    }
};
using MVector = MPoint;
struct MMatrix {
    double entries[4][4] = {};
    const double* operator[](unsigned int i) const {
        return entries[i];
    }
};
using double3 = double[3];
using DT_DROPOFF_TYPE = int;
const int DT_DROPOFF_CONSTANT = 0, DT_DROPOFF_TYPE_COUNT = 3;
const int kMaxNoiseOctaves = noise::kMaxPerlinOctaves;

struct MDataHandle {
    bool valid;
    double value;
    double3 triple = {0.0, 0.0, 0.0};
    MDataHandle(bool ok, double v) : valid(ok), value(v) {}
    float asFloat() const {
        assert(valid);
        return static_cast<float>(value);
    }
    double asDouble() const {
        assert(valid);
        return value;
    }
    int asInt() const {
        assert(valid);
        return static_cast<int>(value);
    }
    short asShort() const {
        assert(valid);
        return static_cast<short>(value);
    }
    bool asBool() const {
        assert(valid);
        return value != 0.0;
    }
    MMatrix asMatrix() const {
        assert(valid);
        return MMatrix();
    }
    const double3& asDouble3() const {
        assert(valid);
        return triple;
    }
};
struct MDataBlock {
    int calls = 0, failAt = -1;
    std::map<int, double> values;
    MDataHandle inputValue(int attribute, MStatus* status) {
        const bool ok = calls++ != failAt;
        *status = ok ? MS::kSuccess : MS::kFailure;
        const auto found = values.find(attribute);
        return MDataHandle(ok, found == values.end() ? 1.0 : found->second);
    }
};

struct MPointArray {
    std::vector<MPoint> values;
    static int failSetAt;
    static bool failAllocation;
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    MStatus setLength(unsigned int count) {
        if (failAllocation)
            return MS::kFailure;
        values.resize(count);
        return MS::kSuccess;
    }
    MStatus set(const MPoint& point, unsigned int index) {
        if (static_cast<int>(index) == failSetAt)
            return MS::kFailure;
        values.at(index) = point;
        return MS::kSuccess;
    }
};
int MPointArray::failSetAt = -1;
bool MPointArray::failAllocation = false;

using MFloatPoint = MPoint;
using MFloatVector = MVector;
using MFloatPointArray = MPointArray;
struct MFloatArray {};
struct FakeMesh {
    bool returnedHit = false;
    MStatus returnedStatus = MS::kSuccess;
    bool allIntersections(const MFloatPoint&, const MFloatVector&, std::nullptr_t, std::nullptr_t,
                          bool, MSpace::Space, int, bool, int*, bool, MFloatPointArray&,
                          MFloatArray*, std::nullptr_t, std::nullptr_t, std::nullptr_t,
                          std::nullptr_t, float, MStatus* status) {
        *status = returnedStatus;
        return returnedHit;
    }
};

// Count calls to every fallible iterator boundary. setAllPositions stores by
// iterator order (IDs can be sparse), as expected by the production wrapper.
struct MItGeometry {
    std::vector<MPoint> points;
    std::vector<int> ids;
    std::size_t cursor = 0U;
    int calls = 0, failAt = -1, commits = 0;
    int reportedCount = -2;
    bool check(MStatus* status = nullptr) {
        const bool ok = calls++ != failAt;
        if (status)
            *status = ok ? MS::kSuccess : MS::kFailure;
        return ok;
    }
    MStatus reset() {
        if (!check())
            return MS::kFailure;
        cursor = 0U;
        return MS::kSuccess;
    }
    int exactCount(MStatus* status) {
        if (!check(status))
            return -1;
        return reportedCount == -2 ? static_cast<int>(points.size()) : reportedCount;
    }
    bool isDone(MStatus* status) {
        return check(status) && cursor == points.size();
    }
    int index(MStatus* status) {
        return check(status) ? ids.at(cursor) : -1;
    }
    MPoint position(MSpace::Space, MStatus* status) {
        return check(status) ? points.at(cursor) : MPoint(99.0);
    }
    MStatus next() {
        if (!check())
            return MS::kFailure;
        ++cursor;
        return MS::kSuccess;
    }
    MStatus setAllPositions(const MPointArray& candidate, MSpace::Space) {
        if (!check())
            return MS::kFailure;
        assert(candidate.values.size() == points.size());
        points = candidate.values;
        ++commits;
        return MS::kSuccess;
    }
};

namespace dommy {
namespace maya {
bool isFinitePoint(const MPoint& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::isfinite(p.w);
}
bool isFiniteMatrix(const MMatrix& matrix) {
    for (unsigned int row = 0U; row < 4U; ++row)
        for (unsigned int column = 0U; column < 4U; ++column)
            if (!std::isfinite(matrix[row][column]))
                return false;
    return true;
}
} // namespace maya
} // namespace dommy

#include "deformer_methods.inc"

void testCollisionWeightsAndSmoothing() {
    using dommy::deformer::smoothCollisionPoints;
    const std::vector<MPoint> original{MPoint(0), MPoint(2), MPoint(4)};
    const std::vector<std::vector<unsigned int>> graph{{1U, 2U}, {0U}, {0U}};
    const std::vector<bool> contained{true, false, false};
    auto points = original;
    assert(smoothCollisionPoints(points, graph, {false, true, true}, contained, 0, 1U));
    assert(points[0] == original[0]); // painted zero: fixed boundary
    points = original;
    assert(smoothCollisionPoints(points, graph, {true, true, true}, contained, 0, 1U));
    assert(points[0].x == 3.0 && points[1].x == 0.0);
    MPoint result;
    assert(dommy::deformer::blendPoint(original[0], points[0], 0.5 * 0.5, result));
    assert(result.x == 0.75); // no squared/repeated envelope or vertex weight
    assert(dommy::deformer::blendPoint(original[0], points[0], 0.0, result));
    assert(result == original[0]);
    points = original;
    assert(smoothCollisionPoints(points, graph, {true, true, true}, contained, 1, 1U));
    assert(points[0].x == 3.0 && points[1].x == 2.0);
    points = original;
    assert(smoothCollisionPoints(points, graph, {true, true, true}, contained, 2, 1U));
    assert(points[0].x == 0.0 && points[1].x == 0.0);
    points = original;
    assert(smoothCollisionPoints(points, graph, {true, true, true}, contained, 0, 2U));
    assert(points[0].x == 0.0 && points[1].x == 3.0); // Jacobi, not in-place order dependent

    const double maximum = std::numeric_limits<double>::max();
    points = {MPoint(0), MPoint(maximum), MPoint(maximum)};
    assert(smoothCollisionPoints(points, graph, {true, false, false}, contained, 0, 1U));
    assert(points[0].x == maximum); // mean is finite even when sum would overflow
    points = original;
    auto invalid = graph;
    invalid[0].push_back(999U);
    assert(!smoothCollisionPoints(points, invalid, {true, true, true}, contained, 0, 1U));
    assert(points == original);
    assert(!smoothCollisionPoints(points, graph, {true}, contained, 0, 1U));
    assert(!smoothCollisionPoints(points, graph, {true, true, true}, contained, 3, 1U));
    assert(!smoothCollisionPoints(points, graph, {true, true, true}, contained, 0,
                                  std::numeric_limits<unsigned int>::max()));
    assert(points == original);
    const std::vector<std::vector<unsigned int>> isolated(3);
    assert(smoothCollisionPoints(points, isolated, {true, true, true}, contained, 0, 1U));
    assert(points == original);
    points.clear();
    assert(smoothCollisionPoints(points, {}, {}, {}, 0, std::numeric_limits<unsigned int>::max()));
    points = original;
    points[2].z = std::numeric_limits<double>::infinity();
    assert(!smoothCollisionPoints(points, graph, {true, true, true}, contained, 0, 1U));
}

void testNumericalBoundaries() {
    using dommy::deformer::offsetPoint;
    MPoint result(99.0);
    const MVector direction(1.0, 0.0, 0.0);
    assert(offsetPoint(MPoint(), direction, 1e308, 2.0, 1.0, 0.01, 1.0, result));
    assert(std::isfinite(result.x) && std::abs(result.x / 2e306 - 1.0) < 1e-14);
    assert(offsetPoint(MPoint(), direction, 1e308, 2.0, 0.0, 1.0, 1.0, result));
    assert(result == MPoint());
    const MPoint unchanged = result;
    assert(!offsetPoint(MPoint(), direction, 1e308, 2.0, 1.0, 1.0, 1.0, result));
    assert(result == unchanged);
    assert(!offsetPoint(MPoint(), MVector(std::numeric_limits<double>::quiet_NaN()), 1.0, 1.0, 0.0,
                        1.0, 1.0, result));
    assert(result == unchanged);
    assert(offsetPoint(MPoint(), direction, 1e-300, 1e-300, 1e300, 1e300, 1.0, result));
    assert(std::abs(result.x - 1.0) < 1e-14);
    const double maximum = std::numeric_limits<double>::max();
    assert(dommy::deformer::blendPoint(MPoint(-maximum), MPoint(maximum), 0.5, result));
    assert(result.x == 0.0);
    MVector normal(maximum, maximum, 0.0);
    assert(dommy::deformer::normalizeVector(normal));
    assert(std::abs(normal.x - std::sqrt(0.5)) < 1e-14);
    normal = MVector();
    assert(dommy::deformer::normalizeVector(normal));
    assert(normal.x == 0.0);
}

void testNoiseBoundaries() {
    const double x = 0.125, y = 0.25, z = 0.375;
    double result = 99.0;
    const noise::PerlinOctave maximum(noise::kMaxPerlinOctaves, 1U);
    assert(maximum.tryNoise(x, y, z, result));
    assert(result == maximum.noise(x, y, z));
    for (int invalid : {-1, 33, 64}) {
        result = 99.0;
        const noise::PerlinOctave value(invalid, 1U);
        assert(!value.tryNoise(x, y, z, result) && result == 99.0);
        assert(value.noise(x, y, z) == 0.0); // compatibility fallback for other callers
    }
    const noise::PerlinOctave valid(1, 1U);
    assert(valid.tryNoise(0.0, 0.0, 0.0, result) && result == 0.0);
    result = 99.0;
    assert(!valid.tryNoise(std::numeric_limits<double>::infinity(), y, z, result));
    assert(result == 99.0);
    assert(!noise::PerlinOctave(2, 1U, 0.5, 1e308).tryNoise(2.0, y, z, result));
    assert(result == 99.0);
    assert(!noise::PerlinOctave(3, 1U, 1e308, 2.0).tryNoise(x, y, z, result));
    assert(result == 99.0);
    assert(noise::PerlinOctave(2, 1U, 1e40, 2.0).tryNoise(x, y, z, result));
    assert(result > std::numeric_limits<float>::max());
    MPoint displaced;
    assert(dommy::deformer::offsetPoint(MPoint(), MVector(1, 0, 0), 1e-40, result, 1.0, 1.0, 1.0,
                                        displaced));
    assert(std::isfinite(displaced.x) && displaced.x > 0.0 && displaced.x < 1.0);
}

MStatus translate(unsigned int index, const MPoint& source, MPoint& target) {
    target = source;
    target.x += index;
    return MS::kSuccess;
}

void testStagedGeometry() {
    MItGeometry iterator;
    iterator.ids = {3, 9};
    iterator.points = {MPoint(1.0), MPoint(2.0)};
    const MItGeometry baseline = iterator;
    assert(dommy::maya::editDeformerPoints(iterator, translate));
    assert(iterator.points[0].x == 4.0 && iterator.points[1].x == 11.0 && iterator.commits == 1);
    const int boundaries = iterator.calls;
    for (int failure = 0; failure < boundaries; ++failure) {
        iterator = baseline;
        iterator.failAt = failure;
        assert(!dommy::maya::editDeformerPoints(iterator, translate));
        assert(iterator.points == baseline.points && iterator.commits == 0);
    }
    iterator = baseline;
    assert(!dommy::maya::editDeformerPoints(
        iterator, [](unsigned int index, const MPoint& source, MPoint& target) -> MStatus {
            if (index == 9U)
                return MS::kFailure;
            target = source;
            target.x += 1.0;
            return MS::kSuccess;
        }));
    assert(iterator.points == baseline.points && iterator.commits == 0);
    for (int failure = 0; failure < 2; ++failure) {
        iterator = baseline;
        MPointArray::failSetAt = failure;
        assert(!dommy::maya::editDeformerPoints(iterator, translate));
        assert(iterator.points == baseline.points && iterator.commits == 0);
    }
    MPointArray::failSetAt = -1;
    iterator = baseline;
    MPointArray::failAllocation = true;
    assert(!dommy::maya::editDeformerPoints(iterator, translate) && iterator.commits == 0);
    assert(iterator.points == baseline.points);
    MPointArray::failAllocation = false;
    for (int count : {-1, 1, 3, 10000001}) {
        iterator = baseline;
        iterator.reportedCount = count;
        assert(!dommy::maya::editDeformerPoints(iterator, translate));
        assert(iterator.points == baseline.points && iterator.commits == 0);
    }
    iterator = baseline;
    assert(!dommy::maya::editDeformerPoints(
        iterator, [](unsigned int, const MPoint&, MPoint& target) -> MStatus {
            target.x = std::numeric_limits<double>::infinity();
            return MS::kSuccess;
        }));
    assert(iterator.points == baseline.points && iterator.commits == 0);
    iterator = baseline;
    iterator.ids[1] = -1;
    assert(!dommy::maya::editDeformerPoints(iterator, translate));
    assert(iterator.points == baseline.points && iterator.commits == 0);
    iterator = baseline;
    iterator.points[1].w = std::numeric_limits<double>::quiet_NaN();
    assert(!dommy::maya::editDeformerPoints(iterator, translate) && iterator.commits == 0);
    iterator = MItGeometry();
    assert(dommy::maya::editDeformerPoints(iterator, translate) && iterator.commits == 0);

    iterator = baseline;
    std::vector<MPoint> collisionPoints(10, MPoint(10.0));
    std::vector<double> weights(10, 0.0);
    weights[9] = 0.5;
    assert(publishCollision(iterator, collisionPoints, weights, 0.5f));
    assert(iterator.points[0] == baseline.points[0]);
    assert(iterator.points[1].x == 4.0); // 2 + (10 - 2) * 0.5 * 0.5
    iterator = baseline;
    assert(publishCollision(iterator, collisionPoints, weights, 0.0f));
    assert(iterator.points == baseline.points);
    iterator = baseline;
    collisionPoints[9].x = std::numeric_limits<double>::infinity();
    assert(!publishCollision(iterator, collisionPoints, weights, 1.0f));
    assert(iterator.points == baseline.points && iterator.commits == 0);
}

void testInputFailures() {
    for (bool reportedHit : {false, true}) {
        FakeMesh mesh;
        mesh.returnedHit = reportedHit;
        bool output = !reportedHit;
        assert(queryCollision(mesh, output) && output == reportedHit);
        mesh.returnedStatus = MS::kFailure;
        output = !reportedHit;
        assert(!queryCollision(mesh, output) && output == !reportedHit);
    }
    for (Reader read : readers) {
        MDataBlock block;
        assert(read(block) && block.calls == 1);
        block = MDataBlock();
        block.failAt = 0;
        assert(!read(block)); // invalid handle access would assert above
    }
    MDataBlock block;
    assert(readNoiseControls(block));
    const int count = block.calls;
    assert(count == 16);
    for (int failed = 0; failed < count; ++failed) {
        block = MDataBlock();
        block.failAt = failed;
        assert(!readNoiseControls(block) && block.calls == failed + 1);
    }
    for (int octave : {0, 33, 64}) {
        block = MDataBlock();
        block.values[frequency] = octave;
        assert(!readNoiseControls(block));
    }
    block = MDataBlock();
    block.values[frequency] = 32;
    assert(readNoiseControls(block));
    block = MDataBlock();
    block.values[envelope] = 0.0;
    block.failAt = 1;
    assert(readNoiseControls(block) && block.calls == 1);
}

int main() {
    testCollisionWeightsAndSmoothing();
    testNumericalBoundaries();
    testNoiseBoundaries();
    testStagedGeometry();
    testInputFailures();
}
