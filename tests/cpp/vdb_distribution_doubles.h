#pragma once
#include "vdb_clip_doubles.h"
#include <random>
// Recording host/native boundaries, NOT sparse geometry or random-distribution equivalence.
static int scatterCalls, packingCalls, baseCalls, meshCalls;
static bool badPoint, extraPoint, badSphere, extraSphere, interrupted, badMask, nullBase, badBase;
static bool badTransform, shortTransform, noSpheres;
static std::string scatterMode;
static std::vector<float> receivedMinimum, receivedMaximum;
static int baseDetail;
static int auxiliaryPublications, sourceVertexCount=3, sourcePolygonCount=1;
static std::vector<MPoint> sourceMeshPoints{{32,32,32},{33,32,32},{32,33,32}};
static std::vector<int> sourceFaceTriangles{1};
static bool badMeshRead, nullMeshInput, nullCreatedMesh;
static std::vector<int> createdMeshIndices;
static int invalidArrayAccesses;
using DPoint = MPoint;
using DPointList = std::vector<DPoint>;
struct DPData : MPxData {
    static const int id = 456;
    DPointList points;
    std::size_t numberOfPoints() const { return points.size(); }
    void setPosition(const DPointList& p) { nativeCall("dp.positions"); points=p; }
    void updateBBox() { nativeCall("dp.bounds"); }
};
const int DPData::id;
static DPData stagedPoints;
struct DistributionHandle : MDataHandle {
    bool staged=false;
    DistributionHandle(const MDataHandle& h) : MDataHandle(h) {}
    double asDouble() const { check(); return value.number; }
    std::int64_t asInt64() const { check(); return value.wideInteger; }
    MObject asMesh() const { check();return {nullMeshInput?"null":"inputMesh"}; }
    MObject asMeshTransformed() const { check();return {nullMeshInput?"null":"transformedMesh"}; }
    MObject data() {check();return {nullMeshInput?"null":"inputMesh"};}
    void set(const MString&) {nativeCall("info.set");++auxiliaryPublications;}
    MStatus set(const MObject& object) {
        assert(!object.isNull()); const auto s=operation("output.set");
        if(s&&!staged){++publications;if(object.name=="staged")outputData=stagedData;} return s;
    }
    void setClean() { nativeCall("output.handleClean"); }
};
struct DistributionBlock : MDataBlock {
    DistributionHandle inputValue(const MObject& a,MStatus* s) {return MDataBlock::inputValue(a,s);}
    DistributionHandle outputValue(const MObject&,MStatus* s) {
        *s=operation("output.handle"); return MDataHandle{InputValue(),bool(*s)};
    }
    MStatus setClean(const MPlug&) { return operation("output.clean"); }
};
namespace dommy {namespace maya {
template<typename T>MStatus createPluginDataObject(const int&,MObject& object,T*& result) {
    DOMMY_RETURN_IF_FAILURE(operation("dp.create"));
    if(nullOutput)return MS::kFailure;
    DOMMY_RETURN_IF_FAILURE(operation("dp.pointer"));
    object={"points"};result=&stagedPoints;return MS::kSuccess;
}
template<typename T>MStatus publishTypedPluginDataObject(DistributionHandle& h,const MObject& o,const int&) {
    DOMMY_RETURN_IF_FAILURE(operation("dp.validate"));
    if(invalidOutputPayload)return MS::kInvalidParameter;
    DOMMY_RETURN_IF_FAILURE(h.set(o));h.setClean();return MS::kSuccess;
}
template<typename T>MStatus publishPluginDataObject(DistributionHandle& h,const MObject& o,const T&) {
    return publishTypedPluginDataObject<T>(h,o,0);
}
}}
template<typename T>struct HostArray {
    std::vector<T> values;
    unsigned int length() const {return unsigned(values.size());}
    const T& operator[](unsigned int i)const{if(i>=values.size())++invalidArrayAccesses;return values.at(i);}
    T& operator[](unsigned int i){if(i>=values.size())++invalidArrayAccesses;return values.at(i);}
    MStatus append(const T& value){const auto s=operation("array.append");if(s)values.push_back(value);return s;}
    MStatus setLength(unsigned int n){const auto s=operation("array.length");if(s)values.resize(n);return s;}
};
using MPointArray=HostArray<MPoint>;
using MFloatPoint=MPoint;
using MFloatPointArray=HostArray<MFloatPoint>;
using MFloatArray=HostArray<float>;
using MIntArray=HostArray<int>;
namespace MSpace {enum Space{kObject,kWorld};}
static MSpace::Space lastMeshSpace=MSpace::kObject;
struct MVector : MPoint {explicit MVector(const MPoint& p):MPoint(p){}};
struct MTransformationMatrix {
    double radius=1; MPoint offset;
    MStatus addScale(const double* s,MSpace::Space){const auto status=operation("mesh.scale");radius=s[0];return status;}
    MStatus addTranslation(const MVector& t,MSpace::Space){const auto s=operation("mesh.translate");offset=t;return s;}
};
struct DMeshSphere {
    MObject meshObj;
    explicit DMeshSphere(int detail){nativeCall("mesh.base");++baseCalls;baseDetail=detail;meshObj={nullBase?"null":"base"};}
};
struct meshPrimData {
    bool hasExplicitNormals = false;
    MObject m_obj;
    std::vector<bool> surfaceGroup,interiorGroup,seamGroup,seamPointGroup;
    MPointArray m_pts;
    MIntArray m_faces,m_connects;
    meshPrimData()=default;
    void readBase() {
        nativeCall("mesh.readBase");
        // Only cardinalities mimic the legacy frequency convention. This is not
        // the native icosphere geometry, winding, seam or Maya object lifetime.
        const std::size_t factor=baseDetail==1?1:std::size_t(1)<<(2*baseDetail);
        m_pts.values.resize(10*factor+2,MPoint(1,0,0));
        m_faces.values.resize(20*factor,3);m_connects.values.resize(60*factor,0);
        if(badBase)m_connects.values[0]=-1;
    }
    MStatus duplicateTransform(const MTransformationMatrix& t,meshPrimData& result)const {
        result=*this;
        for(auto& p:result.m_pts.values){p.x=p.x*t.radius+t.offset.x;p.y=p.y*t.radius+t.offset.y;p.z=p.z*t.radius+t.offset.z;}
        if(badTransform)result.m_pts.values[0].x=INFINITY;
        if(shortTransform)result.m_connects.values.pop_back();
        // A late copy can fail even after apparently complete arrays exist.
        return operation("mesh.duplicate");
    }
};
struct MFnMeshData {
    MObject create(MStatus* s){*s=operation("mesh.createData");return {nullOutput?"null":"meshdata"};}
};
struct MFnMesh {
    MFnMesh()=default;
    MFnMesh(const MObject&,MStatus* s){*s=operation("mesh.attach");}
    int numVertices(MStatus* s){*s=operation("mesh.vertexCount");return sourceVertexCount;}
    int numPolygons(MStatus* s){*s=operation("mesh.faceCount");return sourcePolygonCount;}
    MStatus getPoints(MFloatPointArray& points,MSpace::Space space){
        lastMeshSpace=space;
        const auto s=operation("mesh.points");points.values=sourceMeshPoints;
        if(badMeshRead)points.values.pop_back();return s;
    }
    MObject create(unsigned int points,unsigned int faces,const MFloatPointArray& p,
        const MIntArray& f,const MIntArray& c,MObject&,MStatus* s){
        *s=operation("mesh.create");++meshCalls;
        assert(points==p.length()&&faces==f.length());std::size_t count=0;
        for(int n:f.values){assert(n>=3);count+=n;}assert(count==c.length());
        for(int n:c.values)assert(n>=0&&unsigned(n)<points);
        for(const auto& v:p.values)assert(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));
        createdMeshIndices=c.values;
        return {nullCreatedMesh?"null":"mesh"};
    }
};
MStatus getPrimDataFromMesh(MFnMesh&, meshPrimData&);
namespace boost {namespace algorithm {
inline std::string join(const std::vector<std::string>& parts,const char* separator){
    std::string out;for(const auto& p:parts){if(!out.empty())out+=separator;out+=p;}return out;
}
}}
namespace openvdb {
using Index64=std::uint64_t;
struct Vec4s {
    float v[4];Vec4s(float x,float y,float z,float r):v{x,y,z,r}{}
    float x()const{return v[0];}float y()const{return v[1];}float z()const{return v[2];}
    float& operator[](int i){return v[i];}float operator[](int i)const{return v[i];}
};
namespace util {struct NullInterrupter {bool wasInterrupted()const{return interrupted;}};}
template<typename G>typename G::ConstPtr gridConstPtrCast(const GridBase::ConstPtr& p){return std::dynamic_pointer_cast<const G>(p);}
namespace tools {
template<typename P,typename R,typename I>struct ScatterBoundary {
    P& points;R& random;
    ScatterBoundary(P& p,R& r):points(p),random(r){}
    void emit(std::size_t n) {
        // Never allocate an enormous result if a mutation removes a guard.
        assert(n<=10000000);assert(n<1000);if(extraPoint)++n;
        for(std::size_t i=0;i<n;++i){(void)random();points.add(Vec3d(badPoint?NAN:32,32,32));}
    }
};
template<typename P,typename R,typename I>struct UniformPointScatter:ScatterBoundary<P,R,I> {
    Index64 count;float density=0;bool uniformDensity;
    UniformPointScatter(P& p,Index64 n,R& r,double,I*):ScatterBoundary<P,R,I>(p,r),count(n),uniformDensity(false){}
    UniformPointScatter(P& p,float d,R& r,double,I*):ScatterBoundary<P,R,I>(p,r),count(0),density(d),uniformDensity(true){}
    template<typename G>bool operator()(const G& grid){
        nativeCall("native.scatter");++scatterCalls;scatterMode=uniformDensity?"uniform":"count";
        // Same vulnerable native expression, evaluated under float-cast UBSan.
        const Index64 n=uniformDensity?Index64(density*grid.transform().voxelVolume()*double(grid.activeVoxelCount())):count;
        this->emit(grid.activeVoxelCount()?n:0);return true;
    }
};
template<typename P,typename R,typename I>struct NonUniformPointScatter:ScatterBoundary<P,R,I> {
    float density;
    NonUniformPointScatter(P& p,float d,R& r,double,I*):ScatterBoundary<P,R,I>(p,r),density(d){}
    template<typename G>bool operator()(const G& grid){
        nativeCall("native.scatter");++scatterCalls;scatterMode="weighted";
        const auto dx=grid.voxelSize();const double perVoxel=density*(dx[0]*dx[1]*dx[2]);
        for(auto it=grid.cbeginValueOn();it;++it){const double d=(*it)*perVoxel*it.getVoxelCount();
            const int n=int(d);this->emit(n>0?std::size_t(n):0);}return true;
    }
};
template<typename P,typename R,typename I>struct DenseUniformPointScatter:ScatterBoundary<P,R,I> {
    float perVoxel;
    DenseUniformPointScatter(P& p,float d,R& r,double,I*):ScatterBoundary<P,R,I>(p,r),perVoxel(d){}
    template<typename G>bool operator()(const G& grid){
        nativeCall("native.scatter");++scatterCalls;scatterMode="dense";if(perVoxel<1e-6f)return false;
        const int ppv=int(std::floor(perVoxel));(void)ppv;
        for(auto it=grid.cbeginValueOn();it;++it){const double d=it.isVoxelValue()?double(perVoxel):double(perVoxel*it.getVoxelCount());
            const int n=int(std::floor(d));this->emit(n>0?std::size_t(n):0);}return true;
    }
};
template<typename G>BoolGrid::Ptr distributionMask(const G& grid){
    nativeCall("native.interiorMask");++maskCalls;if(nullCopy)return nullptr;
    auto result=std::make_shared<BoolGrid>();result->xform=grid.xform;result->dx=grid.dx;result->bg=false;
    result->storedBox=grid.storedBox;result->activeCount=emptyInteriorMask?0:1;
    if(emptyInteriorMask){result->values.clear();result->vacant=true;}
    if(badMask)result->xform.shift[0]=10;return result;
}
template<typename G,typename I>void fillWithSpheres(const G&,std::vector<Vec4s>& out,int count,bool,
    float minimum,float maximum,float,int candidates,I*){
    nativeCall("native.packing");++packingCalls;assert(baseCalls>0);
    assert(std::size_t(std::max(count,candidates))*std::size_t(count)<=100000000);
    receivedMinimum.push_back(minimum);receivedMaximum.push_back(maximum);
    if(!noSpheres)out.emplace_back(badSphere?NAN:32,32,32,1);
    if(extraSphere)for(int i=0;i<count;++i)out.emplace_back(32,32,32,1);
}
}}
