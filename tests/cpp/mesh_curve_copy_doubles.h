#pragma once

// Only host boundaries are replaced: mesh iteration/control flow and numerical
// helpers under test come from production. This does not emulate Maya topology.
#include "curve_copy_doubles.h"
#include "common/utility/DDeformerMath.h"

using MIntArray = Array<int>;
using MFloatArray = Array<float>;
using DPointList = MPointArray;
using DVec3Data = MVector;
struct MeshFace {
    MIntArray ids = {0, 1, 2, 3};
    MFloatArray u = {0, 1, 1, 0}, v = {0, 0, 1, 1};
    MVector normal{0, 1, 0};
    bool hasUvs = true;
    int cornerCount = -1;
};
struct MeshGeometry {
    MPointArray points = {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}};
    std::vector<MeshFace> faces{MeshFace()};
    int reportedPoints = -1, reportedFaces = -1;
    bool stuck = false;
    bool analyticSlab = false;
};
using MFloatVector = MVector;
using MFloatPointArray = MPointArray;
struct MMeshIsectAccelParams {};
namespace {
std::map<std::string, MeshGeometry> meshes;
DCurveData objectStorage;
int acceleratorReleases = 0;
std::vector<MPoint> intersectionSources;
} // namespace
inline MObject MDataHandle::asMeshTransformed() const {
    check();
    if (input->object.isNull())
        return MObject::kNullObj;
    auto candidate = meshes.at(input->object.name);
    for (auto& p : candidate.points)
        p = p * input->matrix;
    // Normal-sensitive fixtures use translations/positive axis scales only;
    // this boundary does not emulate Maya's transformed mesh normals.
    const MObject result{input->object.name + ".transformed"};
    meshes[result.name] = candidate;
    return result;
}
struct MFnMesh {
    MObject object;
    MFnMesh(MObject o, MStatus* s) : object(o) {
        *s = api("mesh.bind");
    }
    int numVertices(MStatus* s) const {
        *s = api("mesh.pointCount");
        const auto& g = meshes.at(object.name);
        return g.reportedPoints < 0 ? int(g.points.size()) : g.reportedPoints;
    }
    int numPolygons(MStatus* s) const {
        *s = api("mesh.faceCount");
        const auto& g = meshes.at(object.name);
        return g.reportedFaces < 0 ? int(g.faces.size()) : g.reportedFaces;
    }
    MStatus getPoints(MPointArray& points, MSpace::Space space) const {
        assert(space == MSpace::kObject);
        const int index = callCount;
        const auto s = api("mesh.points");
        if (s) {
            points = meshes.at(object.name).points;
            if (index == shortAt && points.size()) {
                points.pop_back();
                shortApplied = true;
            }
        }
        return s;
    }
    MMeshIsectAccelParams autoUniformGridParams() const {
        return {};
    }
    MStatus freeCachedIntersectionAccelerator() {
        ++acceleratorReleases;
        return MS::kSuccess;
    }
    bool allIntersections(const MPoint& source, const MVector& ray, const void*, const void*, bool,
                          MSpace::Space space, float, bool, MMeshIsectAccelParams*, bool,
                          MPointArray& hits, void*, void*, void*, void*, void*, float,
                          MStatus* status) {
        assert(space == MSpace::kObject);
        *status = api("mesh.intersections");
        if (!*status)
            return false;
        intersectionSources.push_back(source);
        const auto& g = meshes.at(object.name);
        if (!g.analyticSlab || ray.x == 0)
            return false;
        double lo = g.points.front().x, hi = lo;
        for (const auto& p : g.points) {
            lo = std::min(lo, p.x);
            hi = std::max(hi, p.x);
        }
        for (double x : {lo, hi})
            if ((x - source.x) / ray.x >= 0)
                hits.push_back(MPoint(x, source.y, source.z));
        if (ray.x < 0)
            std::reverse(hits.begin(), hits.end());
        return !hits.empty();
    }
    bool closestIntersection(const MPoint& source, const MVector& ray, const void*, const void*,
                             bool, MSpace::Space space, float, bool, MMeshIsectAccelParams*,
                             MPoint& hit, void*, void*, void*, void*, void*, float,
                             MStatus* status) {
        assert(space == MSpace::kObject);
        *status = api("mesh.closestIntersection");
        if (!*status)
            return false;
        const auto& g = meshes.at(object.name);
        double nearest = std::numeric_limits<double>::infinity();
        for (const auto& p : g.points) {
            const double t = (p.x - source.x) / ray.x;
            if (t >= 0 && t < nearest) {
                nearest = t;
                hit = MPoint(p.x, source.y, source.z);
            }
        }
        return std::isfinite(nearest);
    }
    MStatus getClosestPointAndNormal(const MPoint& source, MPoint& hit, MVector& normal,
                                     MSpace::Space space, void*, MMeshIsectAccelParams*) {
        assert(space == MSpace::kObject);
        const auto status = api("mesh.closestNormal");
        if (status) {
            hit = source;
            normal = MVector(1, 0, 0);
        }
        return status;
    }
};
struct MItMeshPolygon {
    MObject object;
    std::size_t index = 0;
    MItMeshPolygon(MObject o, MStatus* s) : object(o) {
        *s = api("mesh.iterator");
    }
    const MeshFace& face() const {
        return meshes.at(object.name).faces.at(index);
    }
    MStatus reset() {
        const auto s = api("mesh.reset");
        if (s)
            index = 0;
        return s;
    }
    bool isDone(MStatus* s) const {
        *s = api("mesh.done");
        return index == meshes.at(object.name).faces.size();
    }
    MStatus next() {
        const auto s = api("mesh.next");
        if (s && !meshes.at(object.name).stuck)
            ++index;
        return s;
    }
    unsigned int polygonVertexCount(MStatus* s) const {
        *s = api("mesh.corners");
        return face().cornerCount < 0 ? face().ids.length() : unsigned(face().cornerCount);
    }
    bool hasUVs(MStatus* s) const {
        *s = api("mesh.hasUvs");
        return face().hasUvs;
    }
    MStatus getVertices(MIntArray& ids) const {
        const int at = callCount;
        const auto s = api("mesh.vertices");
        if (s) {
            ids = face().ids;
            if (at == shortAt && ids.size())
                ids.pop_back();
        }
        return s;
    }
    MStatus getUVs(MFloatArray& u, MFloatArray& v) const {
        const int at = callCount;
        const auto s = api("mesh.uvs");
        if (s) {
            u = face().u;
            v = face().v;
            if (at == shortAt && v.size())
                v.pop_back();
        }
        return s;
    }
    MStatus getNormal(MVector& n, MSpace::Space space) const {
        assert(space == MSpace::kObject);
        const auto s = api("mesh.normal");
        if (s)
            n = face().normal;
        return s;
    }
};
namespace dommy {
namespace maya {
inline bool isFloatRepresentablePoint(const MPoint& p) {
    return isFinitePoint(p) && std::abs(p.x) <= std::numeric_limits<float>::max() &&
           std::abs(p.y) <= std::numeric_limits<float>::max() &&
           std::abs(p.z) <= std::numeric_limits<float>::max();
}
template <class T> MStatus createPluginDataObject(MFnData::Type, MObject& object, T*& output) {
    const auto s = api("plugin.create");
    if (s) {
        objectStorage = DCurveData();
        object = {"pluginData"};
        output = &objectStorage;
        pending = output;
    }
    return s;
}
inline MStatus publishPluginDataObject(MDataHandle& h, MObject object, const DCurveData&) {
    return publishTypedPluginDataObject<DCurveData>(h, object, DCurveData::id);
}
} // namespace maya
} // namespace dommy
