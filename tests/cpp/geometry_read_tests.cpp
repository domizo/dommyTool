#include "mesh_emitter_doubles.h"
#include "math/DBounds.h"
#include "math/DRandomGenerator.h"
#include <functional>
#include <limits>

// Reuse the existing API failure/array fixtures, extending only missing read
// operations. Scalar counts can be huge without allocating huge test arrays.
int surfaceU = 2, surfaceV = 2;
bool negativeCurveCount = false, negativeFaceCount = false;
std::vector<int> declaredTriangles;
std::vector<double> faceAreas{2.0};
struct ReadHandle : MDataHandle {
    ReadHandle(MDataHandle value) : MDataHandle(value) {}
    MObject asNurbsCurve() const { return data(); }
    MObject asNurbsSurface() const { return data(); }
    MObject asMesh() const { return data(); }
};
struct ReadBlock : MDataBlock {
    ReadHandle inputValue(MObject attr, MStatus* s) {
        return MDataBlock::inputValue(attr, s);
    }
};
struct ReadCurve : MFnNurbsCurve {
    using MFnNurbsCurve::MFnNurbsCurve;
    int numCVs(MStatus* s) const {
        const int count = MFnNurbsCurve::numCVs(s);
        return negativeCurveCount ? -1 : count;
    }
};
struct ReadSurface {
    MObject object;
    ReadSurface(MObject o, MStatus* s) : object(o) { *s = api("surface.bind"); }
    int numCVsInU(MStatus* s) const { *s = api("surface.uCount"); return surfaceU; }
    int numCVsInV(MStatus* s) const { *s = api("surface.vCount"); return surfaceV; }
    MStatus getCVs(MPointArray& points, MSpace::Space space) const {
        assert(space == MSpace::kObject);
        const int ordinal = callCount;
        const MStatus status = api("surface.cvs");
        if (status) {
            points = geometries.at(object.name).points;
            if (ordinal == shortAt && !points.empty()) {
                points.pop_back();
                shortApplied = true;
            }
        }
        return status;
    }
};
struct ReadMesh : MFnMesh {
    using MFnMesh::MFnMesh;
    int numPolygons(MStatus* s) const {
        const int count = MFnMesh::numPolygons(s);
        return negativeFaceCount ? -1 : count;
    }
};
struct ReadPolygon : MItMeshPolygon {
    using MItMeshPolygon::MItMeshPolygon;
    MStatus numTriangles(int& count) const {
        const auto status = api("face.triangleCount");
        if (status)
            count = declaredTriangles.empty() ? mesh().triangleCounts.at(ordinal)
                                             : declaredTriangles.at(ordinal);
        return status;
    }
    MStatus getArea(double& area, MSpace::Space space) const {
        assert(space == MSpace::kWorld);
        const auto status = api("face.area");
        if (status) area = faceAreas.at(ordinal);
        return status;
    }
};
struct MBoundingBox {
    MPoint minimum, maximum;
    MBoundingBox(MPoint a = {}, MPoint b = {}) : minimum(a), maximum(b) {}
};
struct ReadPlug : MPlug {
    using MPlug::MPlug;
    bool isNull() const { return inputs.at(attribute).object.isNull(); }
};
MObject thisMObject() { return {"node"}; }
using DReal = float;
enum { DP_ANIM_EMIT_MATRIX, DP_ANIM_EMIT_POINT, DP_ANIM_EMIT_RANDOM };
MStatus finishOutput(ReadBlock&, const MPlug&, const MObject&, const MPointArray&) {
    return api("empty.output");
}
namespace dommy { namespace packing {
bool isFinite(const MPoint& point) { return dommy::maya::isFinitePoint(point); }
} }

