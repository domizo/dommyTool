#pragma once

#include "common/utility/DGenerationLimits.h"
#include "modules/dynamics/DBullet/Collision/DCollisionMeshInput.h"
#include "modules/dynamics/DBullet/mvl/vec.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using vec3f = mvl::vec<float, 3>;
namespace MS { enum Code { kSuccess, kFailure, kInvalidParameter }; }
struct MStatus {
    MS::Code code;
    MStatus(MS::Code value=MS::kSuccess):code(value) {}
    operator bool() const { return code==MS::kSuccess; }
    bool operator==(MS::Code value) const { return code==value; }
    bool operator!=(MS::Code value) const { return code!=value; }
};
namespace MFn { enum Type { kMesh }; }
namespace MSpace { enum Space { kWorld, kTransform }; }
struct MObject { bool mesh=true; bool hasFn(MFn::Type) const { return mesh; } };

// Count overrides never allocate the reported data. Bounds-checked arrays
// detect accidental consumption of incomplete host-query results.
template<class T> struct HostArray {
    std::vector<T> values;
    int reported=-1;
    unsigned int length() const { return reported<0 ? values.size() : unsigned(reported); }
    const T& operator[](unsigned int i) const { return values.at(i); }
};
struct MFloatPoint {
    float x,y,z,w;
    MFloatPoint(float a=0,float b=0,float c=0,float d=1):x(a),y(b),z(c),w(d) {}
    void cartesianize() { x/=w; y/=w; z/=w; w=1; }
};
using MFloatPointArray=HostArray<MFloatPoint>;
using MFloatVectorArray=HostArray<MFloatPoint>;
using MIntArray=HostArray<int>;
struct MeshFixture {
    int vertices=4, faces=2;
    std::vector<int> faceTriangles={1,1};
    std::vector<MFloatPoint> points={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
    std::vector<int> counts={1,1}, indices={0,1,2,0,2,3};
    std::vector<MFloatPoint> normals={{0,1,0},{0,0,1},{1,0,0},{0,-1,0}};
    int pointReadLength=-1, countReadLength=-1, indexReadLength=-1;
    bool stalled=false;
    MSpace::Space pointSpace=MSpace::kWorld;
};
static MeshFixture fixture;
static std::vector<std::string> calls;
static int failAt=-1, throwAt=-1;
static MS::Code injected=MS::kInvalidParameter;
static int pointReads=0, triangleReads=0, normalReads=0;
MStatus operation(const char* label) {
    const int ordinal=static_cast<int>(calls.size()); calls.push_back(label);
    if (ordinal==throwAt) throw std::runtime_error("injected host exception");
    return ordinal==failAt ? injected : MS::kSuccess;
}
struct MDagPath {
    static MStatus getAPathTo(const MObject&,MDagPath&) { return operation("dag.path"); }
};
struct MFnMesh {
    bool valid=true;
    MFnMesh()=default;
    MFnMesh(const MDagPath&,MStatus* status) {
        *status=operation("mesh.bind"); valid=bool(*status);
    }
    int numVertices(MStatus* status) {
        assert(valid); *status=operation("mesh.vertices"); return fixture.vertices;
    }
    int numPolygons(MStatus* status) {
        assert(valid); *status=operation("mesh.faces"); return fixture.faces;
    }
    MObject object(MStatus* status) {
        assert(valid); *status=operation("mesh.object"); return MObject();
    }
    MStatus getPoints(MFloatPointArray& result,MSpace::Space space) {
        assert(valid); ++pointReads; fixture.pointSpace=space;
        const auto status=operation("mesh.points");
        result.values=fixture.points; result.reported=fixture.pointReadLength;
        return status;
    }
    MStatus getTriangles(MIntArray& counts,MIntArray& indices) {
        assert(valid); ++triangleReads; const auto status=operation("mesh.triangles");
        counts.values=fixture.counts; counts.reported=fixture.countReadLength;
        indices.values=fixture.indices; indices.reported=fixture.indexReadLength;
        return status;
    }
    MStatus getVertexNormals(bool weighted,MFloatVectorArray& result,MSpace::Space space) {
        assert(valid && !weighted && space==MSpace::kWorld); ++normalReads;
        const auto status=operation("mesh.normals"); result.values=fixture.normals;
        return status;
    }
    MStatus getNormals(MFloatVectorArray& result,MSpace::Space space) {
        // A legal independent normal-pool order, deliberately not vertex order.
        const auto status=getVertexNormals(false,result,space);
        if (result.values.size()==4U) std::swap(result.values[0],result.values[2]);
        return status;
    }
};
struct MItMeshPolygon {
    std::size_t index=0; bool valid;
    MItMeshPolygon(const MObject&,MStatus* status) {
        *status=operation("iterator.bind"); valid=bool(*status);
    }
    bool isDone(MStatus* status) {
        assert(valid); *status=operation("iterator.done"); return index>=fixture.faceTriangles.size();
    }
    MStatus numTriangles(int& result) {
        assert(valid); const auto status=operation("iterator.triangles");
        result=fixture.faceTriangles.at(index); return status;
    }
    MStatus next() {
        assert(valid); const auto status=operation("iterator.next");
        if (!fixture.stalled) ++index; return status;
    }
};
struct DBSoftBodyNode {
    static MStatus createHelperMesh(MFnMesh&,std::vector<int>&,std::vector<float>&,MSpace::Space);
};
