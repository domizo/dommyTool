// Exact native-import constructor and batch admission with bounded host data.
// Bulk arrays never allocate from the scalar reports used for limit tests.
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

static int calls = 0, failAt = -1, bulkReads = 0;
static MStatus injected = MS::kFailure;
static MStatus operation() { return calls++ == failAt ? injected : MStatus(MS::kSuccess); }
struct MObject {
    int index;
    MObject(int value = 0) : index(value) {}
    bool isNull() const { return index < 0; }
};
struct MFloatPoint { float x = 0, y = 0, z = 0; };
using MFloatVector = MFloatPoint;
struct MPoint { double x, y, z, w; };
using MQuaternion = MPoint;
struct DMItem {};
namespace MSpace { enum Space { kWorld, kObject }; }
template <typename T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length() const { return static_cast<unsigned int>(this->size()); }
    MStatus append(T value) {
        const MStatus status = operation(); if (status) this->push_back(value); return status;
    }
    T& operator[](std::size_t index) { return this->at(index); }
    const T& operator[](std::size_t index) const { return this->at(index); }
};
using MFloatPointArray = Array<MFloatPoint>;
using MFloatVectorArray = Array<MFloatVector>;
using MFloatArray = Array<float>;
using MIntArray = Array<int>;
static std::vector<std::array<int, 6>> reports;
static int uvCount = 3, uvIdCount = 3, uvTableCount = 3,
    pointCount = 3, normalCount = 1, polygonCorners = 3;
struct MFnMesh {
    int index;
    MFnMesh(MObject object, MStatus* status) : index(object.index) { *status = operation(); }
    int read(unsigned field, MStatus* status) { *status = operation(); return reports.at(index).at(field); }
    int numVertices(MStatus* s) { return read(0, s); }
    int numPolygons(MStatus* s) { return read(1, s); }
    int numFaceVertices(MStatus* s) { return read(2, s); }
    int numUVs(MStatus* s) { return read(3, s); }
    int numNormals(MStatus* s) { return read(4, s); }
    int numEdges(MStatus* s) { return read(5, s); }
    MStatus getPoints(MFloatPointArray& values, MSpace::Space) {
        ++bulkReads; values.resize(pointCount); return operation();
    }
    MStatus getPolygonVertices(int, MIntArray& values) {
        ++bulkReads; values.resize(polygonCorners);
        for (int i = 0; i < polygonCorners; ++i) values[i] = i;
        return operation();
    }
    MStatus getNormals(MFloatVectorArray& values, MSpace::Space) {
        ++bulkReads; values.resize(normalCount); return operation();
    }
    MStatus getNormalIds(MIntArray& counts, MIntArray& ids) {
        ++bulkReads; counts.assign(1, 3); ids.assign(3, 0); return operation();
    }
    MStatus getUVs(MFloatArray& u, MFloatArray& v) {
        ++bulkReads; u.assign(uvTableCount, 0); v.assign(uvTableCount, 0); return operation();
    }
    MStatus getAssignedUVs(MIntArray& counts, MIntArray& ids) {
        ++bulkReads; counts.assign(1, uvCount); ids.assign(uvIdCount, 0); return operation();
    }
    bool isEdgeSmooth(int, MStatus* status) { *status = operation(); return true; }
};
struct MDataHandle { MObject object; MObject data() { return object; } };
struct MArrayDataHandle {
    std::vector<MObject> values;
    std::size_t current = 0;
    unsigned int elementCount(MStatus* s) { *s = operation(); return values.size(); }
    MStatus jumpToArrayElement(unsigned int index) { current = index; return operation(); }
    MDataHandle inputValue(MStatus* s) { *s = operation(); return {values.at(current)}; }
};
#include "generator_mesh.inc"