#define MDataHandle ReadHandle
#define MDataBlock ReadBlock
#define MFnNurbsCurve ReadCurve
#define MFnNurbsSurface ReadSurface
#define MFnMesh ReadMesh
#define MItMeshPolygon ReadPolygon
#define MPlug ReadPlug
#include "geometry_read.inc"
#undef MPlug
#undef MItMeshPolygon
#undef MFnMesh
#undef MFnNurbsSurface
#undef MFnNurbsCurve
#undef MDataBlock
#undef MDataHandle

void fixture(MFnData::Type type = MFnData::kMesh) {
    reset();
    injectedStatus = MS::kFailure;
    geometries.clear();
    emitterMeshes.clear();
    inputs.clear();
    geometries["source"] = Geometry();
    emitterMeshes["source"] = EmitterMesh();
    inputs["geometry"].type = type;
    surfaceU = surfaceV = 2;
    negativeCurveCount = negativeFaceCount = invalidPointCount = false;
    declaredTriangles.clear();
    faceAreas = {2.0};
    meshSpaces.clear();
}
void animationConnections() {
    const auto setup=[]() {fixture();inputs["matrix"].connected=true;};
    ReadBlock block;
    setup();bool admitted=false;
    assert(readAnimationConnection(block,admitted)==MS::kSuccess && admitted);
    const int count=callCount;
    for(int index=0;index<count;++index) {
        for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            setup();failAt=index;injectedStatus=code;admitted=false;
            assert(readAnimationConnection(block,admitted)==code && !admitted);
            assert(callCount==index+1);
            assert(std::find(operations.begin(),operations.end(),"empty.output")==operations.end());
        }
        setup();throwAt=index;admitted=false;
        assert(readAnimationConnection(block,admitted)==MS::kFailure && !admitted);
        setup();assert(readAnimationConnection(block,admitted)==MS::kSuccess && admitted);
    }
    for(bool nullPlug:{false,true}) {
        setup();inputs["matrix"].connected=false;
        if(nullPlug)inputs["matrix"].object=MObject::kNullObj;
        admitted=false;
        assert(readAnimationConnection(block,admitted)==MS::kSuccess && !admitted);
        assert(operations.back()=="empty.output");
        assert(callCount==(nullPlug?1:2));
    }
}
bool called(const std::string& name) {
    return std::find(operations.begin(), operations.end(), name) != operations.end();
}
MStatus animation(MPointArray& result, int mode = DP_ANIM_EMIT_POINT) {
    ReadBlock block;
    return readAnimationGeometry(block, mode, result);
}
MStatus information(MBoundingBox& box, MObject& object) {
    ReadBlock block;
    return getMeshInformation({"geometry"}, block, box, object);
}
MStatus area(double& result, size_t count = 1U) {
    ReadBlock block;
    return readScatterArea(block, count, result);
}
int failures(const std::function<MStatus()>& body, MFnData::Type type = MFnData::kMesh) {
    fixture(type);
    assert(body() == MS::kSuccess);
    const int calls = callCount;
    for (const auto code : {MS::kFailure, MS::kInvalidParameter}) {
        for (int i = 0; i < calls; ++i) {
            fixture(type);
            injectedStatus = code;
            reset(i);
            const MStatus actual = body();
            if (actual != code) {
                std::cerr << "Failure not propagated at " << i << " (" << int(code) << ")\n";
                for (const auto& entry : operations) std::cerr << entry << '\n';
            }
            assert(actual == code);
            assert(callCount == i + 1);
        }
    }
    return calls * 2;
}
void geometryCounts() {
    const int limit = int(dommy::generation::kMaxGeneratedPointCount);
    for (auto type : {MFnData::kMesh, MFnData::kNurbsCurve, MFnData::kNurbsSurface}) {
        fixture(type);
        MPointArray result;
        assert(animation(result) == MS::kSuccess);
        assert(result.size() == (type == MFnData::kMesh ? 3U : 4U));
        const auto trace = operations;
        for (size_t i = 0; i < trace.size(); ++i) {
            if (trace[i] != "mesh.points" && trace[i] != "curve.cvs" && trace[i] != "surface.cvs")
                continue;
            fixture(type);
            shortAt = int(i);
            assert(animation(result) == MS::kFailure && shortApplied);
        }
        for (bool negative : {false, true}) {
            fixture(type);
            if (type == MFnData::kMesh) {
                emitterMeshes["source"].pointCount = limit + 1;
                invalidPointCount = negative;
            } else if (type == MFnData::kNurbsCurve) {
                geometries["source"].reportedCvCount = limit + 1;
                negativeCurveCount = negative;
            } else {
                surfaceU = negative ? -1 : limit + 1;
            }
            assert(animation(result) == MS::kInvalidParameter);
            assert(!called("mesh.points") && !called("curve.cvs") && !called("surface.cvs"));
        }
        fixture(type);
        if (type == MFnData::kMesh) emitterMeshes["source"].pointCount = limit;
        else if (type == MFnData::kNurbsCurve) geometries["source"].reportedCvCount = limit;
        else { surfaceU = 2500; surfaceV = 4000; }
        // Exact limit passes preflight, but the intentionally tiny returned
        // fixture is rejected. This avoids a 10-million-point test allocation.
        assert(animation(result) == MS::kFailure);
        assert(called("mesh.points") || called("curve.cvs") || called("surface.cvs"));
        fixture(type);
        if (type == MFnData::kMesh) emitterMeshes["source"].pointCount = 2;
        else if (type == MFnData::kNurbsCurve) geometries["source"].reportedCvCount = 3;
        else { surfaceU = 1; surfaceV = 2; }
        assert(animation(result) == MS::kFailure); // overlong returned array
        fixture(type);
        if (type == MFnData::kMesh) emitterMeshes["source"].points.clear();
        else {
            geometries["source"].points.clear();
            surfaceU = 0;
        }
        assert(animation(result) == MS::kSuccess && result.empty());
    }
    for (auto counts : {std::make_pair(4000, 4000), std::make_pair(2, -1),
                        std::make_pair(2, limit + 1)}) {
        fixture(MFnData::kNurbsSurface);
        surfaceU = counts.first; surfaceV = counts.second;
        MPointArray result;
        assert(animation(result) == MS::kInvalidParameter && !called("surface.cvs"));
    }
    fixture();
    MPointArray result;
    assert(animation(result, DP_ANIM_EMIT_MATRIX) == MS::kSuccess && operations.empty());
    fixture(MFnData::kAny);
    assert(animation(result) == MS::kSuccess && called("empty.output"));

    for (int count : {0, limit + 1}) {
        fixture();
        emitterMeshes["source"].pointCount = count;
        MBoundingBox box({7,8,9}, {10,11,12}); MObject object{"old"};
        assert(information(box, object) == MS::kInvalidParameter);
        assert(!called("mesh.points") && object.name == "old" && box.minimum.x == 7);
    }
    fixture();
    MBoundingBox box; MObject object;
    assert(information(box, object) == MS::kSuccess);
    assert(object.name == "source" && box.maximum.x == 2 && box.maximum.y == 2);
    assert(meshSpaces.back().second == MSpace::kObject);
    fixture();
    emitterMeshes["source"].pointCount = 4;
    assert(information(box, object) == MS::kFailure);
    fixture();
    invalidPointCount = true;
    assert(information(box, object) == MS::kInvalidParameter && !called("mesh.points"));
    fixture();
    emitterMeshes["source"].pointCount = limit + 1;
    assert(readScatterMesh({"source"}) == MS::kInvalidParameter && !called("mesh.points"));
    fixture();
    emitterMeshes["source"].pointCount = 4;
    assert(readScatterMesh({"source"}) == MS::kFailure);
}
void trianglesAndTraversal() {
    fixture();
    assert(readScatterMesh({"source"}) == MS::kSuccess);
    assert(meshSpaces.back().second == MSpace::kWorld);
    for (int count : {-1, 0, int(dommy::generation::kMaxGeneratedFaceCount) + 1}) {
        fixture();
        declaredTriangles = {count};
        assert(!readScatterMesh({"source"}) && !called("mesh.triangles"));
    }
    for (bool negative : {false, true}) {
        fixture();
        negativeFaceCount = negative;
        emitterMeshes["source"].faceCount = int(dommy::generation::kMaxGeneratedFaceCount) + 1;
        assert(!readScatterMesh({"source"}) && !called("mesh.triangles"));
    }
    fixture();
    declaredTriangles = {2};
    assert(readScatterMesh({"source"}) == MS::kFailure); // result count differs from preflight
    fixture();
    declaredTriangles = {int(dommy::generation::kMaxGeneratedFaceCount)};
    assert(readScatterMesh({"source"}) == MS::kFailure && called("mesh.triangles"));
    fixture();
    emitterMeshes["source"].triangleCounts = {1, 1};
    assert(readScatterMesh({"source"}) == MS::kFailure); // face array differs
    fixture();
    emitterMeshes["source"].triangleIds = {0,1};
    assert(readScatterMesh({"source"}) == MS::kFailure);
    for (int count : {0, 2}) {
        fixture();
        emitterMeshes["source"].faceCount = count;
        assert(readScatterMesh({"source"}) == MS::kFailure && !called("mesh.triangles"));
    }
    fixture();
    emitterMeshes["source"].stuck = true;
    assert(readScatterMesh({"source"}) == MS::kFailure && !called("mesh.triangles"));
    assert(std::count(operations.begin(), operations.end(), "face.triangleCount") == 1);
    fixture();
    emitterMeshes["source"].centers.push_back({0,0,0});
    declaredTriangles = {int(dommy::generation::kMaxGeneratedFaceCount), 1};
    assert(readScatterMesh({"source"}) == MS::kInvalidParameter && !called("mesh.triangles"));

    fixture();
    double result = -1;
    assert(area(result) == MS::kSuccess && result == 2);
    for (double invalid : {-1.0, 0.0, std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()}) {
        fixture(); faceAreas = {invalid}; result = -1;
        assert(!area(result) && result == -1);
    }
    fixture();
    emitterMeshes["source"].centers.push_back({0,0,0});
    faceAreas = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
    assert(!area(result, 2));
    for (size_t count : {size_t(0), size_t(2), dommy::generation::kMaxGeneratedFaceCount + 1}) {
        fixture(); assert(!area(result, count));
    }
    fixture();
    emitterMeshes["source"].stuck = true;
    assert(!area(result));
    assert(std::count(operations.begin(), operations.end(), "face.area") == 1);
}
void animationBounds() {
    double lower = 0, upper = 0;
    assert(readAnimationBounds(20, 8, true, lower, upper) == MS::kSuccess);
    assert(lower == 16 && upper == 24);
    assert(readAnimationBounds(20, -8, true, lower, upper) == MS::kSuccess);
    assert(lower == 16 && upper == 24);
    const double maximum = std::numeric_limits<double>::max();
    for (double length : {maximum, -maximum}) {
        lower = upper = 42;
        assert(!readAnimationBounds(length, maximum, true, lower, upper));
        assert(lower == 42 && upper == 42);
    }
    assert(readAnimationBounds(maximum, maximum, false, lower, upper) == MS::kSuccess);
}
int main() {
    geometryCounts();
    trianglesAndTraversal();
    animationBounds();
    animationConnections();
    MPointArray points;
    int count = 0;
    for (auto type : {MFnData::kMesh, MFnData::kNurbsCurve, MFnData::kNurbsSurface})
        count += failures([&]() {return animation(points);}, type);
    MBoundingBox box; MObject object; double value = 0;
    count += failures([&]() {return information(box, object);});
    count += failures([]() {return readScatterMesh({"source"});});
    count += failures([&]() {return area(value);});
    std::cout << "Geometry reads/traversal: " << count
              << " injected API failures plus size/cardinality/stall/range cases passed\n";
}
