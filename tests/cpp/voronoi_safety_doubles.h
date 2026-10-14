#pragma once

// Geometry/API boundaries only. No native Maya Boolean or ray geometry is
// emulated; production control flow, budgets and checked helpers are executed.
#include <maya/MStatus.h>
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::vector<std::string> calls;
int failAt = -1, throwAt = -1, shortAt = -1;
int published = 0, cleaned = 0, releases = 0, bulkReads = 0;
bool shortApplied = false, connected = true, useCrop = true, checkCell = true;
bool stuck = false, earlyDone = false, nullCreate = false, badInputType = false;
double toleranceValue = 0.001;
double edgeSpan = 1.0;
std::vector<int> rayHits;
std::size_t rayIndex = 0U;
std::vector<float> rayLengths, rayTolerances;
MStatus api(const std::string& name) {
    const int index = static_cast<int>(calls.size());
    calls.push_back(name);
    if (index == throwAt) throw std::runtime_error("injected exception");
    return index == failAt ? MS::kFailure : MS::kSuccess;
}
bool shorten() {
    if (static_cast<int>(calls.size()) - 1 != shortAt) return false;
    shortApplied = true;
    return true;
}
}

namespace MSpace { enum Space { kObject, kWorld }; }
namespace MFnData { enum Type { kMesh, kInvalid }; }
struct MVector {
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    double& operator[](unsigned i) { assert(i < 3U); return i == 0 ? x : i == 1 ? y : z; }
    double operator[](unsigned i) const { assert(i < 3U); return i == 0 ? x : i == 1 ? y : z; }
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    MVector operator/(double divisor) const { return MVector(x/divisor,y/divisor,z/divisor); }
};
struct MPoint : MVector {
    double w = 1.0;
    MPoint(double a = 0, double b = 0, double c = 0) : MVector(a, b, c) {}
    MVector operator-(const MPoint& p) const { return MVector(x-p.x, y-p.y, z-p.z); }
};
using MFloatPoint = MPoint;
using MFloatVector = MVector;
template<class T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    unsigned length() const { return static_cast<unsigned>(this->size()); }
    MStatus setLength(unsigned n) {
        MStatus s = api("array.length");
        if (s) this->resize(n - (n != 0U && shorten() ? 1U : 0U));
        return s;
    }
    MStatus copy(const Array& other) {
        MStatus s = api("array.copy");
        if (s) { *this = other; if (!this->empty() && shorten()) this->pop_back(); }
        return s;
    }
    MStatus append(const T& value) {
        const MStatus s = api("array.append");
        if (s) this->push_back(value);
        return s;
    }
};
using MPointArray = Array<MPoint>;
using MFloatPointArray = Array<MPoint>;
using MIntArray = Array<int>;
struct MObject {
    int id;
    MObject(int value = 0) : id(value) {}
    bool isNull() const { return id == 0; }
};
using MObjectArray = Array<MObject>;
struct MBoundingBox {
    MPoint lo, hi; bool empty = true;
    void expand(const MPoint& p) {
        if (empty) { lo = hi = p; empty = false; return; }
        for (unsigned i = 0; i < 3; ++i) { lo[i] = std::min(lo[i], p[i]); hi[i] = std::max(hi[i], p[i]); }
    }
    MPoint min() const { return lo; }
    MPoint max() const { return hi; }
    double width() const { return hi.x-lo.x; }
    double height() const { return hi.y-lo.y; }
    double depth() const { return hi.z-lo.z; }
};
using DPoint = MPoint;
using DPointList = std::vector<DPoint>;
struct MPlug {
    MPlug() = default;
    MPlug(MObject, int) {}
    bool isConnected(MStatus* s) const { *s = api("plug.connected"); return connected; }
};
struct MDataHandle {
    int attribute = 0; bool valid = true;
    bool asBool() const { assert(valid); return attribute == 2 ? useCrop : checkCell; }
    double asDouble() const { assert(valid); return toleranceValue; }
    MFnData::Type type() const { assert(valid); return badInputType ? MFnData::kInvalid : MFnData::kMesh; }
    MObject data() const { assert(valid); return MObject(1); }
    void setClean() { assert(valid); ++cleaned; }
};
struct MDataBlock {
    MDataHandle inputValue(int attr, MStatus* s) {
        *s = api("input." + std::to_string(attr));
        MDataHandle h; h.attribute = attr; h.valid = bool(*s); return h;
    }
    MDataHandle outputValue(int attr, MStatus* s) {
        *s = api("output." + std::to_string(attr));
        MDataHandle h; h.attribute = attr; h.valid = bool(*s); return h;
    }
    MStatus setClean(const MPlug&) { const auto s = api("clean"); if (s) ++cleaned; return s; }
};
struct DPVoronoi {
    enum { inObject = 1, aUseCropObject = 2, aIsCheckCell = 3, aTolerance = 4, outDC = 5, outDM = 6 };
    MObject thisMObject() const { return MObject(9); }
};
struct Geometry {
    MPointArray points{{0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}};
    MIntArray counts{4}, connections{0,1,2,3};
    int reportedPoints = -1, reportedFaces = -1, reportedConnections = -1, reportedUVs = 0;
    int edges = 1; double edgeLength = 1.0;
    bool zeroArea = false, collapsed = false;
};
namespace { std::map<int, Geometry> geometries; }
struct MMeshIsectAccelParams {};
struct MFnMesh {
    enum Operation { kIntersection };
    MObject object;
    MFnMesh() = default;
    MFnMesh(MObject o, MStatus* s) : object(o) { *s = api("mesh.bind"); }
    Geometry& geometry() const { return geometries.at(object.id); }
    int numVertices(MStatus* s) const { *s = api("mesh.points.count"); const auto& g=geometry(); return g.reportedPoints == -1 ? int(g.points.size()) : g.reportedPoints; }
    int numPolygons(MStatus* s) const { *s = api("mesh.faces.count"); const auto& g=geometry(); return g.reportedFaces == -1 ? int(g.counts.size()) : g.reportedFaces; }
    int numFaceVertices(MStatus* s) const { *s = api("mesh.connections.count"); const auto& g=geometry(); return g.reportedConnections == -1 ? int(g.connections.size()) : g.reportedConnections; }
    int numUVs(MStatus* s) const { *s = api("mesh.uvs.count"); return geometry().reportedUVs; }
    int numEdges(MStatus* s) const { *s = api("mesh.edges.count"); return geometry().edges; }
    MStatus getPoints(MPointArray& p, MSpace::Space space) {
        assert(space == MSpace::kObject); ++bulkReads;
        const auto s = api("mesh.points"); if (s) { p = geometry().points; if (shorten()) p.pop_back(); } return s;
    }
    MStatus getVertices(MIntArray& counts, MIntArray& connections) {
        ++bulkReads; const auto s = api("mesh.vertices");
        if (s) { counts=geometry().counts; connections=geometry().connections; if (shorten()) connections.pop_back(); } return s;
    }
    MObject create(int n, int f, const MPointArray& p, const MIntArray& c, const MIntArray& v, MObject owner, MStatus* s) {
        *s = api("mesh.create"); assert(n == int(p.size()) && f == int(c.size()));
        if (!*s || nullCreate) return MObject();
        Geometry& g=geometries[owner.id]; g.points=p; g.counts=c; g.connections=v;
        return owner;
    }
    MMeshIsectAccelParams autoUniformGridParams() { return {}; }
    MStatus freeCachedIntersectionAccelerator() { ++releases; return api("cache.release"); }
    bool allIntersections(const MPoint&, const MVector& ray, const void*, const void*, bool,
            MSpace::Space space, float length, bool, MMeshIsectAccelParams*, bool,
            MFloatPointArray& hits, void*, void*, void*, void*, void*, float tolerance, MStatus* s) {
        assert(space == MSpace::kObject);
        assert(ray.length() > 0.0);
        *s = api("mesh.intersections"); if (!*s) return false;
        rayLengths.push_back(length); rayTolerances.push_back(tolerance);
        const int count = rayIndex < rayHits.size() ? rayHits[rayIndex++] : 1;
        if (count >= 0) hits.resize(static_cast<std::size_t>(count));
        return count != 0; // Negative count intentionally models inconsistent success.
    }
    MStatus collapseEdges(const MIntArray& ids) { assert(ids.size()==1U && ids[0]==0); const auto s=api("mesh.collapse"); if(s) geometry().collapsed=true; return s; }
    MStatus updateSurface() { return api("mesh.update"); }
    MStatus booleanOps(Operation, const MObjectArray& operands, bool) {
        assert(operands.size()==2U && operands[0].id==object.id && operands[1].id==1);
        return api("mesh.boolean");
    }
};
struct MFnMeshData {
    MObject create(MStatus* s) { *s=api("mesh.data"); return *s && !nullCreate ? MObject(3) : MObject(); }
};
struct MItMeshVertex {
    MObject object; unsigned indexValue=0;
    MItMeshVertex(MObject o, MStatus* s):object(o){*s=api("vertex.bind");}
    bool isDone(MStatus* s) { *s=api("vertex.done"); return earlyDone || indexValue>=geometries.at(object.id).points.size(); }
    MStatus next(){const auto s=api("vertex.next");if(s && !stuck)++indexValue;return s;}
    MPoint position(MSpace::Space space,MStatus* s){assert(space==MSpace::kObject);*s=api("vertex.position");return geometries.at(object.id).points.at(indexValue);}
};
struct MItMeshEdge {
    MObject object; int indexValue=0;
    MItMeshEdge(MObject o,MStatus* s):object(o){*s=api("edge.bind");}
    bool isDone(MStatus* s){*s=api("edge.done");return earlyDone || indexValue>=geometries.at(object.id).edges;}
    MStatus next(){const auto s=api("edge.next");if(s && !stuck)++indexValue;return s;}
    MPoint point(int i,MSpace::Space space,MStatus* s){assert(space==MSpace::kObject);*s=api("edge.point");return MPoint(i*edgeSpan,0,0);}
    MStatus getLength(double& value,MSpace::Space space){assert(space==MSpace::kObject);const auto s=api("edge.length");if(s)value=geometries.at(object.id).collapsed?1.0:geometries.at(object.id).edgeLength;return s;}
    int index(MStatus* s){*s=api("edge.index");return indexValue;}
    int getConnectedFaces(MIntArray& faces,MStatus* s){*s=api("edge.faces");if(*s)faces={0,0};return 2;}
};
struct MItMeshPolygon {
    MObject object;
    MItMeshPolygon(MObject o,MStatus* s):object(o){*s=api("polygon.bind");}
    MStatus setIndex(int,int&){return api("polygon.index");}
    bool zeroArea(MStatus* s){*s=api("polygon.area");return geometries.at(object.id).zeroArea;}
};
struct DCurveInternalData {
    MPointArray points; int degree;
    DCurveInternalData(const MPointArray& p,int d):points(p),degree(d){}
    bool hasValidCurveTopology()const{return degree==1 && points.size()>1U;}
};
using DCItem=std::shared_ptr<DCurveInternalData>;
struct DCurveData {
    enum{id=1};std::vector<DCItem> curves;
    bool insert(DCItem p){const auto s=api("curve.insert");if(s)curves.push_back(p);return bool(s);}
    bool hasValidCurveTopology()const{for(const auto& p:curves)if(!p->hasValidCurveTopology())return false;return true;}
    void updateBBox(){}
};
struct DMeshInternalData;
using DMItem=std::shared_ptr<DMeshInternalData>;
struct DMeshInternalData {
    static MStatus createFromMayaObject(MObject,DMItem& p){const auto s=api("mesh.import");if(s)p.reset(new DMeshInternalData);return s;}
    bool hasValidMeshData()const{return true;}
    std::size_t numberOfPoints()const{return 4U;}
    int numFace()const{return 1;}
    MIntArray getConnectList()const{return {0,1,2,3};}
    std::size_t numberOfConnections()const{return 4U;}
    std::size_t numberOfUVs()const{return 0U;}
};
struct DMeshData {
    using DMeshList=std::vector<DMItem>;
    enum{id=2};std::vector<DMItem> meshes;
    bool insert(const DMeshList& p){const auto s=api("mesh.insert");if(s)meshes.insert(meshes.end(),p.begin(),p.end());return bool(s);}
    bool hasValidMeshTopology()const{return true;}
    void updateBBox(){}
};
namespace { DCurveData curves; DMeshData meshes; }
static std::size_t cellCount=1U;
namespace voro {struct container{};}
namespace dommy {namespace voronoi {
inline bool extractCellMeshes(voro::container&,double,std::vector<MObject>& cells,
    std::size_t,std::size_t,std::size_t,std::size_t){cells.assign(cellCount,MObject(2));return true;}
}}
namespace dommy {namespace maya {
inline MStatus createPluginDataObject(int,MObject& o,DCurveData*& p){const auto s=api("plugin.create");if(s){curves.curves.clear();p=&curves;o=MObject(4);}return s;}
inline MStatus createPluginDataObject(int,MObject& o,DMeshData*& p){const auto s=api("plugin.create");if(s){meshes.meshes.clear();p=&meshes;o=MObject(4);}return s;}
template<class T>MStatus createPluginDataObject(int id,MObject& o,T*& p){return createPluginDataObject(id,o,p);}
template<class T>MStatus publishPluginDataObject(MDataHandle& h,const MObject&,const T&){assert(h.valid);const auto s=api("publish");if(s)++published;return s;}
}}

#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMeshIntersection.h"
