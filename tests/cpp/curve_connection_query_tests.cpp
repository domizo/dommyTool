#include "common/utility/DCurveConnection.h"
#include "common/utility/DCurveReflection.h"
#include "common/utility/DMeshIntersection.h"

#include <cassert>
#include <iostream>

bool operator!(const MStatus& status) { return status != MS::kSuccess; }
#define CHECK_MSTATUS_AND_RETURN_IT(status) do { if (!(status)) return status; } while (false)
namespace MSpace { enum Space { kObject, kWorld }; }

struct MVector {
    std::array<double, 3> values;
    MVector(double x = 0, double y = 0, double z = 0) : values{{x, y, z}} {}
    double& operator[](std::size_t i) { return values.at(i); }
    double operator[](std::size_t i) const { return values.at(i); }
    bool operator==(const MVector& other) const { return values == other.values; }
    MVector operator+(const MVector& other) const { return MVector(values[0]+other[0], values[1]+other[1], values[2]+other[2]); }
    MVector operator*(double scale) const { return MVector(values[0]*scale, values[1]*scale, values[2]*scale); }
    MVector operator-() const { return *this * -1; }
    MVector operator^(const MVector& other) const {
        return MVector(values[1]*other[2]-values[2]*other[1],
            values[2]*other[0]-values[0]*other[2], values[0]*other[1]-values[1]*other[0]);
    }
};
using MPoint = MVector;
using MFloatPoint = MVector;
using MFloatVector = MVector;
using DVec3Data = MVector;

struct MPointArray {
    static int failAppendAt, appends;
    static bool failCopy, failClear;
    std::vector<MPoint> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    MPoint& operator[](unsigned int i) { return values.at(i); }
    const MPoint& operator[](unsigned int i) const { return values.at(i); }
    MStatus append(const MPoint& value) {
        if (++appends == failAppendAt) return MS::kFailure;
        values.push_back(value); return MS::kSuccess;
    }
    MStatus clear() { if (failClear) return MS::kFailure; values.clear(); return MS::kSuccess; }
    MStatus copy(const MPointArray& other) {
        if (failCopy) return MS::kFailure;
        values = other.values; return MS::kSuccess;
    }
    static void reset() { failAppendAt = appends = 0; failCopy = failClear = false; }
};
int MPointArray::failAppendAt = 0;
int MPointArray::appends = 0;
bool MPointArray::failCopy = false;
bool MPointArray::failClear = false;
using MFloatPointArray = MPointArray;
struct FloatArray { MStatus append(float) { return MS::kSuccess; } };
struct Ramp { int calls = 0; bool fail = false; };
namespace dommy { namespace maya {
bool sampleFiniteRamp(Ramp& ramp, float, float& result) {
    ++ramp.calls; result = 0; return !ramp.fail;
}
bool isFloatRepresentablePoint(const MPoint& point) {
    for (unsigned int i = 0; i < 3; ++i) if (!numeric::canRepresentAsFloat(point[i])) return false;
    return true;
}
} }

struct Curve {
    int fail = 0, tangentCalls = 0, normalCalls = 0;
    MVector tangentValue{1,0,0}, normalValue{0,1,0};
    MPoint closestPoint(const MPoint& point, double* param, double, MSpace::Space, MStatus* status) {
        *status = fail == 1 ? MS::kFailure : MS::kSuccess; *param = point[0]; return point;
    }
    double findParamFromLength(double length, MStatus* status) {
        *status = fail == 1 ? MS::kFailure : MS::kSuccess; return length;
    }
    double findLengthFromParam(double param, MStatus* status) {
        *status = fail == 4 ? MS::kFailure : MS::kSuccess; return param;
    }
    MStatus getPointAtParam(double param, MPoint& point, MSpace::Space) {
        point = MPoint(100 + param,0,0); return fail == 5 ? MS::kFailure : MS::kSuccess;
    }
    MVector tangent(double, MSpace::Space, MStatus* status) {
        ++tangentCalls; *status = fail == 2 ? MS::kFailure : MS::kSuccess; return tangentValue;
    }
    MVector normal(double, MSpace::Space, MStatus* status) {
        ++normalCalls; *status = fail == 3 ? MS::kFailure : MS::kSuccess; return normalValue;
    }
};

