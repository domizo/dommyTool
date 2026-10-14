#pragma once

// Host data/query/publication boundaries only. Triangulation of the small
// convex fixtures is supplied below; the production remesher/slicer is real.
#include "mesh_emitter_doubles.h"
#include "common/utility/DMayaTriangleMesh.h"
#include "common/utility/DSliceBudget.h"
#include "geometry/DMeshRemesher.h"
#include "geometry/DPolygonTriangulator.h"
#include "geometry/DTrianglePlaneSlicer.h"

struct RemeshItem {
    DPointList points{{0, 0, 0}, {.2, 0, 0}, {.2, .2, 0}, {0, .2, 0}};
    MIntArray faces{4}, connections{0, 1, 2, 3};
    MMatrix matrix;
    std::size_t reportedPoints = 0;
    RemeshItem() = default;
    RemeshItem(const MPointArray& p, const MIntArray& f, const MIntArray& c)
        : points(p), faces(f), connections(c) {}
    const DPointList* getPositionPtrConst() const {
        return &points;
    }
    const MIntArray& getFaceList() const {
        return faces;
    }
    const MIntArray& getConnectList() const {
        return connections;
    }
    const MMatrix& getTransformMatrix() const {
        return matrix;
    }
    void setTransformMatrix(const MMatrix& m) {
        matrix = m;
    }
    std::size_t numberOfPoints() const {
        return reportedPoints ? reportedPoints : points.size();
    }
    int numFace() const {
        return faces.size();
    }
    std::size_t numberOfConnections() const {
        return connections.size();
    }
    std::size_t numberOfUVs() const {
        return 0;
    }
};
using DMItem = std::shared_ptr<RemeshItem>;
struct RemeshData {
    static constexpr MFnData::Type id = MFnData::kAny;
    using DMeshList = std::vector<DMItem>;
    DMeshList items;
    bool valid = true;
    std::size_t numberOfMesh() const {
        return items.size();
    }
    auto beginMeshes() const -> decltype(items.begin()) {
        return items.begin();
    }
    auto endMeshes() const -> decltype(items.end()) {
        return items.end();
    }
    bool hasValidMeshTopology() const {
        for (const auto& item : items)
            if (!item)
                return false;
        return valid;
    }
    bool insert(const DMItem& item) {
        if (!api("mesh.insert"))
            return false;
        items.push_back(item);
        return true;
    }
    bool insert(const DMeshList& batch) {
        if (batch.empty()) return true;
        if (!api("mesh.batch.insert")) return false;
        RemeshData candidate;
        candidate.items = items;
        for (const auto& item : batch) {
            if (!item) return false;
            candidate.items.push_back(std::make_shared<RemeshItem>(*item));
        }
        candidate.updateBBox();
        items.swap(candidate.items);
        return true;
    }
    void updateBBox() {
        if (!api("mesh.bounds"))
            throw std::runtime_error("bounds");
    }
};
static RemeshData remeshInput, remeshStorage, remeshOutput;
static DCurveData sliceStorage;
static bool badNativeCounts = false;
static int reportedTriangles = -1;
static int triangleReadFault = 0;
static bool oversizedNativePoints = false;
inline const RemeshData* getInputDM(MObject, MDataBlock&) {
    return api("mesh.input") ? &remeshInput : nullptr;
}
inline MStatus getValidInputDM(MObject, MDataBlock&, const RemeshData*& result) {
    result = nullptr;
    const auto s = api("mesh.input");
    if (!s) return s;
    if (!remeshInput.hasValidMeshTopology()) return MS::kInvalidParameter;
    result = &remeshInput;
    return s;
}
inline MStatus generateMMeshFromDmeshWithUV(const RemeshItem& item, MObject& object) {
    const auto s = api("mesh.convertUV");
    if (!s)
        return s;
    object = {"mesh" + std::to_string(emitterMeshes.size())};
    auto& mesh = emitterMeshes[object.name];
    mesh.points = item.points;
    mesh.centers = {};
    mesh.triangleCounts = {};
    mesh.triangleIds = {};
    unsigned offset = 0;
    for (int count : item.faces) {
        mesh.centers.push_back(MPoint());
        mesh.triangleCounts.push_back(count - 2);
        for (int i = 1; i < count - 1; ++i) {
            mesh.triangleIds.push_back(item.connections.at(offset));
            mesh.triangleIds.push_back(item.connections.at(offset + i));
            mesh.triangleIds.push_back(item.connections.at(offset + i + 1));
        }
        offset += count;
    }
    if (badNativeCounts)
        mesh.triangleCounts.at(0) = -1;
    return s;
}
struct SlicePolygon : MItMeshPolygon {
    using MItMeshPolygon::MItMeshPolygon;
    MStatus numTriangles(int& count) {
        const auto s = api("face.triangleCount");
        if (s)
            count = reportedTriangles >= 0 ? reportedTriangles : mesh().triangleCounts.at(ordinal);
        return s;
    }
};
struct RemeshNativeMesh : MFnMesh {
    using MFnMesh::MFnMesh;
    int numVertices(MStatus* status) const {
        const int count = MFnMesh::numVertices(status);
        return oversizedNativePoints ? int(dommy::generation::kMaxGeneratedPointCount + 1U) : count;
    }
    MStatus getTriangles(MIntArray& counts, MIntArray& ids) const {
        const MStatus status = MFnMesh::getTriangles(counts, ids);
        if (!status)
            return status;
        if (triangleReadFault == 1 && ids.size() >= 6U)
            ids.resize(ids.size() - 3U);
        if (triangleReadFault == 2 && !counts.empty())
            counts.pop_back();
        if (triangleReadFault == 3 && !counts.empty())
            --counts[0];
        if (triangleReadFault == 4 && !counts.empty())
            counts[0] = -1;
        if (triangleReadFault == 5 && !ids.empty())
            ids[0] = int(mesh().points.size());
        if (triangleReadFault == 6 && !ids.empty())
            ids[0] = -1;
        return status;
    }
};
struct RemeshPlugin {
    MObject create(MFnData::Type, MStatus* s) {
        *s = api("plugin.create");
        remeshStorage = RemeshData();
        sliceStorage = DCurveData();
        return *s ? object() : MObject::kNullObj;
    }
    MObject object() const {
        return {"remeshData"};
    }
};
namespace dommy {
namespace maya {
template <typename T> T* pluginDataPointer(RemeshPlugin&, MStatus*);
template <> inline RemeshData* pluginDataPointer<RemeshData>(RemeshPlugin&, MStatus* s) {
    *s = api("plugin.pointer");
    return *s ? &remeshStorage : nullptr;
}
template <> inline DCurveData* pluginDataPointer<DCurveData>(RemeshPlugin&, MStatus* s) {
    *s = api("plugin.pointer");
    pending = &sliceStorage;
    return *s ? pending : nullptr;
}
template <>
inline MStatus publishTypedPluginDataObject<RemeshData>(MDataHandle& h, MObject, MFnData::Type) {
    h.check();
    const auto s = api("publish");
    if (s) {
        remeshOutput = remeshStorage;
        ++published;
    }
    return s;
}
} // namespace maya
} // namespace dommy
using MPointArrayPtr = std::shared_ptr<MPointArray>;
#define MFnPluginData RemeshPlugin
#define DMeshData RemeshData
#define DMeshInternalData RemeshItem
#define MItMeshPolygon SlicePolygon
#define MFnMesh RemeshNativeMesh
