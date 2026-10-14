#pragma once
// Native mesh/rotation/search/custom-data boundaries only. This is not a Maya,
// nanoflann or TBB emulator. Production compute/traversal/staging math executes
// unchanged; real metadata-budget tests live in point_cache_values_tests.cpp.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DGenerationLimits.h"
#include <ImathVec.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <functional>

namespace {
int shortAt=-1, restCount=-1, animatedCount=-1, edgeCount=3;
int published=0, cleaned=0, replaced=0, copied=0, bulkReads=0;
bool early=false, stuck=false, badIndex=false, badFrame=false, connected=true;
bool nullInput=false, nullMesh=false, badMeshType=false;
int badSearch=0;
float inputRadius=0.1f;
}
inline void throwingOperation(const char* name) {
    if(!operation(name)) throw std::runtime_error("injected dependency failure");
}
using DPoint=Imath::V3f;
using DPointList=std::vector<DPoint>;
using DDoubleArray=std::vector<double>;
using IDs=std::vector<uint32_t>;
using IDsListArray=std::vector<IDs>;
using DScalarList=std::vector<double>;
using DScalarListArray=std::vector<DScalarList>;
using MTypeId=MFnData::Type;
namespace MSpace {enum Space{kWorld,kObject};}
struct MFloatPoint {
    float x,y,z,w;
    MFloatPoint(float x=0,float y=0,float z=0):x(x),y(y),z(z),w(1){}
};
struct MPoint {
    double x,y,z,w;
    MPoint(const MFloatPoint& p):x(p.x),y(p.y),z(p.z),w(p.w){}
};
struct MMatrix {
    double values[4][4];
    MMatrix(){for(int r=0;r<4;++r)for(int c=0;c<4;++c)values[r][c]=r==c;}
    MMatrix(const double v[4][4]){for(int r=0;r<4;++r)for(int c=0;c<4;++c)values[r][c]=v[r][c];}
    double* operator[](unsigned int r){return values[r];}
    const double* operator[](unsigned int r)const{return values[r];}
    MMatrix& operator+=(const MMatrix& b){for(int r=0;r<4;++r)for(int c=0;c<4;++c)values[r][c]+=b[r][c];return *this;}
    MMatrix inverse()const {MMatrix out;for(int r=0;r<4;++r)for(int c=0;c<4;++c)out[r][c]=values[c][r];return out;}
    static const MMatrix identity;
};
const MMatrix MMatrix::identity;
inline MMatrix operator*(double n,const MMatrix& m){MMatrix out;for(int r=0;r<4;++r)for(int c=0;c<4;++c)out[r][c]=n*m[r][c];return out;}
inline MMatrix operator*(const MMatrix& a,const MMatrix& b){MMatrix out;for(int r=0;r<4;++r)for(int c=0;c<4;++c){out[r][c]=0;for(int k=0;k<4;++k)out[r][c]+=a[r][k]*b[k][c];}return out;}
struct MVector {
    double x,y,z;
    MVector(double x=0,double y=0,double z=0):x(x),y(y),z(z){}
    MVector(const MFloatPoint& p):x(p.x),y(p.y),z(p.z){}
    double operator[](unsigned int i)const{return i==0?x:i==1?y:z;}
    MVector operator-(const MVector& b)const{return {x-b.x,y-b.y,z-b.z};}
    MVector operator*(double n)const{return {x*n,y*n,z*n};}
    double operator*(const MVector& b)const{return x*b.x+y*b.y+z*b.z;}
    MVector operator^(const MVector& b)const{return {y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x};}
    MVector& operator+=(const MVector& b){x+=b.x;y+=b.y;z+=b.z;return *this;}
    MVector& operator/=(double n){x/=n;y/=n;z/=n;return *this;}
    MVector& operator*=(const MMatrix& m){const auto old=*this;x=old.x*m[0][0]+old.y*m[1][0]+old.z*m[2][0];y=old.x*m[0][1]+old.y*m[1][1]+old.z*m[2][1];z=old.x*m[0][2]+old.y*m[1][2]+old.z*m[2][2];return *this;}
    double length()const{return std::sqrt(x*x+y*y+z*z);}
    void normalize(){*this/=length();}
    static const MVector xAxis,yAxis;
};
const MVector MVector::xAxis(1,0,0);
const MVector MVector::yAxis(0,1,0);
inline MVector DPToMV(const DPoint& p){return {p[0],p[1],p[2]};}
struct MTransformationMatrix {
    MMatrix matrix;
    MTransformationMatrix()=default;
    explicit MTransformationMatrix(const MMatrix& m):matrix(m){}
    MMatrix asRotateMatrix()const{throwingOperation("frame");auto out=matrix;if(badFrame)out[0][0]=NAN;return out;}
};
template<class T> struct Array:std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length()const{return static_cast<unsigned int>(this->size());}
    T& operator[](std::size_t i){assert(i<this->size());return this->at(i);}
    const T& operator[](std::size_t i)const{assert(i<this->size());return this->at(i);}
    MStatus setLength(unsigned int n){const int at=callCount;auto s=operation("matrix.length");if(s)this->resize(shortAt==at&&n?n-1:n);return s;}
};
using MFloatPointArray=Array<MFloatPoint>;
using MMatrixArray=Array<MMatrix>;
using MIntArray=Array<int>;
namespace {MFloatPointArray restPoints, animatedPoints;std::vector<MIntArray> adjacency;}
struct Group {std::vector<bool> ids;const std::vector<bool>& getGroupIds()const{return ids;}};
struct DPData {
    static const MTypeId id=MFnData::kMesh;
    DPointList points;
    std::map<std::string,std::shared_ptr<Group>> groups;
    std::string metadata="preserved";
    std::size_t metadataBytes=0,countOverride=0;
    bool valid=true;
    std::size_t numberOfPoints()const{return countOverride?countOverride:points.size();}
    bool hasValidPointTopology()const{if(!valid)return false;for(const auto& p:points)for(int a=0;a<3;++a)if(!std::isfinite(p[a]))return false;return true;}
    const DPointList& getPositionConst()const{return points;}
    auto beginGrp()const->decltype(groups.begin()){return groups.begin();}
    auto endGrp()const->decltype(groups.end()){return groups.end();}
    void setDPData(const DPData& p){throwingOperation("data.copy");*this=p;++copied;}
    using PointCopyOperation=std::function<MStatus(DPoint&,std::size_t,std::size_t)>;
    MStatus copyRepeatedFrom(const DPData& source,std::size_t count,const PointCopyOperation& placement){
        const auto status=operation("data.batch");if(!status)return status;
        assert(count==1U);DPData candidate=source;candidate.groups.clear();
        for(const auto& group:source.groups)candidate.groups[group.first]=std::make_shared<Group>(*group.second);
        for(std::size_t i=0;i<candidate.points.size();++i){const auto placed=placement(candidate.points[i],0U,i);if(!placed)return placed;}
        if(!candidate.hasValidPointTopology())return MS::kFailure;
        candidate.updateBBox();*this=std::move(candidate);++copied;return MS::kSuccess;
    }
    bool replacePositions(const DPointList& p){auto s=operation("data.replace");if(!s)return false;points=p;++replaced;return hasValidPointTopology();}
    void updateBBox(){throwingOperation("data.bounds");}
};
const MTypeId DPData::id;
namespace {DPData inputData, outputData, publishedData;}
struct MDataHandle {
    std::string name;
    MFnData::Type type()const{return badMeshType?MFnData::kString:MFnData::kMesh;}
    MObject data()const{return nullMesh?MObject::kNullObj:MObject{name};}
    float asFloat()const{return inputRadius;}
    MStatus set(const MObject&){auto s=operation("output.set");if(s){publishedData=outputData;++published;}return s;}
    void setClean(){throwingOperation("output.clean");++cleaned;}
};
struct MPlug {
    MObject attribute;
    MPlug(const MObject& a):attribute(a){}
    MPlug(const MObject&,const MObject& a):attribute(a){}
    bool operator!=(const MObject& a)const{return attribute.name!=a.name;}
    bool isConnected(MStatus* status)const{*status=operation("connected."+attribute.name);return connected;}
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& a,MStatus* s){*s=operation("input."+a.name);return {a.name};}
    MDataHandle outputValue(const MObject&,MStatus* s){*s=operation("output.handle");return {"output"};}
    MStatus setClean(const MPlug&){return operation("block.clean");}
};
struct MFnMesh {
    bool rest;
    MFnMesh(const MObject& obj,MStatus* s):rest(obj.name=="inRestMesh"){*s=operation("mesh.bind."+obj.name);}
    int numVertices(MStatus* s){*s=operation(rest?"rest.count":"animated.count");int n=rest?restCount:animatedCount;return n!=-1?n:static_cast<int>(rest?restPoints.size():animatedPoints.size());}
    int numEdges(MStatus* s){*s=operation("mesh.edges");return edgeCount;}
    MStatus getPoints(MFloatPointArray& out,MSpace::Space space){assert(space==MSpace::kWorld);const int at=callCount;auto s=operation(rest?"rest.points":"animated.points");if(s){++bulkReads;out=rest?restPoints:animatedPoints;if(at==shortAt&&!out.empty())out.pop_back();}return s;}
};
struct MItMeshVertex {
    unsigned int i=0;
    MItMeshVertex(const MObject&,MStatus* s){*s=operation("iterator.bind");}
    bool isDone(MStatus* s){*s=operation("iterator.done");return early||i==restPoints.size();}
    int index(MStatus* s){*s=operation("iterator.index");return badIndex?-1:static_cast<int>(i);}
    MStatus getConnectedVertices(MIntArray& out){auto s=operation("iterator.adjacency");if(s)out=adjacency.at(i);return s;}
    MStatus next(){auto s=operation("iterator.next");if(s&&!stuck)++i;return s;}
};
struct MFnPluginData {
    MObject object()const{return {"output"};}
    MObject create(MTypeId,MStatus* status){*status=operation("data.create");return *status?MObject{"output"}:MObject::kNullObj;}
};
namespace dommy {namespace maya {
template<class T>MStatus getPluginDataFromHandle(const MDataHandle&,MTypeId,const T*& out){auto s=operation("input.point.bind");out=s&&!nullInput?&inputData:nullptr;return s;}
template<class T>bool validatePluginData(const T& p){return p.hasValidPointTopology();}
template<class T>MStatus createPluginData(MFnPluginData&,MTypeId,T*& out){auto s=operation("data.create");out=s?&outputData:nullptr;return s;}
template<class T>MStatus getPluginData(MFnPluginData&,T*& out){auto s=operation("data.bind");out=s?&outputData:nullptr;return s;}
template<class T>MStatus getPluginDataFromObject(const MObject&,MTypeId,const T*& out){auto s=operation("data.inspect");out=s?&outputData:nullptr;return s;}
}}
namespace tbb {
template<class T>struct blocked_range {T first,last;blocked_range(T a,T b,T=1):first(a),last(b){}T begin()const{return first;}T end()const{return last;}};
template<class R,class Op>void parallel_for(const R& r,Op& op){op(r);}
}
namespace nanoflann {struct KDTreeSingleIndexAdaptorParams{explicit KDTreeSingleIndexAdaptorParams(int){}};}
template<class T>struct PointCloud {struct Point {T x,y,z;};std::vector<Point> pts;};
inline void CreateSearchDataMFP(const MFloatPointArray& p,PointCloud<double>& cloud){throwingOperation("cloud");for(const auto& v:p)cloud.pts.push_back({v.x,v.y,v.z});}
struct DPkdtree {
    const PointCloud<double>& cloud;
    DPkdtree(int,const PointCloud<double>& c,nanoflann::KDTreeSingleIndexAdaptorParams):cloud(c){throwingOperation("search.bind");}
    void buildIndex(){throwingOperation("search.build");}
};
struct PointSearchKDTreeRadiusWithDistanceOp {
    const DPkdtree& tree;const DPointList& points;int limit;double radius;IDsListArray& ids;DScalarListArray& distances;
    PointSearchKDTreeRadiusWithDistanceOp(const DPkdtree& t,const DPointList& p,int n,double r,IDsListArray& i,DScalarListArray& d):tree(t),points(p),limit(n),radius(r),ids(i),distances(d){throwingOperation("search.prepare");}
    void operator()(const tbb::blocked_range<std::size_t>& range){
        throwingOperation("search.run");
        for(std::size_t q=range.begin();q<range.end();++q){
            std::vector<std::pair<double,uint32_t>> matches;
            for(std::size_t i=0;i<tree.cloud.pts.size();++i){const auto& p=tree.cloud.pts[i];const double x=p.x-points[q][0],y=p.y-points[q][1],z=p.z-points[q][2];const double d=std::sqrt(x*x+y*y+z*z);if(d<radius)matches.push_back({d,static_cast<uint32_t>(i)});}
            std::sort(matches.begin(),matches.end());if(matches.size()>static_cast<std::size_t>(limit))matches.resize(limit);
            for(const auto& m:matches){ids[q].push_back(m.second);distances[q].push_back(m.first);}
            if(badSearch==1)ids[q].push_back(999999U);
            if(badSearch==2&&!distances[q].empty())distances[q][0]=NAN;
            if(badSearch==3&&!ids[q].empty())ids[q][0]=999999U;
            if(badSearch==4){ids[q].clear();distances[q].push_back(1.0);}
        }
    }
};
