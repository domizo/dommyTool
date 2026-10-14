#pragma once
// Explicit affine/tree/mesher boundaries, not native OpenVDB or GPU behavior.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "math/DBounds.h"
#include "common/utility/DViewportGeometryBudget.h"
#include "common/utility/DViewportPointBudget.h"
#include "common/utility/DNumericValidation.h"
#include "modules/volume/VDB/Common/DVdbViewportMath.h"
#include "modules/volume/VDB/Common/DOpenVDBLimits.h"

static std::vector<std::string> calls;
static int failAt = -1, failureKind = 0;
static bool boundary(const std::string& name) {
    const int index = static_cast<int>(calls.size());
    calls.push_back(name);
    if (index != failAt) return true;
    if (failureKind == 1) throw std::bad_alloc();
    if (failureKind == 2) throw std::runtime_error("injected boundary failure");
    return false;
}
static void nativeCall(const char* name) {
    if (!boundary(name)) throw std::runtime_error("native boundary failure");
}
using GLuint = unsigned int;
using GLfloat = float;
using GLenum = unsigned int;
#ifndef GL_POINTS
const GLenum GL_POINTS=0;
#endif
namespace tbb {
struct split {};
template <typename T> struct blocked_range {
    T first, last;
    blocked_range(T a,T b):first(a),last(b){}
    T begin()const{return first;} T end()const{return last;}
};
static bool reversePartitions=false;
template <typename R,typename F> void parallel_for(const R& range,const F& function){
    nativeCall("parallel.for");
    if(reversePartitions){for(auto i=range.end();i!=range.begin();){--i;function(R(i,i+1));}}
    else function(range);
}
template <typename R,typename F> void parallel_reduce(const R& range,F& function){
    nativeCall("parallel.reduce");function(range);
}
}
namespace openvdb {
using Int32 = std::int32_t;
template <typename T> struct Vec3 {
    T v[3];
    explicit Vec3(T s = 0):v{s,s,s} {}
    Vec3(T x,T y,T z):v{x,y,z} {}
    template <typename U> Vec3(const Vec3<U>& p):v{T(p[0]),T(p[1]),T(p[2])} {}
    T& operator[](int i) {return v[i];} T operator[](int i) const {return v[i];}
    T& x(){return v[0];} T& y(){return v[1];} T& z(){return v[2];}
    T x() const{return v[0];} T y() const{return v[1];} T z() const{return v[2];}
    Vec3& operator-=(const Vec3& b){for(int i=0;i<3;++i)v[i]-=b[i];return *this;}
    Vec3& operator*=(T s){for(int i=0;i<3;++i)v[i]*=s;return *this;}
    Vec3 cross(const Vec3& b) const {return Vec3(v[1]*b[2]-v[2]*b[1],v[2]*b[0]-v[0]*b[2],v[0]*b[1]-v[1]*b[0]);}
    double length() const {return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
};
using Vec3d=Vec3<double>; using Vec3s=Vec3<float>; using Vec3I=Vec3<int>;
using Vec3f=Vec3s; using Vec3i=Vec3I;
inline Vec3s operator*(float value,const Vec3s& p){return Vec3s(value*p[0],value*p[1],value*p[2]);}
inline Vec3s operator+(const Vec3s& a,const Vec3s& b){return Vec3s(a[0]+b[0],a[1]+b[1],a[2]+b[2]);}
template <typename T> T zeroVal(){return T(0);}
inline Vec3d operator+(const Vec3d& a,const Vec3d& b){return Vec3d(a[0]+b[0],a[1]+b[1],a[2]+b[2]);}
inline Vec3d operator-(const Vec3d& a,const Vec3d& b){return Vec3d(a[0]-b[0],a[1]-b[1],a[2]-b[2]);}
struct Coord : Vec3<Int32> {
    explicit Coord(Int32 s=0):Vec3<Int32>(s){}
    Coord(Int32 x,Int32 y,Int32 z):Vec3<Int32>(x,y,z){}
    Vec3d asVec3d()const{return Vec3d(v[0],v[1],v[2]);}
};
inline bool operator==(const Coord& a,const Coord& b){return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];}
struct Vec4I {int v[4]={0,1,2,3}; int operator[](int i)const{return v[i];}};
struct CoordBBox {
    Coord lo,hi;
    CoordBBox():lo(std::numeric_limits<Int32>::max()),hi(std::numeric_limits<Int32>::lowest()){}
    CoordBBox(Coord a,Coord b):lo(a),hi(b){}
    const Coord& min()const{return lo;} const Coord& max()const{return hi;}
    bool empty()const{return lo[0]>hi[0] || lo[1]>hi[1] || lo[2]>hi[2];}
    void reset(Coord a,Coord b){lo=a;hi=b;}
};
struct BBoxd {
    Vec3d lo{std::numeric_limits<double>::max()},hi{-std::numeric_limits<double>::max()};
    BBoxd()=default; BBoxd(Vec3d a,Vec3d b):lo(a),hi(b){}
    const Vec3d& min()const{return lo;} const Vec3d& max()const{return hi;}
    bool empty()const{return lo[0]>hi[0] || lo[1]>hi[1] || lo[2]>hi[2];}
};
namespace math {
struct Transform {
    double matrix[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    Vec3d translation;
    Vec3d indexToWorld(const Vec3d& p) const {
        nativeCall("transform.forward"); Vec3d result=translation;
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)result[i]+=matrix[i][j]*p[j];
        return result;
    }
    Vec3d indexToWorld(const Coord& p)const{return indexToWorld(p.asVec3d());}
    Vec3d worldToIndex(const Vec3d& p) const {
        nativeCall("transform.inverse"); double cofactors[3][3];
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)
            cofactors[i][j]=matrix[(i+1)%3][(j+1)%3]*matrix[(i+2)%3][(j+2)%3]-matrix[(i+1)%3][(j+2)%3]*matrix[(i+2)%3][(j+1)%3];
        const double determinant=matrix[0][0]*cofactors[0][0]+matrix[0][1]*cofactors[0][1]+matrix[0][2]*cofactors[0][2];
        Vec3d result;
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)result[i]+=cofactors[j][i]*(p[j]-translation[j])/determinant;
        return result;
    }
    BBoxd indexToWorld(const BBoxd& box) const {
        Vec3d lo(std::numeric_limits<double>::max()),hi(-std::numeric_limits<double>::max());
        for(int corner=0;corner<8;++corner){
            Vec3d p;
            for(int axis=0;axis<3;++axis)p[axis]=(corner&(1<<axis))?box.max()[axis]:box.min()[axis];
            p=indexToWorld(p);
            for(int axis=0;axis<3;++axis){
                if(!std::isfinite(p[axis]))return BBoxd(p,p);
                lo[axis]=std::min(lo[axis],p[axis]);hi[axis]=std::max(hi[axis],p[axis]);
            }
        }
        return BBoxd(lo,hi);
    }
};
inline void calculateBounds(const Transform& transform,const Vec3d& a,const Vec3d& b,Vec3d& lo,Vec3d& hi){
    lo=Vec3d(std::numeric_limits<double>::max());hi=Vec3d(-std::numeric_limits<double>::max());
    for(int corner=0;corner<8;++corner){
        Vec3d p;for(int axis=0;axis<3;++axis)p[axis]=(corner&(1<<axis))?b[axis]:a[axis];
        p=transform.worldToIndex(p);
        for(int axis=0;axis<3;++axis){if(!std::isfinite(p[axis])){lo=hi=p;return;}
            lo[axis]=std::min(lo[axis],p[axis]);hi[axis]=std::max(hi[axis],p[axis]);}
    }
}
}
struct GridBase {
    using Ptr=std::shared_ptr<GridBase>; using ConstPtr=std::shared_ptr<const GridBase>;
    virtual ~GridBase()=default;
    CoordBBox bounds{Coord(0),Coord(1)}; math::Transform map;
    int kind=0; std::string name="density"; std::size_t memory=1024,work=8;
    CoordBBox evalActiveVoxelBoundingBox()const{nativeCall("grid.bounds");return bounds;}
    const math::Transform& constTransform()const{return map;}
    const math::Transform& transform()const{return map;}
    bool empty()const{return bounds.empty();}
    const std::string& getName()const{return name;}
    std::size_t activeVoxelCount()const{nativeCall("grid.work");return work;}
    std::size_t memUsage()const{nativeCall("grid.memory");return memory;}
};
static std::vector<Coord> valueQueries;
template <typename T> struct Sample {Coord coordinate;T value;};
template <typename T> struct Leaf {
    std::vector<Sample<T>> samples{{Coord(0),T(1)}};
    int countOverride=-1;
    struct ValueOnCIter {
        const Leaf* leaf=nullptr;std::size_t index=0;
        explicit operator bool()const{return leaf && index<leaf->samples.size();}
        ValueOnCIter& operator++(){++index;return *this;}
        T getValue()const{nativeCall("leaf.value");return leaf->samples[index].value;}
        Coord getCoord()const{nativeCall("leaf.coordinate");return leaf->samples[index].coordinate;}
    };
    ValueOnCIter cbeginValueOn()const{ValueOnCIter result;result.leaf=this;return result;}
    std::size_t onVoxelCount()const{return countOverride<0?samples.size():static_cast<std::size_t>(countOverride);}
};
template <typename T> struct Tree {
    using ValueType=T;using LeafNodeType=Leaf<T>;
    std::vector<Leaf<T>> leaves{Leaf<T>()};T background{0};Vec3d gradient{0,0,1};
    T getValue(const Coord& coord)const{
        nativeCall("tree.value");valueQueries.push_back(coord);
        for(const auto& leaf:leaves)for(const auto& sample:leaf.samples)
            if(sample.coordinate==coord)return sample.value;
        return background;
    }
};
namespace tree {
template <typename T> struct LeafManager {
    const T& data;
    explicit LeafManager(const T& t):data(t){nativeCall("leaf.manager");}
    std::size_t leafCount()const{return data.leaves.size();}
    const typename T::LeafNodeType& leaf(std::size_t i)const{return data.leaves.at(i);}
    tbb::blocked_range<std::size_t> getRange()const{return {0,leafCount()};}
};
template <typename T> struct ValueAccessor {
    const T& data;
    explicit ValueAccessor(const T& t):data(t){nativeCall("accessor.create");}
    typename T::ValueType getValue(const Coord& coord)const{return data.getValue(coord);}
};
}
namespace math {
struct CD_2ND {};
template <typename Scheme> struct ISGradient {
    template <typename A> static Vec3d result(const A& accessor,const Coord&){
        nativeCall("gradient");return accessor.data.gradient;
    }
};
}
enum {GRID_LEVEL_SET=1};
template <typename T,int Kind> struct TypedGrid:GridBase {
    using ConstPtr=std::shared_ptr<const TypedGrid>;using ValueType=T;using TreeType=Tree<T>;
    TreeType data;int gridClass=0;
    TypedGrid(){kind=Kind;}
    const TreeType& tree()const{return data;} int getGridClass()const{return gridClass;}
};
using FloatGrid=TypedGrid<float,0>;
using Vec3SGrid=TypedGrid<Vec3s,1>;
using Vec3DGrid=TypedGrid<Vec3d,2>;
static std::vector<Vec3s> mesherPoints;
static int triangle[3]={0,1,2};
static bool mesherQuad=false;
static float lastIso=0;
namespace tools {
struct PolygonPool {
    std::size_t numQuads()const{return mesherQuad?1U:0U;}
    std::size_t numTriangles()const{return mesherQuad?0U:1U;}
    Vec4I quad(std::size_t)const{return Vec4I();}
    Vec3I triangle(std::size_t)const{return Vec3I(openvdb::triangle[0],openvdb::triangle[1],openvdb::triangle[2]);}
};
using PolygonPoolList=std::vector<PolygonPool>;
struct VolumeToMesh {
    PolygonPoolList pools{PolygonPool()};
    explicit VolumeToMesh(float iso){nativeCall("mesher.create");lastIso=iso;}
    void operator()(const GridBase&){nativeCall("mesher.run");}
    std::size_t pointListSize()const{return mesherPoints.size();}
    const std::vector<Vec3s>& pointList()const{return mesherPoints;}
    std::size_t polygonPoolListSize()const{return pools.size();}
    PolygonPoolList& polygonPoolList(){return pools;}
};
}
}
using GridCPtr=openvdb::GridBase::ConstPtr; using GridPtr=openvdb::GridBase::Ptr;
using GridCRef=const openvdb::GridBase&;
using GridCPtrVec=std::vector<GridCPtr>;
using GridCPtrVecCIter=GridCPtrVec::const_iterator; using GridCPtrVecIter=GridCPtrVec::iterator;
enum class DVdbGridType { kFloat = 1, kVec3f = 6, kVec3d = 7, kUnknown = 100 };
// Local TypedGrid tags are fixture IDs, not production enum ordinals.
inline DVdbGridType getVdbGridType(GridCRef grid){
    switch(grid.kind){
    case 0:return DVdbGridType::kFloat;
    case 1:return DVdbGridType::kVec3f;
    case 2:return DVdbGridType::kVec3d;
    default:return DVdbGridType::kUnknown;
    }
}