static void reset() { calls = bulkReads = 0; failAt = -1; injected = MS::kFailure; }
int main() {
    reports = {{{3, 1, 3, 3, 1, 3}}};
    reset(); dupMesh valid(MObject(0)); assert(valid.isValid());
    const int constructorCalls = calls;
    for (auto status : {MS::kFailure, MS::kInvalidParameter})
        for (int at = 0; at < constructorCalls; ++at) {
            reset(); failAt = at; injected = status;
            dupMesh invalid(MObject(0)); assert(!invalid.isValid());
            assert(calls == at + 1);
        }
    dupMesh::ImportPlan plan;
    reset(); assert(dupMesh::preflight(MObject(0), plan));
    const int preflightCalls = calls;
    for (int at = 0; at < preflightCalls; ++at) {
        reset(); failAt = at; injected = MS::kInvalidParameter;
        assert(dupMesh::preflight(MObject(0), plan) == MS::kInvalidParameter);
        assert(plan.geometry.points == 3 && plan.geometry.connections == 3);
        assert(plan.normals == 1 && plan.edges == 3 && bulkReads == 0);
    }
    for (int field = 0; field < 6; ++field)
        for (int value : {-1, std::numeric_limits<int>::max()}) {
            reports = {{{3, 1, 3, 3, 1, 3}}}; reports[0][field] = value;
            reset(); dupMesh invalid(MObject(0));
            assert(!invalid.isValid() && bulkReads == 0);
        }
    reports = {{{3, 1, 3, 3, 1, 3}}};
    for (int* count : {&pointCount, &normalCount, &polygonCorners, &uvIdCount, &uvCount, &uvTableCount}) {
        const int saved = *count; ++*count;
        reset(); dupMesh invalid(MObject(0)); assert(!invalid.isValid()); *count = saved;
    }
    // A partial UV assignment is valid, while hasUVData remains the legacy
    // fully-assigned-UV predicate used by callers.
    uvCount = uvIdCount = 0; reset(); dupMesh partial(MObject(0));
    assert(partial.isValid() && !partial.hasUVData());
    uvTableCount = 0; reports[0][3] = 0; reset(); dupMesh noUV(MObject(0));
    assert(noUV.isValid() && !noUV.hasUVData());
    uvCount = uvIdCount = uvTableCount = 3; reports[0][3] = 3;
    reports[0][2] = 4; reset(); dupMesh tooFewCorners(MObject(0));
    assert(!tooFewCorners.isValid()); reports[0][2] = 3;

    MArrayDataHandle handle; handle.values = {MObject(-1), MObject(0)};
    std::vector<dupMesh> output;
    reset(); assert(collectDuplicateMeshes(handle, 10, output));
    assert(output.size() == 1 && output[0].isValid());
    const int batchCalls = calls;
    for (int at = 0; at < batchCalls; ++at) {
        reset(); failAt = at;
        assert(!collectDuplicateMeshes(handle, 10, output));
        assert(output.size() == 1 && output[0].isValid());
        assert(calls == at + 1);
    }
    // Each source fits by itself, but combined source+output does not. Reject
    // before any bulk read, not after constructing the first source snapshot.
    for (const auto report : std::vector<std::array<int, 6>>{
            {{500000, 200000, 800000, 0, 1, 3}},
            {{3, 1, 3, 2000000, 1, 3}},
            {{3, 1, 1000000, 0, 1, 3}}}) {
        reports = {report, report}; handle.values = {MObject(0), MObject(1)};
        reset(); assert(!collectDuplicateMeshes(handle, 10, output) && bulkReads == 0);
    }
    // Individually admissible normals/edges must also count in the batch.
    for (const auto report : std::vector<std::array<int, 6>>{
            {{3, 1, 3, 0, 9000000, 3}}, {{3, 1, 3, 0, 1, 34000000}}}) {
        reports = {report, report}; handle.values = {MObject(0), MObject(1)};
        reset(); assert(!collectDuplicateMeshes(handle, 1, output) && bulkReads == 0);
    }
    dommy::mesh::MeshMergePlan empty;
    assert(dommy::mesh::validMeshOperationStorage(empty, dommy::mesh::kMaxMeshOperationBytes));
    assert(!dommy::mesh::validMeshOperationStorage(empty, dommy::mesh::kMaxMeshOperationBytes + 1));
    std::cout << "dupMesh / Curve Duplicator: pre-bulk limits, aggregate storage, "
              << constructorCalls << " import and " << batchCalls << " batch failure boundaries passed\n";
}
