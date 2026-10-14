#pragma once
// DG/geometry ownership, native geometry queries and KD search are doubles.
// Node dispatch, direction arithmetic, budgets and typed fields are production.
#include "copy_utility_compute_doubles.h"
#include "common/utility/DDirectionMath.h"
#include "common/utility/DAttributeTransferPlan.h"
#include <atomic>
#include <random>
using DVec3Data=Imath::V3f;
using DVec3DataList=std::vector<DVec3Data>;
using ConstDPointIter=DPointList::const_iterator;
enum { DP_ATTR_CLASS_POINT=0, DP_ATTR_CLASS_PRIMITIVE, DP_ATTR_CLASS_TYPE_COUNT };
struct SearchString : CopyString {
    SearchString(const char* s=""){value=s;}
    SearchString(const CopyString& s){value=s.value;}
};
#undef MString
#define MString SearchString
struct SearchVector : Imath::V3d {
    SearchVector(double x=0,double y=0,double z=0):Imath::V3d(x,y,z){}
    SearchVector(const Imath::V3d& v):Imath::V3d(v){}
    static const SearchVector zero;
};
const SearchVector SearchVector::zero;
#define MVector SearchVector
struct MVectorArray : std::vector<MVector> {
    using std::vector<MVector>::vector;
    unsigned int length()const{return static_cast<unsigned int>(size());}
};
inline bool isVector3DAttrType(DP_ATTR_DATA_TYPE type){
    return type==DP_ATTR_VEC3I||type==DP_ATTR_VEC3F||type==DP_ATTR_VEC3D;
}
inline bool getCheckedMVectorArrayFromAttribute(const DPData& data,const std::string& name,std::size_t expected,MVectorArray& out){
    voidApi("target.attribute.read");std::vector<Imath::V3d> values;
    if(expected!=data.numberOfPoints()||!dommy::attribute::convertVector3Attribute(data.getAttr(name),expected,values))return false;
    out.assign(values.begin(),values.end());return true;
}
namespace tbb {
#ifdef DOMMY_COLLISION_DISTANCE_TESTS
static bool recordOnly = false;
static std::size_t scheduledPoints = 0U;
#endif
template<typename T>struct blocked_range{
    T first,last;blocked_range(T a,T b,std::size_t=1):first(a),last(b){}
    T begin()const{return first;}T end()const{return last;}
};
template<typename T,typename Operation>void parallel_for(const blocked_range<T>& range,Operation operation){
#ifdef DOMMY_COLLISION_DISTANCE_TESTS
    scheduledPoints += range.end() - range.begin();
    if(recordOnly)return;
#endif
    const T middle=range.begin()+(range.end()-range.begin())/2;
    operation(blocked_range<T>(range.begin(),middle));operation(blocked_range<T>(middle,range.end()));
}
}
// The real Maya function sets remain covered by the complete-unit SDK pass.
// These adapters exercise dispatch arguments, failure and transactional output.
static int nativeQueries=0;
static bool rejectNative=false,badNeighbor=false;
struct CurveDirOp {
    const DPointList& points;DVec3DataList& values;short calculation,normalType;
    CurveDirOp(MObject object,bool,double,short c,short n,const DPointList& p,DVec3DataList& v)
        :points(p),values(v),calculation(c),normalType(n){assert(!object.isNull());}
    MStatus operator()(const tbb::blocked_range<std::size_t>& range){
        const auto status=operation("native.query");if(!status)return status;
        ++nativeQueries;if(rejectNative)return MS::kFailure;
        for(std::size_t i=range.begin();i<range.end();++i)
            values[i]=DPoint(calculation+1,normalType+1,points[i][2]);
        return MS::kSuccess;
    }
};
using SurfaceDirOp=CurveDirOp;
struct MeshDirOp : CurveDirOp {
    MeshDirOp(MObject object,bool use,double distance,short c,const DPointList& p,DVec3DataList& v)
        :CurveDirOp(object,use,distance,c,0,p,v){}
};

using IDs=std::vector<uint32_t>;
using IDsListArray=std::vector<IDs>;
using DScalarList=std::vector<double>;
using DScalarListArray=std::vector<DScalarList>;
template<typename T>struct PointCloud{
    struct Point {T x,y,z;};std::vector<Point> pts;
};
namespace nanoflann {struct KDTreeSingleIndexAdaptorParams{explicit KDTreeSingleIndexAdaptorParams(int){}};}
struct DPkdtree {
    const PointCloud<double>& cloud;
    DPkdtree(int,const PointCloud<double>& c,nanoflann::KDTreeSingleIndexAdaptorParams):cloud(c){}
    void buildIndex(){voidApi("search.build");}
};
struct PointSearchKDTreeNearestWithDistanceOp {
    const DPkdtree& tree;const DPointList& points;IDs& ids;DScalarList& distances;
    PointSearchKDTreeNearestWithDistanceOp(const DPkdtree& t,const DPointList& p,IDs& i,DScalarList& d)
        :tree(t),points(p),ids(i),distances(d){}
    void operator()(const tbb::blocked_range<std::size_t>& range){
        voidApi("search.nearest");
        for(std::size_t i=range.begin();i<range.end();++i){
            distances[i]=INFINITY;ids[i]=UINT32_MAX;
            for(std::size_t j=0;j<tree.cloud.pts.size();++j){
                const auto& p=tree.cloud.pts[j];
                const double d=std::hypot(std::hypot(points[i][0]-p.x,points[i][1]-p.y),points[i][2]-p.z);
                if(d<distances[i]){distances[i]=d;ids[i]=static_cast<uint32_t>(j);}
            }
            if(badNeighbor)ids[i]=UINT32_MAX;
        }
    }
};
struct PointSearchKDTreeRadiusWithDistanceOp {
    const DPkdtree& tree;const DPointList& points;int maximum;double radius;
    IDsListArray& ids;DScalarListArray& distances;
    PointSearchKDTreeRadiusWithDistanceOp(const DPkdtree& t,const DPointList& p,int m,double r,
        IDsListArray& i,DScalarListArray& d):tree(t),points(p),maximum(m),radius(r),ids(i),distances(d){}
    void operator()(const tbb::blocked_range<std::size_t>& range){
        voidApi("search.radius");
        for(std::size_t i=range.begin();i<range.end();++i){
            std::vector<std::pair<double,uint32_t>> found;
            for(std::size_t j=0;j<tree.cloud.pts.size();++j){
                const auto& p=tree.cloud.pts[j];
                const double d=std::hypot(std::hypot(points[i][0]-p.x,points[i][1]-p.y),points[i][2]-p.z);
                if(d<radius)found.emplace_back(d,static_cast<uint32_t>(j));
            }
            std::sort(found.begin(),found.end());
            if(found.size()>static_cast<std::size_t>(maximum))found.resize(maximum);
            for(const auto& match:found){ids[i].push_back(badNeighbor?UINT32_MAX:match.second);distances[i].push_back(match.first);}
        }
    }
};