#ifndef DOMMY_VDB_VIEWPORT_SDK
struct MStatus {
    int value; MStatus(int v=0):value(v){} explicit operator bool()const{return value==0;}
    bool operator!=(int v)const{return value!=v;}
};
namespace MS {enum{kSuccess=0,kFailure=1,kInvalidParameter=2};}
using MTypeId=int;
struct MPxData{virtual ~MPxData()=default;};
struct MString {
    std::string value; MString(const char* s=""):value(s){}
    const char* asChar()const{return value.c_str();}
};
struct MPoint {
    double x,y,z,w=1;
    explicit MPoint(double a=0,double b=0,double c=0):x(a),y(b),z(c){}
};
struct MVector {
    double v[3];MVector(double x=0,double y=0,double z=0):v{x,y,z}{}
    double operator[](int axis)const{return v[axis];}
};
struct MColor {
    float r,g,b,a;
    MColor(float red=0,float green=0,float blue=0,float alpha=1):r(red),g(green),b(blue),a(alpha){}
};
struct MBoundingBox {
    MPoint lo,hi; MBoundingBox(MPoint a,MPoint b):lo(a),hi(b){}
    MPoint min()const{return lo;} MPoint max()const{return hi;}
};
struct MObject {
    int key=0; MPxData* data=nullptr; MTypeId type=0;
    explicit MObject(int k=0):key(k){}
    bool isNull()const{return key==0 && !data;}
    static const MObject kNullObj;
};
const MObject MObject::kNullObj;
struct PlugValue {bool boolean=false; short integer=0; float number=0; MString text; MColor color;};
static std::map<int,PlugValue> values;
static std::map<int,std::string> names;
static MObject inputObject;
static bool inputConnected=true;
struct MPlug {
    int key;
    MPlug(const MObject&,const MObject& a):key(a.key){}
    bool isConnected(MStatus* status=nullptr)const{
        const bool ok=boundary("plug.connected");if(status)*status=ok?MS::kSuccess:MS::kFailure;
        return inputConnected;
    }
    MStatus getValue(MObject& object)const{
        const MStatus status=boundary("plug.object")?MS::kSuccess:MS::kFailure;
        if(status)object=inputObject;
        return status;
    }
};
struct MFnPluginData {
    MObject object;
    MFnPluginData(const MObject& o,MStatus* status):object(o){*status=boundary("data.bind")?MS::kSuccess:MS::kFailure;}
    MTypeId typeId(MStatus* status)const{*status=boundary("data.type")?MS::kSuccess:MS::kFailure;return object.type;}
    const MPxData* constData(MStatus* status)const{*status=boundary("data.pointer")?MS::kSuccess:MS::kFailure;return object.data;}
};
inline MStatus readPlugValue(const MPlug& p,bool& v){if(!boundary("read."+names[p.key]))return MS::kFailure;v=values[p.key].boolean;return MS::kSuccess;}
inline MStatus readPlugValue(const MPlug& p,short& v){if(!boundary("read."+names[p.key]))return MS::kFailure;v=values[p.key].integer;return MS::kSuccess;}
inline MStatus readPlugValue(const MPlug& p,float& v){if(!boundary("read."+names[p.key]))return MS::kFailure;v=values[p.key].number;return MS::kSuccess;}
inline MStatus readPlugValue(const MPlug& p,MString& v){if(!boundary("read."+names[p.key]))return MS::kFailure;v=values[p.key].text;return MS::kSuccess;}
inline MStatus readPlugValue(const MPlug& p,MColor& v){if(!boundary("read."+names[p.key]))return MS::kFailure;v=values[p.key].color;return MS::kSuccess;}
struct MGlobal {static void displayError(const char*){} static void displayWarning(const char*){}};
namespace dommy {namespace maya {inline void reportViewportCallbackException(const char*,const char*,const std::exception_ptr&)noexcept{}}}
#define DOMMY_RETURN_IF_FAILURE(value) do{const MStatus s=(value);if(!s)return s;}while(false)
#else
MStatus readPlugValue(const MPlug&,bool&);
MStatus readPlugValue(const MPlug&,short&);
MStatus readPlugValue(const MPlug&,float&);
MStatus readPlugValue(const MPlug&,MString&);
MStatus readPlugValue(const MPlug&,MColor&);
#endif
struct DOpenVDBData : MPxData {
    static const MTypeId id;
    GridCPtrVec mGrids;
    bool hasValidData()const;
    std::size_t numberOfGrids()const{return mGrids.size();}
    GridCPtr gridPtr(std::size_t i)const{return mGrids.at(i);}
    GridCRef grid(std::size_t i)const{return *mGrids.at(i);}
};
const MTypeId DOpenVDBData::id(123);
struct lineBufferObjectVP2 {
    std::vector<float> points,colors; std::vector<unsigned int> indexArray;
    void clear(){points.clear();colors.clear();indexArray.clear();}
};
struct valueBufferObjectVP2 : lineBufferObjectVP2 {
    std::vector<float> normals;
    void clear(){lineBufferObjectVP2::clear();normals.clear();}
    void swap(valueBufferObjectVP2& other){points.swap(other.points);colors.swap(other.colors);indexArray.swap(other.indexArray);normals.swap(other.normals);}
};
struct BufferObjectVP2 {
    std::vector<MPoint> points,normals; std::vector<unsigned int> indexArray;
    void clear(){points.clear();normals.clear();indexArray.clear();}
};
struct BufferObject {
    std::vector<float> points,colors,normals;std::vector<unsigned int> indices;
    void clear(){points.clear();colors.clear();normals.clear();indices.clear();}
    void genVertexBuffer(const std::vector<float>& v){nativeCall("legacy.position");points=v;}
    void genColorBuffer(const std::vector<float>& v){nativeCall("legacy.color");colors=v;}
    void genNormalBuffer(const std::vector<float>& v){nativeCall("legacy.normal");normals=v;}
    void genIndexBuffer(const std::vector<unsigned int>& v,GLenum){nativeCall("legacy.indices");indices=v;}
};

