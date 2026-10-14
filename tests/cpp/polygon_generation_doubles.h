#pragma once

// Native Maya storage and mesh calls are injectable. Sphere construction and
// primitive import come from the actual production bodies, not geometry mocks.
#include <maya/MStatus.h>
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DNumericValidation.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
std::vector<std::string> calls;
int failAt=-1, throwAt=-1, shortAt=-1, invalidAccesses=0, bulkReads=0, creates=0;
bool shortened=false, nullData=false, nullMesh=false;
int normalWrites=0,pointWrites=0,matrixReads=0;
const void* normalInputs[3]={};
MStatus api(const std::string& name) {
    const int index=static_cast<int>(calls.size()); calls.push_back(name);
    if(index==throwAt)throw std::runtime_error("injected exception");
    return index==failAt ? MS::kFailure : MS::kSuccess;
}
bool shorten() {
    if(static_cast<int>(calls.size())-1!=shortAt)return false;
    shortened=true;return true;
}
}
struct MFloatPoint;
struct MMatrix {
    double values[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    double* operator[](unsigned i){return values[i];}
    const double* operator[](unsigned i)const{return values[i];}
};
struct MPoint {
    double x,y,z,w;
    MPoint(double a=0,double b=0,double c=0):x(a),y(b),z(c),w(1){}
    MPoint(const MFloatPoint&);
    MPoint operator*(const MMatrix& matrix)const {
        const double values[4]={x,y,z,w};double result[4]={};
        for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)
            result[col]+=values[row]*matrix[row][col];
        MPoint point(result[0],result[1],result[2]);point.w=result[3];return point;
    }
};
struct MVector {
    double x,y,z;
    MVector(double a=0,double b=0,double c=0):x(a),y(b),z(c){}
};
struct MFloatPoint {
    float x,y,z,w;
    MFloatPoint(float a=0,float b=0,float c=0):x(a),y(b),z(c),w(1){}
};
inline MPoint::MPoint(const MFloatPoint& p):x(p.x),y(p.y),z(p.z),w(p.w){}
template<class T>struct Array:std::vector<T> {
    using std::vector<T>::vector;
    int reportedLength=-1;
    static std::string type(){return std::is_same<T,int>::value?"int":std::is_same<T,MVector>::value?"vector":"point";}
    unsigned length()const{return reportedLength<0?static_cast<unsigned>(this->size()):static_cast<unsigned>(reportedLength);}
    T& operator[](unsigned index){if(index>=this->size())++invalidAccesses;return this->at(index);}
    const T& operator[](unsigned index)const{if(index>=this->size())++invalidAccesses;return this->at(index);}
    MStatus append(const T& value){const auto s=api(type()+".append");if(s && !shorten())this->push_back(value);return s;}
    MStatus setLength(unsigned n){const auto s=api(type()+".length");if(s)this->resize(n-(n && shorten()?1U:0U));return s;}
    MStatus copy(const Array& other){const auto s=api(type()+".copy");if(s){*this=other;if(!this->empty()&&shorten())this->pop_back();}return s;}
};
using MFloatPointArray=Array<MFloatPoint>;
using MPointArray=Array<MPoint>;
using MIntArray=Array<int>;
struct MTransformationMatrix {
    MMatrix matrix;
    MMatrix asMatrix()const{++matrixReads;return matrix;}
};
using MVectorArray=Array<MVector>;
namespace MSpace {enum Space{kObject,kWorld};}
struct MObject {
    int id;MObject(int i=0):id(i){}
    static const MObject kNullObj;
    bool isNull()const{return id==0;}
};
const MObject MObject::kNullObj;
struct Geometry {
    MPointArray points{{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
    MIntArray faces{3,3},connections{0,1,2,0,2,3};
    int reportedPoints=-1,reportedFaces=-1,reportedConnections=-1,reportedUVs=0;
};
namespace {Geometry geometry;}
struct MFnMeshData {
    MObject create(MStatus* s){*s=api("mesh.data");return *s&&!nullData?MObject(1):MObject();}
};
struct MFnMesh {
    MFnMesh()=default;
    MFnMesh(MObject,MStatus* s){*s=api("mesh.bind");}
    MObject create(unsigned p,unsigned f,const MFloatPointArray& points,
                   const MIntArray& faces,const MIntArray& connections,MObject owner,MStatus* s){
        *s=api("mesh.create");++creates;
        assert(p==points.length() && f==faces.length());
        if(!*s || nullMesh)return MObject();
        geometry=Geometry();geometry.points.clear();
        for(const auto& point:points)geometry.points.push_back(MPoint(point));
        geometry.faces=faces;geometry.connections=connections;
        return owner;
    }
    int numVertices(MStatus* s){*s=api("mesh.pointCount");return geometry.reportedPoints==-1?int(geometry.points.size()):geometry.reportedPoints;}
    int numPolygons(MStatus* s){*s=api("mesh.faceCount");return geometry.reportedFaces==-1?int(geometry.faces.size()):geometry.reportedFaces;}
    int numFaceVertices(MStatus* s){*s=api("mesh.connectionCount");return geometry.reportedConnections==-1?int(geometry.connections.size()):geometry.reportedConnections;}
    int numUVs(MStatus* s){*s=api("mesh.uvCount");return geometry.reportedUVs;}
    MStatus getPoints(MPointArray& output,MSpace::Space space){
        assert(space==MSpace::kObject);++bulkReads;const auto s=api("mesh.points");
        if(s){output=geometry.points;if(!output.empty()&&shorten())output.pop_back();}return s;
    }
    MStatus getVertices(MIntArray& faces,MIntArray& connections){
        ++bulkReads;const auto s=api("mesh.topology");
        if(s){faces=geometry.faces;connections=geometry.connections;if(!connections.empty()&&shorten())connections.pop_back();}return s;
    }
    MStatus setFaceVertexNormals(MVectorArray& normals,MIntArray& faces,MIntArray& vertices,MSpace::Space space){
        assert(space==MSpace::kObject);const auto s=api("mesh.normals");
        if(s){++normalWrites;normalInputs[0]=&normals;normalInputs[1]=&faces;normalInputs[2]=&vertices;}return s;
    }
    MStatus setPoints(const MPointArray& points){
        const auto s=api("mesh.setPoints");if(s){++pointWrites;geometry.points=points;}return s;
    }
    MStatus updateSurface(){return api("mesh.updateSurface");}
};

inline std::uint64_t geometryFingerprint() {
    // Quantize coordinates to 1e-6; preserve exact point/face/connection order.
    // This avoids making a golden fixture depend on libm's final float bit.
    std::uint64_t hash=14695981039346656037ULL;
    const auto add=[&](std::int64_t value){
        const std::uint64_t bits=static_cast<std::uint64_t>(value);
        for(unsigned byte=0;byte<8;++byte){hash^=(bits>>(byte*8))&255U;hash*=1099511628211ULL;}
    };
    for(const auto& p:geometry.points)for(double value:{p.x,p.y,p.z})add(std::llround(value*1000000.0));
    for(int count:geometry.faces)add(count);
    for(int index:geometry.connections)add(index);
    return hash;
}
