#pragma once

// Maya array/data-owner boundaries only. The complete production compute,
// finite-point check and mesh staging plan are exercised without native Maya.
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMeshMergePlan.h"
#include <maya/MPoint.h>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
int failAt = -1, throwAt = -1, calls = 0;
MS::MStatusCode injectedStatus = MS::kFailure;
std::vector<std::string> trace;
std::vector<bool> statusSites;
int exports = 0, reads = 0, invalidReads = 0, publications = 0, cleans = 0;
int intArrayCount = 0, exportDelta = 0;
bool missingInput = false, invalidTopology = false, nonfiniteExport = false;
bool nullOwner = false, nullGeometry = false, wrongInputType = false;
int dagCreates = 0;
std::size_t syntheticCount = 0U;
short generationType = 0;

MStatus operation(const std::string& name, bool statusSite = true) {
    trace.push_back(name); statusSites.push_back(statusSite);
    const int at = calls++;
    if (at == throwAt) throw std::runtime_error("injected host boundary exception");
    return at == failAt ? injectedStatus : MS::kSuccess;
}
struct MObject {
    std::string name;
    static const MObject kNullObj;
    bool isNull() const { return name.empty(); }
};
const MObject MObject::kNullObj;
struct MPlug {
    MObject attribute;
    explicit MPlug(const MObject& value) : attribute(value) {}
    bool operator!=(const MObject& value) const { return attribute.name != value.name; }
};
struct MPointArray {
    std::vector<MPoint> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    MStatus setLength(unsigned int size) {
        const auto status = operation("points.resize");
        if (status) values.resize(size);
        return status;
    }
    MPoint& operator[](unsigned int index) { return values.at(index); }
    const MPoint& operator[](unsigned int index) const { return values.at(index); }
};
struct MIntArray {
    std::vector<int> values;
    int slot = intArrayCount++;
    MStatus setLength(unsigned int size) {
        const auto status = operation("integers.resize." + std::to_string(slot));
        if (status) values.resize(size, -999);
        return status;
    }
    int& operator[](unsigned int index) { return values.at(index); }
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
};
using MFloatArray = std::vector<float>;
struct DPData {
    static const int id = 1;
    std::vector<MPoint> points;
    bool hasValidPointTopology() const { return !invalidTopology; }
    std::size_t numberOfPoints() const { return syntheticCount ? syntheticCount : points.size(); }
    MPointArray getMPosition() const {
        (void)operation("points.export", false); ++exports;
        MPointArray result; result.values = points;
        if (exportDelta < 0 && !result.values.empty()) result.values.pop_back();
        if (exportDelta > 0) result.values.push_back(MPoint());
        if (nonfiniteExport && !result.values.empty()) result.values.back().x = NAN;
        return result;
    }
};
DPData inputData;
struct MeshResult {
    std::vector<MPoint> points;
    std::vector<int> faces, corners;
};
MeshResult staged, published;
struct MDataHandle {
    bool valid;
    short asShort() const { ++reads; if (!valid) ++invalidReads; return generationType; }
    MStatus set(const MObject&) {
        assert(valid);
        const auto status = operation("output.set");
        if (status) { published = staged; ++publications; }
        return status;
    }
    void setClean() { assert(valid); (void)operation("output.clean", false); ++cleans; }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject&, MStatus* status) {
        *status = operation("input.handle"); return {bool(*status)};
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = operation("output.handle"); return {bool(*status)};
    }
    MStatus setClean(const MPlug&) { return operation("block.clean"); }
};
const DPData* getInputDP(const MObject&, MDataBlock&) {
    return operation("input.data") && !missingInput ? &inputData : nullptr;
}
namespace dommy { namespace maya {
template <typename DataType>
MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DataType*& output) {
    assert(handle.valid); output = nullptr;
    const auto status = operation("input.bind");
    if (!status) return status;
    if (wrongInputType) return MS::kFailure;
    output = missingInput ? nullptr : &inputData;
    return MS::kSuccess;
}
}}
struct MFnMeshData {
    MObject create(MStatus* status) {
        *status = operation("mesh.data"); return {nullOwner ? "" : "mesh"};
    }
};
struct MFnMesh {
    MObject create(unsigned int pointCount, unsigned int faceCount,
                   const MPointArray& points, const MIntArray& faces,
                   const MIntArray& corners, const MObject& owner, MStatus* status) {
        if (owner.isNull()) ++dagCreates;
        *status = operation("mesh.create");
        if (*status) {
            assert(points.values.size() == pointCount && faces.values.size() == faceCount);
            staged.points = points.values; staged.faces = faces.values; staged.corners = corners.values;
        }
        return {nullGeometry ? "" : "mesh"};
    }
    MObject create(unsigned int pointCount, unsigned int faceCount,
                   const MPointArray& points, const MIntArray& faces,
                   const MIntArray& corners, const MFloatArray&, const MFloatArray&,
                   const MObject& owner, MStatus* status) {
        return create(pointCount, faceCount, points, faces, corners, owner, status);
    }
    MStatus assignUVs(const MIntArray&, const MIntArray&) { return operation("mesh.assignUV"); }
};

enum class DMeshPointSpace { Local, Transformed };
struct DMeshInternalData {
    MPointArray points;
    MIntArray faces, corners;
    bool uv = false;
    bool hasValidMeshData() const { return true; }
    bool hasUV() const { return uv; }
    std::size_t numberOfPoints() const { return points.length(); }
    int numFace() const { return static_cast<int>(faces.length()); }
    const MIntArray& getFaceList() const { return faces; }
    const MIntArray& getConnectList() const { return corners; }
    MPointArray getMPosition() const { return points; }
    MStatus getUvData(MFloatArray& u, MFloatArray& v, MIntArray& counts, MIntArray& ids) const {
        const auto status = operation("mesh.readUV");
        if (status) { u = {0,1,0}; v = {0,0,1}; counts = faces; ids = corners; }
        return status;
    }
};
inline MStatus meshOutputPoints(const DMeshInternalData& mesh, DMeshPointSpace, MPointArray& points) {
    const auto status = operation("mesh.points"); if (status) points = mesh.points; return status;
}
namespace dommy { namespace mesh {
inline MStatus restoreCornerNormals(MFnMesh&, const DMeshInternalData&, bool) {
    return operation("mesh.normals");
}
}}
std::vector<std::shared_ptr<DMeshInternalData>> groupMeshes;