struct Mesh {
    int allCalls = 0, failAll = 0, closestCalls = 0, cleanups = 0;
    bool failClosest = false, failNormal = false, closestHit = true, malformed = false;
    std::vector<unsigned int> hitCounts{0,0,0};
    double surfaceX = 101;
    MStatus freeCachedIntersectionAccelerator() { ++cleanups; return MS::kSuccess; }
    bool allIntersections(const MPoint&, const MVector&, const void*, const void*, bool,
            MSpace::Space, float, bool, int*, bool, MPointArray& points,
            void*, void*, void*, void*, void*, float, MStatus* status) {
        ++allCalls;
        const unsigned int count = static_cast<std::size_t>(allCalls) <= hitCounts.size()
            ? hitCounts[allCalls - 1U] : 0U;
        points.values.assign(count, MPoint(surfaceX,0,0));
        *status = allCalls == failAll ? MS::kFailure : MS::kSuccess;
        if (malformed && count) points[0][0] = std::numeric_limits<double>::quiet_NaN();
        return count != 0;
    }
    bool closestIntersection(const MPoint&, const MVector&, const void*, const void*, bool,
            MSpace::Space, float, bool, int*, MPoint& point,
            void*, void*, void*, void*, void*, float, MStatus* status) {
        ++closestCalls; point = MPoint(surfaceX,0,0);
        *status = failClosest ? MS::kFailure : MS::kSuccess; return closestHit;
    }
    MStatus getClosestPointAndNormal(const MPoint& source, MPoint& point, MVector& normal,
            MSpace::Space, void*, int*) {
        point = source; normal = MVector(1,0,0); return failNormal ? MS::kFailure : MS::kSuccess;
    }
};

#include "curve_query_methods.inc"

void testNormals()
{
    for (int mode = 0; mode < DC_NORMAL_GEN_TYPE_COUNT; ++mode) {
        Curve curve; Ramp ramp; std::vector<MVector> result;
        assert(normal_samples(mode, curve, ramp, result) == MS::kSuccess);
        assert(result.size() == 2);
        if (mode == DC_NORMAL_FROM_CURVE_TANGENT) {
            assert(result[0] == MVector(1,0,0) && result[1] == result[0]);
            assert(ramp.calls == 0 && curve.normalCalls == 0);
        }
        if (mode == DC_NORMAL_VECTOR || mode == DC_NORMAL_FULL_RANDOM) assert(curve.normalCalls == 0);
    }
    const std::vector<MVector> original{MVector(7,8,9)};
    for (int failure : {1,2,3,4}) {
        Curve curve; curve.fail = failure; Ramp ramp; auto result = original;
        assert(normal_samples(DC_NORMAL_FROM_CURVE, curve, ramp, result) == MS::kFailure);
        assert(result == original);
    }
    Curve curve; curve.fail = 3; Ramp ramp; ramp.fail = true; auto result = original;
    assert(normal_samples(DC_NORMAL_FROM_CURVE_TANGENT, curve, ramp, result) == MS::kSuccess);
    curve.tangentValue = MVector(); result = original;
    assert(normal_samples(DC_NORMAL_FROM_CURVE_TANGENT, curve, ramp, result) == MS::kInvalidParameter);
    assert(result == original);
}