// Tree visitors and Clip are recording boundaries. Their native algorithms are
// not replaced by these tests; production dispatch/callback control flow is used.
template <typename Buffer> struct DrawBoundary {
    Buffer& buffer; std::size_t maximum;
    DrawBoundary(Buffer& b,std::size_t n):buffer(b),maximum(n){}
    template <typename Grid> void operator()(typename Grid::ConstPtr){
        nativeCall("tree.draw"); if(!maximum)return;
        buffer.points={0,0,0};buffer.indexArray={0};
    }
    std::size_t generatedBoxCount()const{return buffer.indexArray.size();}
    std::size_t generatedPointCount()const{return buffer.indexArray.size();}
};
using InternalNodesGeoVP2=DrawBoundary<lineBufferObjectVP2>;
using LeafNodesGeoVP2=DrawBoundary<lineBufferObjectVP2>;
using ActiveTileGeoVP2=DrawBoundary<lineBufferObjectVP2>;
struct BBoxClipOp {
    GridPtr outputGrid;
    BBoxClipOp(const openvdb::BBoxd&,bool){}
    template <typename Grid> void operator()(typename Grid::ConstPtr grid){nativeCall("clip.run");outputGrid=std::make_shared<Grid>(*grid);}
};
template <typename Op> bool processTypedGrid(GridCPtr grid,Op& op){op.template operator()<openvdb::GridBase>(grid);return true;}
template <typename Op> bool processTypedScalarGrid(GridCPtr grid,Op& op){
    const auto typed=std::dynamic_pointer_cast<const openvdb::FloatGrid>(grid);
    if(!typed)return false;
    op.template operator()<openvdb::FloatGrid>(typed);return true;
}
template <typename Op> bool processTypedVectorGrid(GridCPtr grid,Op& op){
    if(grid->kind==1){const auto typed=std::dynamic_pointer_cast<const openvdb::Vec3SGrid>(grid);if(!typed)return false;op.template operator()<openvdb::Vec3SGrid>(typed);return true;}
    if(grid->kind==2){const auto typed=std::dynamic_pointer_cast<const openvdb::Vec3DGrid>(grid);if(!typed)return false;op.template operator()<openvdb::Vec3DGrid>(typed);return true;}
    return false;
}
template <typename Op> bool processVdbScalarGrid(DVdbGridType,GridCRef grid,Op& op){return processTypedScalarGrid(GridCPtr(&grid,[](const openvdb::GridBase*){}),op);}
template <typename Op> bool processVdbVectorGrid(DVdbGridType,GridCRef grid,Op& op){return processTypedVectorGrid(GridCPtr(&grid,[](const openvdb::GridBase*){}),op);}
