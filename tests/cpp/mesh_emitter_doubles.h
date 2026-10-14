#pragma once

// Maya/custom-data boundaries, not a Maya mesh/UV emulator. Samplers and budgets
// are extracted unchanged from production; the fixture supplies query payloads.
#include "curve_emitter_doubles.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DUniformSampling.h"

#undef CHECK_MSTATUS
#define CHECK_MSTATUS(x)                                                                           \
    {                                                                                              \
        const MStatus s = (x);                                                                     \
        if (!s)                                                                                    \
            s.pAPIerror("mesh emitter", __LINE__);                                                 \
    }
using MFloatPointArray = MPointArray;
using MIntArray = Array<int>;
using int2 = int[2];
namespace DTools {
struct Vec2 {
    float v[2];
    Vec2(float x, float y) : v{x, y} {}
    float operator[](unsigned i) const {
        return v[i];
    }
};
} // namespace DTools
inline DPoint MPToDP(const MPoint& p) {
    return p;
}
struct EmitterMesh {
    MPointArray points{{0, 0, 0}, {2, 0, 0}, {0, 2, 0}};
    std::vector<std::array<int, 2>> edges{{{0, 1}}, {{1, 2}}, {{2, 0}}};
    MPointArray centers{{2.0 / 3, 2.0 / 3, 0}};
    MIntArray triangleCounts{1}, triangleIds{0, 1, 2};
    int pointCount = -1, faceCount = -1;
    bool hasUV = true, valid = true, stuck = false;
};
namespace {
std::map<std::string, EmitterMesh> emitterMeshes;
bool uvNoHit = false, badUVPoint = false, badConversion = false;
bool invalidPointCount = false;
std::vector<std::pair<std::string, MSpace::Space>> meshSpaces;
} // namespace
struct MeshItem {
    std::string object;
    void hasUVData(bool& value) const {
        value = emitterMeshes.at(object).hasUV;
    }
    MPointArray::const_iterator beginPt() const {
        return emitterMeshes.at(object).points.begin();
    }
    MPointArray::const_iterator endPt() const {
        return emitterMeshes.at(object).points.end();
    }
};
struct DMeshData {
    static constexpr MFnData::Type id = MFnData::kAny;
    std::vector<std::shared_ptr<MeshItem>> items;
    size_t numberOfMesh() const {
        return items.size();
    }
    std::vector<std::shared_ptr<MeshItem>>::const_iterator beginMeshes() const {
        return items.begin();
    }
    std::vector<std::shared_ptr<MeshItem>>::const_iterator endMeshes() const {
        return items.end();
    }
    bool hasValidMeshTopology() const {
        for (const auto& m : items)
            if (!m || !emitterMeshes.at(m->object).valid)
                return false;
        return true;
    }
};
inline MStatus convertMesh(const MeshItem& mesh, MObject& object, const char* operation) {
    const auto status = api(operation);
    if (status)
        object = badConversion ? MObject::kNullObj : MObject{mesh.object};
    return status;
}
inline MStatus generateMMeshFromDmesh(const MeshItem& m, MObject& o) {
    return convertMesh(m, o, "mesh.convert");
}
inline MStatus generateMMeshFromDmeshWithUV(const MeshItem& m, MObject& o) {
    return convertMesh(m, o, "mesh.convertUV");
}
struct MFnMesh {
    MObject object;
    bool valid = false;
    MFnMesh() = default;
    MFnMesh(MObject o, MStatus* s) {
        *s = setObject(o);
    }
    MStatus setObject(MObject o) {
        const auto s = api("mesh.bind");
        valid = bool(s) && !o.isNull() && emitterMeshes.count(o.name);
        if (valid)
            object = o;
        return s ? (valid ? MS::kSuccess : MS::kInvalidParameter) : s;
    }
    const EmitterMesh& mesh() const {
        assert(valid);
        return emitterMeshes.at(object.name);
    }
    int numVertices(MStatus* s) const {
        *s = api("mesh.pointCount");
        return invalidPointCount
                   ? -1
                   : (mesh().pointCount < 0 ? int(mesh().points.size()) : mesh().pointCount);
    }
    int numPolygons(MStatus* s) const {
        *s = api("mesh.faceCount");
        return mesh().faceCount < 0 ? int(mesh().centers.size()) : mesh().faceCount;
    }
    int numEdges(MStatus* s) const {
        *s = api("mesh.edgeCount");
        return int(mesh().edges.size());
    }
    MStatus getPoints(MFloatPointArray& result, MSpace::Space space = MSpace::kObject) {
        const int ordinal = callCount;
        const auto status = api("mesh.points");
        meshSpaces.emplace_back(object.name, space);
        if (status) {
            result = mesh().points;
            if (ordinal == shortAt && !result.empty()) {
                result.pop_back();
                shortApplied = true;
            }
        }
        return status;
    }
    MStatus getEdgeVertices(int index, int2& result) const {
        const auto status = api("mesh.edge");
        if (status) {
            result[0] = mesh().edges.at(index)[0];
            result[1] = mesh().edges.at(index)[1];
        }
        return status;
    }
    MStatus getTriangles(MIntArray& counts, MIntArray& ids) const {
        const int ordinal = callCount;
        const auto status = api("mesh.triangles");
        if (status) {
            counts = mesh().triangleCounts;
            ids = mesh().triangleIds;
            if (ordinal == shortAt && !ids.empty()) {
                ids.pop_back();
                shortApplied = true;
            }
        }
        return status;
    }
    MStatus getPointsAtUV(MIntArray& faces, MPointArray& points, const float2& uv,
                          MSpace::Space space, const void*, float) const {
        assert(space == MSpace::kWorld && valid);
        const int ordinal = callCount;
        const auto status = api("mesh.uv");
        if (status && !uvNoHit) {
            faces = {0, 1};
            points = {{uv[0], uv[1], badUVPoint ? NAN : 0}, {uv[0], uv[1], 1}};
            if (ordinal == shortAt) {
                points.pop_back();
                shortApplied = true;
            }
        }
        return status;
    }
};
struct MItMeshPolygon {
    MObject object;
    unsigned ordinal = 0;
    bool valid;
    MItMeshPolygon(MObject o, MStatus* status = nullptr) : object(o) {
        const auto s = api("face.bind");
        valid = bool(s);
        if (status)
            *status = s;
    }
    const EmitterMesh& mesh() const {
        assert(valid);
        return emitterMeshes.at(object.name);
    }
    MStatus reset() {
        const auto s = api("face.reset");
        if (s)
            ordinal = 0;
        return s;
    }
    bool isDone(MStatus* status = nullptr) {
        const auto s = api("face.done");
        if (status)
            *status = s;
        return ordinal >= mesh().centers.size();
    }
    MStatus next() {
        const auto s = api("face.next");
        if (s && !mesh().stuck)
            ++ordinal;
        return s;
    }
    unsigned int index(MStatus* status) const {
        *status = api("face.index");
        return ordinal;
    }
    MPoint center(MSpace::Space space = MSpace::kObject, MStatus* status = nullptr) {
        const auto s = api("face.center");
        if (status)
            *status = s;
        meshSpaces.emplace_back(object.name, space);
        return s ? mesh().centers.at(ordinal) : MPoint();
    }
};
inline MStatus DGenerateRelaxPoint2D(size_t count, size_t, int, float, float, float, float,
                                     MPointArray& result) {
    const int ordinal = callCount;
    const auto status = api("uv.relax");
    if (status) {
        result.assign(count, MPoint(.25, 0, .75));
        if (ordinal == shortAt && count) {
            result.pop_back();
            shortApplied = true;
        }
    }
    return status;
}
// Old-path adapters remain solely to prove pre-fix source mutations fail.
struct MeshPluginData {
    MObject create(MFnData::Type, MStatus* s) {
        *s = api("plugin.create");
        pointStorage = DPData();
        return *s ? object() : MObject::kNullObj;
    }
    MObject object() const {
        return {"pointData"};
    }
};
namespace dommy {
namespace maya {
template <class T> T* pluginDataPointer(MeshPluginData&, MStatus* s) {
    *s = api("plugin.pointer");
    return *s ? &pointStorage : nullptr;
}
} // namespace maya
} // namespace dommy
