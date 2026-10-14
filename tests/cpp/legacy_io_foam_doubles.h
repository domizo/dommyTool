#pragma once

// Failure-injectable API boundaries only; production parsers, topology builder
// and complete compute bodies run unchanged. Not a Maya DG/mesh emulator.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DNumericValidation.h"
#include "common/file/DLegacyTextData.h"
#include "common/file/DSequencePath.h"
#include "modules/generators/icosahedron/DIcosahedronTopology.h"
#include <fstream>
#include <iostream>
#include <map>

#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
namespace {
int shortAt = -1, nullMode = 0, published = 0, cleaned = 0;
bool wrongColorName = false;
MStatus api(const std::string& name) { return operation(name); }
}
struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0):x(x),y(y),z(z) {}
    MVector operator+(const MVector& v)const{return {x+v.x,y+v.y,z+v.z};}
    MVector operator-(const MVector& v)const{return {x-v.x,y-v.y,z-v.z};}
    MVector operator*(double v)const{return {x*v,y*v,z*v};}
    MVector operator/(double v)const{return {x/v,y/v,z/v};}
    MVector operator-()const{return {-x,-y,-z};}
    MVector normal()const{const double n=length();return n>0 ? *this/n : MVector();}
    MVector& operator/=(double v){*this=*this/v;return *this;}
    double operator*(const MVector& v)const{return x*v.x+y*v.y+z*v.z;}
    MVector& operator+=(const MVector& v){*this=*this+v;return *this;}
    double length()const{return std::sqrt(*this * *this);}
};
inline MVector operator*(double t,const MVector& v){return v*t;}
struct MPoint : MVector {
    double w = 1.0;
    MPoint(double x=0,double y=0,double z=0,double w=1):MVector(x,y,z),w(w){}
    MPoint(const MVector& v):MVector(v){}
    MPoint& operator*=(double value){x*=value;y*=value;z*=value;return *this;}
};
using MFloatPoint = MPoint;
struct MColor {
    float r,g,b,a;
    MColor(float r=0,float g=0,float b=0,float a=1):r(r),g(g),b(b),a(a){}
};
template<class T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    unsigned length()const{return static_cast<unsigned>(this->size());}
    T& operator[](std::size_t i){return this->at(i);}
    const T& operator[](std::size_t i)const{return this->at(i);}
    MStatus append(const T& v){const int at=callCount;const auto s=api("array.append");
        if(s && at!=shortAt)this->push_back(v);return s;}
    MStatus set(const T& v,unsigned i){const auto s=api("array.set");if(s)this->at(i)=v;return s;}
    MStatus setLength(unsigned n){const int at=callCount;const auto s=api("array.length");
        if(s)this->resize(at==shortAt && n ? n-1 : n);return s;}
    MStatus copy(const Array& source){const int at=callCount;const auto s=api("array.copy");
        if(s){*this=source;if(at==shortAt && !this->empty())this->pop_back();}return s;}
};
using MPointArray = Array<MPoint>;
using MFloatPointArray = Array<MPoint>;
using MVectorArray = Array<MVector>;
using MFloatArray = Array<float>;
using MColorArray = Array<MColor>;
using MIntArray = Array<int>;
using MStringArray = Array<MString>;
struct MeshData {
    MPointArray points;
    MIntArray counts, ids;
    std::map<std::string,MColorArray> colors;
    std::map<std::string,MIntArray> colorIds;
    std::string current = "other";
    int reportedCorners = -1;
};
namespace {
std::map<std::string,MeshData> meshes;
MVectorArray positions;
MFloatArray radii;
MPointArray decoded;
MObject output{"old"};
}
struct MPlug {
    MObject attribute;
    bool operator!=(const MObject& other)const{return attribute.name!=other.name;}
    bool operator==(const MObject& other)const{return !(*this!=other);}
};
struct TestTypeId {unsigned value;unsigned id()const{return value;}};
struct MDataHandle {
    bool valid;
    MObject object;
    double value;
    MFnData::Type dataType;
    void check()const{assert(valid);}
    int asInt()const{check();return static_cast<int>(value);}
    double asDouble()const{check();return value;}
    bool asBool()const{check();return value!=0;}
    MString asString()const{check();return object.name.c_str();}
    MFnData::Type type()const{check();return dataType;}
    TestTypeId typeId()const{check();return {static_cast<unsigned>(dataType)};}
    MObject data()const{check();return object;}
    MStatus set(const MObject& value){check();const auto s=api("output.set");
        if(s){output=value;++published;}return s;}
    void setClean(){check();++cleaned;}
};
struct MDataBlock {
    std::map<std::string,double> controls;
    std::string path;
    MFnData::Type meshType=MFnData::kMesh,positionType=MFnData::kVectorArray,radiusType=MFnData::kFloatArray;
    MDataHandle inputValue(const MObject& attr,MStatus* status=nullptr){
        const auto valueStatus=api("input."+attr.name);if(status)*status=valueStatus;
        const auto type=attr.name=="inPosition"?positionType:attr.name=="radiusArray"?radiusType:meshType;
        return {bool(valueStatus),{attr.name=="filePath" || attr.name=="RadiusAttribute"?path:"input"},controls[attr.name],type};
    }
    MDataHandle outputValue(const MObject&,MStatus* status){
        *status=api("output.handle");return {bool(*status),{"output"},0,MFnData::kMesh};}
    MStatus setClean(const MPlug&){const auto s=api("data.clean");if(s)++cleaned;return s;}
};
struct MFileObject {
    MString path;
    MStatus setRawFullName(const MString& p){const auto s=api("file.resolve");if(s)path=p;return s;}
    MString resolvedFullName()const{return path;}
};
struct MFnPointArrayData {
    MObject create(const MPointArray& values,MStatus* status){*status=api("points.create");
        if(!*status || nullMode==3)return MObject::kNullObj;decoded=values;return {"points"};}
};
struct MFnVectorArrayData {
    MFnVectorArrayData(const MObject&,MStatus* s){*s=api("vectors.bind");}
    MVectorArray array(MStatus* s)const{*s=api("vectors.read");return positions;}
};
struct MFnFloatArrayData {
    MFnFloatArrayData(const MObject&,MStatus* s){*s=api("radii.bind");}
    MFloatArray array(MStatus* s)const{*s=api("radii.read");return radii;}
};
struct MFnMeshData {
    MObject create(MStatus* s){*s=api("meshData.create");
        return !*s || nullMode==1?MObject::kNullObj:MObject{"candidate"};}
};
struct MFnMesh {
    MObject object;
    MFnMesh()=default;
    MFnMesh(const MObject& obj,MStatus* s):object(obj){*s=api("mesh.bind");}
    MObject copy(const MObject& source,const MObject& dest,MStatus* s){*s=api("mesh.copy");
        if(!*s || nullMode==2)return MObject::kNullObj;
        meshes[dest.name]=meshes.at(source.name);object=dest;return dest;}
    int numVertices(MStatus* s)const{*s=api("mesh.count");return int(meshes.at(object.name).points.size());}
    int numPolygons(MStatus* s)const{*s=api("mesh.faces");return int(meshes.at(object.name).counts.size());}
    int numFaceVertices(MStatus* s)const{*s=api("mesh.corners");const auto& m=meshes.at(object.name);
        return m.reportedCorners==-1?int(m.ids.size()):m.reportedCorners;}
    MStatus getVertices(MIntArray& counts,MIntArray& ids){const int at=callCount;const auto s=api("mesh.vertices");
        if(s){counts=meshes.at(object.name).counts;ids=meshes.at(object.name).ids;
            if(at==shortAt && !ids.empty())ids.pop_back();}return s;}
    MStatus getColorSetNames(MStringArray& names){const auto s=api("mesh.colorSets");
        if(s)for(const auto& set:meshes.at(object.name).colors)names.push_back(set.first.c_str());return s;}
    MStatus createColorSetDataMesh(MString& name){const auto s=api("mesh.createColors");
        if(s){if(wrongColorName)name="wrong";meshes.at(object.name).colors[name.asChar()]={};}return s;}
    MStatus clearColors(const MString* name){assert(name);const auto s=api("mesh.clearColors");
        if(s)meshes.at(object.name).colors.at(name->asChar()).clear();return s;}
    MStatus setColors(const MColorArray& colors,const MString* name){assert(name);const auto s=api("mesh.setColors");
        if(s)meshes.at(object.name).colors.at(name->asChar())=colors;return s;}
    MStatus assignColors(const MIntArray& ids,const MString* name){assert(name);const auto s=api("mesh.assignColors");
        if(s){for(int id:ids)assert(id>=0 && unsigned(id)<meshes.at(object.name).colors.at(name->asChar()).length());
            meshes.at(object.name).colorIds[name->asChar()]=ids;}return s;}
    MObject create(int vertices,int faces,const MFloatPointArray& points,const MIntArray& counts,
                   const MIntArray& ids,const MObject& dest,MStatus* s){
        *s=api("mesh.generate");assert(vertices==int(points.size()) && faces==int(counts.size()));
        assert(ids.size()==counts.size()*3);
        for(int id:ids)assert(id>=0 && id<vertices);
        if(!*s || nullMode==4)return MObject::kNullObj;
        auto& m=meshes[dest.name];m.points=points;m.counts=counts;m.ids=ids;return dest;}
};

inline MDataBlock resetRun() {
    resetCalls();shortAt=-1;nullMode=0;published=cleaned=0;wrongColorName=false;output={"old"};
    meshes.clear();decoded.clear();
    auto& m=meshes["input"];
    m.points={{0,0,0},{1,0,0},{0,1,0}};m.counts={3};m.ids={0,1,2};
    m.colors["other"]={{0.5f,0.6f,0.7f}};
    positions={{0,0,0},{1,0,0}};radii={1,1};
    MDataBlock b;
    b.controls={{"frame",1},{"offset",0},{"padding",4},{"frequency",1},
        {"radiusOffset",0},{"radiusResultOffset",0},{"selfDeform",1}};
    return b;
}