void testConnections()
{
    using namespace dommy::curve;
    using Points = std::vector<MPoint>;
    const auto connect = [](const std::vector<Points>& points, double distance, bool angle,
                            std::vector<CurveChain>& chains) {
        std::vector<ConnectionEndpoints> endpoints(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            endpoints[i].points[0] = points[i].front().values;
            endpoints[i].points[1] = points[i].back().values;
            for (unsigned int e = 0; e < 2; ++e)
                endpoints[i].tangents[e] = {{points[i].back()[0]-points[i].front()[0],0,0}};
        }
        return buildCurveConnectionChains(endpoints, distance, angle, 0.1, chains);
    };
    for (bool reverseA : {false,true}) for (bool reverseB : {false,true}) for (bool swap : {false,true}) {
        std::vector<Points> points{{MPoint(-1,0,0), MPoint(0,0,0)}, {MPoint(0,0,0), MPoint(1,0,0)}};
        if (reverseA) std::reverse(points[0].begin(), points[0].end());
        if (reverseB) std::reverse(points[1].begin(), points[1].end());
        if (swap) std::swap(points[0], points[1]);
        for (bool angle : {false,true}) {
            std::vector<CurveChain> chains;
            assert(connect(points, 0, angle, chains) && chains.size() == 1 && chains[0].size() == 2);
            Points result;
            assert(emitCurveChain(chains[0], [&](std::size_t i) -> const Points& { return points.at(i); },
                [&](const MPoint& point) { result.push_back(point); return true; }));
            assert(result.size() == 3 && result[1][0] == 0);
            assert((result[0][0] == -1 && result[2][0] == 1) || (result[0][0] == 1 && result[2][0] == -1));
        }
    }
    std::vector<CurveChain> chains;
    std::vector<Points> points{{MPoint(-2,0,0),MPoint(-1,0,0)}, {MPoint(1500,0,0),MPoint(1501,0,0)}};
    assert(connect(points, 1501, false, chains) && chains.size() == 1);
    Points result;
    assert(emitCurveChain(chains[0], [&](std::size_t i) -> const Points& { return points.at(i); },
        [&](const MPoint& point) { result.push_back(point); return true; }));
    assert(result.size() == 4); // A real gap is not a duplicate endpoint.
    int emissions = 0;
    assert(!emitCurveChain(chains[0], [&](std::size_t i) -> const Points& { return points.at(i); },
        [&](const MPoint&) { return ++emissions != 2; }));
    assert(emissions == 2);
    assert(connect(points, 1500, false, chains) && chains.size() == 2);
    assert(connect({}, 0, false, chains) && chains.empty());
    assert(connect({points[0]}, 0, false, chains) && chains.size() == 1);
    points = {{MPoint(0,0,0),MPoint(1,0,0)}, {MPoint(1,0,0),MPoint(0,1,0)}, {MPoint(0,1,0),MPoint(0,0,0)}};
    assert(connect(points, 0, false, chains) && chains.size() == 1 && chains[0].size() == 3);
    std::vector<bool> seen(3, false);
    for (const auto& entry : chains[0]) { assert(!seen[entry.index]); seen[entry.index] = true; }
    std::vector<ConnectionEndpoints> excessive(5001);
    assert(!buildCurveConnectionChains(excessive, 0, false, 1, chains));
    assert(chains.size() == 1); // Failure does not replace the previous plan.
    assert(!buildCurveConnectionChains({}, -1, false, 1, chains));
}

void testQueryPublication()
{
    using namespace dommy::maya;
    Mesh mesh; mesh.hitCounts = {1}; mesh.failAll = 1;
    int accelerator = 0;
    MPointArray output; output.values = {MPoint(7,8,9)};
    assert(checkedAllIntersections(mesh, MPoint(), MVector(1,0,0),
        MSpace::kWorld, &accelerator, output) == MS::kFailure);
    assert(output.length() == 1 && output[0] == MPoint(7,8,9));
    bool hit = true; MPoint point(7,8,9);
    mesh = Mesh(); mesh.closestHit = false;
    assert(checkedClosestIntersection(mesh, MPoint(), MVector(1,0,0),
        MSpace::kWorld, &accelerator, hit, point) == MS::kSuccess);
    assert(!hit && point == MPoint(7,8,9));
    mesh.failClosest = true; hit = true;
    assert(checkedClosestIntersection(mesh, MPoint(), MVector(1,0,0),
        MSpace::kWorld, &accelerator, hit, point) == MS::kFailure);
    assert(hit && point == MPoint(7,8,9));
    MVector normal(7,8,9); mesh.failNormal = true;
    assert(checkedClosestPointAndNormal(mesh, MPoint(), MSpace::kWorld,
        &accelerator, point, normal) == MS::kFailure);
    assert(point == MPoint(7,8,9) && normal == MVector(7,8,9));
    bool finished = false; point = MPoint(7,8,9);
    assert(!dommy::curve::evaluateReflectionHit(MPoint(), MPoint(), MVector(1,0,0),
        MPoint(), MVector(1,0,0), -1, 0, 10, 0, finished, point));
    assert(!finished && point == MPoint(7,8,9));
}

