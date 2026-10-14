#pragma once
// Maya/custom-data boundaries only. Node bodies, traversal, exact mean and
// attribute publication execute production code. Rotation is a supplied host
// result, not an emulator of Maya's quaternion/Euler implementation.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DNumericValidation.h"
#include "common/utility/DPointMean.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DAttributePublication.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>

#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
namespace MSpace { enum Space { kObject, kWorld }; }
namespace {
int shortAt = -1, earlyPass = -1, stuckPass = -1, nullMode = 0;
int pointCount = -1, cornerCount = -1, faceCount = 1;
int translationWrites = 0, rotationWrites = 0, published = 0, cleaned = 0;
bool badIndex = false, badNormal = false, zeroNormal = false, badColor = false;
std::array<double, 3> translation{{99, 99, 99}}, rotation{{99, 99, 99}};
std::array<double, 3> hostRotation{{0.1, 0.2, 0.3}};
std::vector<double> samples;
MStatus api(const std::string& name) { return operation(name); }
}
struct MQuaternion {};
struct MVector {
    double x, y, z;
    MVector(double x=0, double y=0, double z=0):x(x), y(y), z(z) {}
    static const MVector zero;
    MVector operator*(double d)const { return {x*d, y*d, z*d}; }
    MVector operator/(double d)const { return {x/d, y/d, z/d}; }
    double operator*(const MVector& v)const { return x*v.x+y*v.y+z*v.z; }
    MVector& operator-=(const MVector& v) { x-=v.x; y-=v.y; z-=v.z; return *this; }
    MVector& operator+=(const MVector& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
    double length()const { return std::sqrt(*this * *this); }
    double angle(const MVector& v)const { return std::acos(std::max(-1.0, std::min(1.0, (*this*v)/(length()*v.length())))); }
    MQuaternion rotateTo(const MVector&)const { return {}; }
    MVector rotateBy(const MQuaternion&)const { return *this; }
};
const MVector MVector::zero;
struct MPoint {
    double x, y, z, w;
    MPoint(double x=0, double y=0, double z=0, double w=1):x(x),y(y),z(z),w(w) {}
    MVector operator-(const MPoint& p)const { return {x-p.x, y-p.y, z-p.z}; }
    MPoint& operator+=(const MVector& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
};
struct MColor {
    float r, g, b, a;
    MColor(float r=0,float g=0,float b=0):r(r),g(g),b(b),a(1) {}
};
template<class T> struct Array:std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length()const { return static_cast<unsigned int>(this->size()); }
    T& operator[](std::size_t i) { assert(i<this->size()); return this->at(i); }
    const T& operator[](std::size_t i)const { assert(i<this->size()); return this->at(i); }
    MStatus setLength(unsigned int n) {
        const int at=callCount; const auto s=api("array.length");
        if(s) this->resize(at==shortAt && n ? n-1 : n); return s;
    }
};
using MPointArray=Array<MPoint>;
using MIntArray=Array<int>;
using MVectorArray=Array<MVector>;
using DPoint=std::array<double,3>;
using DPointList=std::vector<DPoint>;
using DDoubleArray=std::vector<double>;
struct DVec3Data {
    float v[3]; DVec3Data(float x=0,float y=0,float z=0):v{x,y,z} {}
    float operator[](unsigned int i)const { return v[i]; }
};
using DVec3DataList=std::vector<DVec3Data>;
namespace {
MPointArray meshPoints;
MIntArray polygonIds;
std::vector<MIntArray> faces, neighbours;
MVector faceNormal;
MPoint faceCenter;
}
struct DPAttributeData {
    using Ptr=std::shared_ptr<DPAttributeData>;
    virtual ~DPAttributeData()=default;
    virtual std::size_t attrSize()const=0;
    virtual std::string typeName()const=0;
    virtual Ptr clone()const=0;
};
struct DPVec3fAttr:DPAttributeData {
    DVec3DataList values;
    explicit DPVec3fAttr(const DVec3DataList& v):values(v) {}
    std::size_t attrSize()const override { return values.size(); }
    std::string typeName()const override { return "vec3f"; }
    Ptr clone()const override { return Ptr(new DPVec3fAttr(*this)); }
};
struct DPDoubleAttr:DPAttributeData {
    DDoubleArray values;
    DPDoubleAttr(const DDoubleArray& v,double):values(v) {}
    std::size_t attrSize()const override { return values.size(); }
    std::string typeName()const override { return "double"; }
    Ptr clone()const override { return Ptr(new DPDoubleAttr(*this)); }
};
struct Attributes {
    std::map<std::string,DPAttributeData::Ptr> attributes;
    auto beginAttr()->decltype(attributes.begin()) { return attributes.begin(); }
    auto endAttr()->decltype(attributes.end()) { return attributes.end(); }
    DPAttributeData::Ptr getAttr(const std::string& n)const {
        auto it=attributes.find(n);return it==attributes.end()?nullptr:it->second;
    }
    void addAttr(const std::string& n,const DPAttributeData& value) { attributes[n]=value.clone(); }
    void addAttrWithValues(const std::string& n,const DVec3DataList& v) { addAttr(n,DPVec3fAttr(v)); }
};
struct MeshItem:Attributes {
    DPointList points;
    const DPointList* getPositionPtrConst()const { return &points; }
};
struct DMeshData:Attributes {
    static const MFnData::Type id=MFnData::kMesh;
    using DMeshIter=std::vector<std::shared_ptr<MeshItem>>::iterator;
    std::vector<std::shared_ptr<MeshItem>> meshes;
    std::size_t totalOverride=0;
    bool valid=true;
    bool hasValidMeshTopology()const { return valid; }
    bool getCheckedTotalVertex(std::size_t& count)const {
        count=0;for(const auto& m:meshes)if(m)count+=m->points.size();
        if(totalOverride)count=totalOverride; return true;
    }
    MStatus copyFrom(const DMeshData& source) {
        const auto s=api("data.copy"); if(s){attributes=source.attributes;meshes.clear();valid=source.valid;
            for(const auto& m:source.meshes)meshes.push_back(m?std::make_shared<MeshItem>(*m):nullptr);}
        return s;
    }
    std::size_t numberOfMesh()const { return meshes.size(); }
    DMeshIter beginMeshes() { return meshes.begin(); }
    DMeshIter endMeshes() { return meshes.end(); }
};
namespace { DMeshData inputData, stagedData; }
struct MPlug {
    std::string name;
    MPlug(const MObject&,const MObject& a):name(a.name) {}
    bool operator!=(const MObject& a)const { return name!=a.name; }
};
struct MDataHandle {
    std::string name;bool valid;double value;MString string;
    void check()const { assert(valid); }
    MObject asMesh()const { check();return nullMode==1?MObject::kNullObj:MObject{"mesh"}; }
    int asInt()const { check();return int(value); }
    short asShort()const { check();return short(value); }
    bool asBool()const { check();return value!=0; }
    MString asString()const { check();return string; }
    void set3Double(double x,double y,double z) {
        check();if(name=="outTranslate"){++translationWrites;translation={{x,y,z}};}
        else {++rotationWrites;rotation={{x,y,z}};}
    }
    MStatus set(const MObject&) { check();auto s=api("output.set");if(s)++published;return s; }
    void setClean() { check();++cleaned; }
};
struct MDataBlock {
    std::map<std::string,double> controls;
    std::map<std::string,std::string> strings;
    MDataHandle inputValue(const MObject& a,MStatus* s) {
        *s=api("input "+a.name);return {a.name,bool(*s),controls[a.name],strings[a.name].c_str()};
    }
    MDataHandle outputValue(const MObject& a,MStatus* s) {
        *s=api("output "+a.name);return {a.name,bool(*s),0,""};
    }
    MStatus setClean(const MPlug&) { const auto s=api("clean");if(s)++cleaned;return s; }
};
namespace dommy { namespace maya {
template<class T> MStatus getPluginDataFromHandle(const MDataHandle& handle,MFnData::Type,const T*& data) {
    handle.check();data=nullptr;const auto status=api("input.bind");
    if(status&&nullMode!=1)data=&inputData;return status;
}
template<class T> MStatus createPluginDataObject(MFnData::Type,MObject& object,T*& data) {
    const auto s=api("data.create");if(!s)return s;
    if(nullMode==2){data=nullptr;return MS::kFailure;}
    object={"staged"};stagedData=DMeshData();data=&stagedData;return s;
}
inline bool validatePluginData(const DMeshData& data) { return data.hasValidMeshTopology(); }
}}
inline MStatus DMGeneratePrimitiveCenterList(const DMeshData& data,DVec3DataList& values) {
    const auto s=api("data.centers");if(s)values.assign(data.numberOfMesh(),DVec3Data(10,20,30));return s;
}
inline void checkDMeshPointAttribute(const std::string& name,DMeshData& data,bool remove) {
    if(remove)for(auto& m:data.meshes)if(m)m->attributes.erase(name);
}
inline MStatus generateMMeshFromDmeshWithUV(const MeshItem&,MObject& object) {
    const auto s=api("mesh.convert");if(s)object=nullMode==3?MObject::kNullObj:MObject{"mesh"};return s;
}
struct MFnMesh {
    MFnMesh(const MObject& object,MStatus* s) { *s=api("mesh.bind");if(*s && object.isNull())*s=MS::kFailure; }
    int numVertices(MStatus* s) { *s=api("mesh.pointsCount");return pointCount==-1?int(meshPoints.size()):pointCount; }
    int numPolygons(MStatus* s) { *s=api("mesh.facesCount");return faceCount; }
    int polygonVertexCount(int,MStatus* s) { *s=api("mesh.cornersCount");return cornerCount==-1?int(polygonIds.size()):cornerCount; }
    MStatus getPoints(MPointArray& values,MSpace::Space) {
        const int at=callCount;const auto s=api("mesh.points");if(s){values=meshPoints;if(at==shortAt && !values.empty())values.pop_back();}return s;
    }
    MStatus getPolygonVertices(int,MIntArray& values) {
        const int at=callCount;const auto s=api("mesh.vertices");if(s){values=polygonIds;if(at==shortAt && !values.empty())values.pop_back();}return s;
    }
    MStatus getPolygonNormal(int,MVector& normal,MSpace::Space) {
        const auto s=api("mesh.normal");if(s)normal=badNormal?MVector(NAN,0,0):zeroNormal?MVector():faceNormal;return s;
    }
};
struct MItMeshVertex {
    unsigned int current=0;int pass=0;
    MItMeshVertex(const MObject&,MStatus* s) { *s=api("vertex.bind"); }
    bool isDone(MStatus* s) { *s=api("vertex.done");return current>=(pass==earlyPass?0U:meshPoints.size()); }
    MStatus next() { const auto s=api("vertex.next");if(s && pass!=stuckPass)++current;return s; }
    MStatus reset() { const auto s=api("vertex.reset");if(s){current=0;++pass;}return s; }
    int index(MStatus* s) { *s=api("vertex.index");return badIndex?-1:int(current); }
    MStatus getConnectedFaces(MIntArray& values) { auto s=api("vertex.faces");if(s)values=faces.at(current);return s; }
    MStatus getConnectedVertices(MIntArray& values) { auto s=api("vertex.neighbours");if(s)values=neighbours.at(current);return s; }
};
struct MItMeshPolygon {
    MItMeshPolygon(const MObject&,MStatus* s) { *s=api("polygon.bind"); }
    MStatus setIndex(int face,int&) { auto s=api("polygon.index");if(s && face!=0)return MS::kInvalidParameter;return s; }
    MStatus getNormal(MVector& normal,MSpace::Space) {
        const auto s=api("polygon.normal");if(s)normal=badNormal?MVector(NAN,0,0):zeroNormal?MVector():faceNormal;return s;
    }
    MPoint center(MSpace::Space,MStatus* s) { *s=api("polygon.center");return faceCenter; }
};
struct ColorRamp {
    ColorRamp()=default;
    ColorRamp(const MPlug&,MStatus* s) { *s=api("ramp.bind"); }
    void getColorAtPosition(float value,MColor& color,MStatus* s) {
        *s=api("ramp.sample");if(*s){samples.push_back(value);color=MColor(badColor?NAN:value,.5f,1-value);}
    }
};
#define MRampAttribute ColorRamp
struct MTransformationMatrix {
    enum RotationOrder { kXYZ };
    void rotateBy(const MQuaternion&,MSpace::Space,MStatus* s) { *s=api("transform.rotate"); }
    MStatus getRotation(double* values,RotationOrder&) {
        auto s=api("transform.rotation");if(s)std::copy(hostRotation.begin(),hostRotation.end(),values);return s;
    }
};

inline MDataBlock setup(int kind,int mode=0) {
    resetCalls();shortAt=earlyPass=stuckPass=-1;nullMode=0;pointCount=cornerCount=-1;faceCount=1;
    translationWrites=rotationWrites=published=cleaned=0;
    badIndex=badNormal=zeroNormal=badColor=false;
    translation=rotation={{99,99,99}};hostRotation={{.1,.2,.3}};samples.clear();
    meshPoints={{0,0,0},{-1,0,1},{0,0,2},{2,0,0},{3,0,0}};
    polygonIds={0,1,2};faces={{0},{0},{0},{},{}};neighbours={{1,2},{0},{0},{},{}};
    faceNormal={0,0,1};faceCenter={0,0,1};
    if(kind==0){meshPoints={{1,0,0},{0,0,1},{-1,0,0}};faceNormal={0,1,0};}
    inputData=DMeshData();stagedData=DMeshData();
    auto mesh=std::make_shared<MeshItem>();
    for(const auto& p:meshPoints)mesh->points.push_back({{p.x,p.y,p.z}});
    mesh->addAttr("curvature",DPDoubleAttr({9,9,9,9,9},0));inputData.meshes.push_back(mesh);
    MDataBlock block;block.controls={{"faceIndex",0},{"aAddCenter",1},{"aAddCurvature",1},
        {"aSmoothIterate",1},{"aCurvatureOutputType",mode}};
    block.strings={{"aCenterAttributeName","center"},{"aCurvatureAttributeName","curvature"}};
    return block;
}