void testQueriesAndReflection()
{
    const std::vector<MPoint> original{MPoint(7,8,9)};
    for (int failure : {1,2,5}) {
        Curve curve; curve.fail = failure; Mesh mesh; auto output = original;
        assert(reflect_samples(curve, mesh, false, output) == MS::kFailure && output == original);
    }
    for (bool cut : {false,true}) {
        Curve curve; Mesh mesh; mesh.hitCounts = {0,0,0}; auto output = original;
        assert((cut ? cut_samples(curve, mesh, false, output) : reflect_samples(curve, mesh, false, output)) == MS::kSuccess);
        assert(cut ? output.empty() : output.size() == 3);
        mesh = Mesh(); mesh.hitCounts = {1,1,0,1}; mesh.failAll = 4; output = original;
        // Cut has already staged a complete fitted segment when the later query fails.
        if (cut) assert(cut_samples(curve, mesh, true, output) == MS::kFailure && output == original);
        mesh = Mesh(); mesh.failAll = 2; output = original;
        assert((cut ? cut_samples(curve, mesh, false, output) : reflect_samples(curve, mesh, false, output)) == MS::kFailure);
        assert(output == original);
        for (int failedAppend = 1; failedAppend <= 3; ++failedAppend) {
            MPointArray::reset(); MPointArray::failAppendAt = failedAppend;
            mesh = Mesh(); if (cut) mesh.hitCounts = {1,1,1}; output = original;
            assert((cut ? cut_samples(curve, mesh, false, output) : reflect_samples(curve, mesh, false, output)) == MS::kFailure);
            assert(output == original);
        }
        MPointArray::reset(); MPointArray::failCopy = true; mesh = Mesh(); output = original;
        assert((cut ? cut_samples(curve, mesh, false, output) : reflect_samples(curve, mesh, false, output)) == MS::kFailure);
        assert(output == original); MPointArray::reset();
    }
    Curve curve; Mesh mesh; mesh.hitCounts = {1}; mesh.surfaceX = 90;
    auto output = original;
    assert(reflect_samples(curve, mesh, false, output) == MS::kSuccess);
    assert(output.size() == 1 && output[0] == MPoint(90,0,0) && mesh.allCalls == 1);
    mesh = Mesh(); mesh.hitCounts = {0,0,1}; mesh.surfaceX = 110;
    assert(reflect_samples(curve, mesh, false, output) == MS::kSuccess && output.size() == 3);
    assert(output.back() == MPoint(110,0,0));
    mesh = Mesh(); mesh.hitCounts = {0,1}; mesh.failClosest = true; output = original;
    assert(reflect_samples(curve, mesh, false, output) == MS::kFailure && output == original);
    mesh = Mesh(); mesh.hitCounts = {1}; mesh.failNormal = true; output = original;
    assert(reflect_samples(curve, mesh, false, output) == MS::kFailure && output == original);
    mesh = Mesh(); mesh.hitCounts = {1}; mesh.malformed = true;
    assert(reflect_samples(curve, mesh, false, output) == MS::kInvalidParameter && output == original);
    bool finished = true; MPoint next(7,8,9);
    assert(dommy::curve::evaluateReflectionHit(MPoint(100,0,0), MPoint(100,0,0), MVector(1,0,0),
        MPoint(100,0,0), MVector(1,0,0), 5, 0, 10, 0, finished, next));
    assert(!finished && next == MPoint(95,0,0));
    mesh = Mesh();
    try { dommy::maya::MeshAcceleratorGuard<Mesh> guard(mesh); throw 1; } catch (int) {}
    assert(mesh.cleanups == 1);
    MFloatPoint source(7,8,9); MFloatVector ray(7,8,9);
    assert(!dommy::maya::makeFloatRay(MPoint(std::numeric_limits<double>::max(),0,0), MVector(1,0,0), source, ray));
    assert(source == MPoint(7,8,9) && ray == MVector(7,8,9));
}

int main()
{
    testNormals(); testConnections(); testQueryPublication(); testQueriesAndReflection();
    std::cout << "Curve connection/query executable checks passed\n";
}
