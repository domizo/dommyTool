#include "vdb_distribution_doubles.h"
#include <list>
// These are recording host/native boundaries, not sparse surface extraction,
// reference sharpening, normals, or native OpenVDB allocation equivalence.
static int surfaceCalls, referenceCalls;
static bool badIndex, badFlags, noPoints, noPools, hugePoints, hugePools, hugeFaces;
static bool emptySurface, noPointFlags, nullReference, badReferenceMap, badReferenceValue, badReferenceIndex;
static std::vector<std::string> referenceNames;
struct MItMeshPolygon {
    std::size_t cursor=0;
    MItMeshPolygon(const MObject&,MStatus* s){*s=operation("mesh.iterator");}
    bool isDone(MStatus* s){*s=operation("mesh.done");return cursor>=sourceFaceTriangles.size();}
    MStatus next(){++cursor;return operation("mesh.next");}
    MStatus numTriangles(int& count){count=sourceFaceTriangles.at(cursor);return operation("mesh.triangles");}
};
namespace openvdb {
using Vec3s=math::Vec3<float>;
using Vec3I=math::Vec3<std::uint32_t>;
using Index32=std::uint32_t;
namespace util {const Index32 INVALID_IDX=UINT32_MAX;}
struct Vec4I {
    Index32 v[4];explicit Vec4I(Index32 n):v{n,n,n,n}{}
    Vec4I(Index32 a,Index32 b,Index32 c,Index32 d):v{a,b,c,d}{}
    Index32 operator[](int i)const{return v[i];}Index32& operator[](int i){return v[i];}
};
namespace tools {
const int UNSIGNED_DISTANCE_FIELD=1, POLYFLAG_EXTERIOR=1, POLYFLAG_FRACTURE_SEAM=2;
template<typename P,typename F>struct QuadAndTriangleDataAdapter {
    const std::vector<P>& points;
    QuadAndTriangleDataAdapter(const std::vector<P>& p,const std::vector<F>&):points(p){}
};
template<typename G,typename A>typename G::Ptr meshToVolume(const A& mesh,const math::Transform& t,float,float,int){
    nativeCall("native.referenceVolume");++referenceCalls;
    for(const auto& p:mesh.points)for(int axis=0;axis<3;++axis){volatile int index=int(std::floor(p[axis]));(void)index;}
    if(nullReference)return nullptr;
    auto result=std::make_shared<G>();result->xform=t;result->dx=t.voxelSize();
    result->storedBox.reset({32,32,32},{33,33,33});result->kind=GRID_LEVEL_SET;
    if(badReferenceMap)result->xform.shift[0]=10;
    if(badReferenceValue)result->values[0]=NAN;
    return result;
}
template<typename G,typename A,typename I>typename G::Ptr meshToVolume(util::NullInterrupter&,const A& mesh,
    const math::Transform& t,float e,float i,int flags,I* index){
    index->values={badReferenceIndex?1:0};
    return meshToVolume<G>(mesh,t,e,i,flags);
}
template<typename G>void sdfToFogVolume(G&) {nativeCall("native.fog");}
struct PolygonPool {
    Vec4I q{0,1,2,3};Vec3I t{0,1,2};
    std::size_t numQuads()const{return hugeFaces?2000001:1;}
    std::size_t numTriangles()const{return 1;}
    const Vec4I& quad(std::size_t i)const{assert(i==0);return q;}
    const Vec3I& triangle(std::size_t i)const{assert(i==0);return t;}
    int quadFlags(std::size_t)const{return POLYFLAG_EXTERIOR;}
    int triangleFlags(std::size_t)const{return POLYFLAG_FRACTURE_SEAM;}
};
struct VolumeToMesh {
    std::unique_ptr<Vec3s[]> points;std::unique_ptr<PolygonPool[]> pools;std::vector<char> flags;
    VolumeToMesh(float,float){}
    template<typename G>void operator()(const G&){
        nativeCall("native.surface");++surfaceCalls;
        points.reset(noPoints?nullptr:new Vec3s[4]);pools.reset(noPools?nullptr:new PolygonPool[1]);
        if(points){points[0]={badPoint?NAN:32,32,32};points[1]={33,32,32};points[2]={33,33,32};points[3]={32,33,32};}
        if(pools&&badIndex)pools[0].t[2]=4;
        flags.assign(emptySurface||noPointFlags?0:badFlags?3:4,1);
    }
    std::size_t pointListSize()const{return emptySurface?0:hugePoints?10000001:4;}
    std::size_t polygonPoolListSize()const{return emptySurface?0:hugePools?2000001:1;}
    std::unique_ptr<Vec3s[]>& pointList(){return points;}
    std::unique_ptr<PolygonPool[]>& polygonPoolList(){return pools;}
    const std::vector<char>& pointFlags()const{return flags;}
};
}}
MStatus DOpenVDBMakePrimList(const MObject&,std::vector<openvdb::Vec4I>& primitives,std::vector<int>* faces=nullptr){
    if(faces)faces->push_back(0);
    DOMMY_RETURN_IF_FAILURE(operation("mesh.primitives"));primitives.emplace_back(0,1,2,3);return MS::kSuccess;
}
template<typename Op>bool processTypedScalarGrid(const GridCPtr& grid,Op& op){
#define TYPE(G) if(grid->isType<openvdb::G>()){op.template operator()<openvdb::G>(openvdb::gridConstPtrCast<openvdb::G>(grid));return true;}
    TYPE(FloatGrid) TYPE(DoubleGrid) TYPE(Int32Grid) TYPE(Int64Grid)
#undef TYPE
    return false;
}
struct MArrayDataBuilder {
    MArrayDataBuilder(const MObject&,unsigned int,MStatus* s){*s=operation("array.builder");}
    DistributionHandle addElement(unsigned int,MStatus* s){
        *s=operation("array.element");DistributionHandle h(MDataHandle{InputValue(),bool(*s)});h.staged=true;return h;
    }
};
struct MArrayDataHandle {
    MStatus set(const MArrayDataBuilder&){auto s=operation("array.publish");if(s)++publications;return s;}
    MStatus setAllClean(){return operation("array.clean");}
};
struct MeshingBlock:DistributionBlock {
    MArrayDataHandle outputArrayValue(const MObject&,MStatus* s){*s=operation("array.output");return {};}
};
using DColor=MPoint;
using DColorList=std::vector<DColor>;
using DPIdList=std::vector<bool>;
struct DPVec3fAttr {DPVec3fAttr(const DColorList&,const DColor&) {}};
struct DPGroupData {explicit DPGroupData(const DPIdList&) {}};
enum {D_COMP_FACE,D_COMP_POINT};
struct DMeshInternalData;
using DMItem=std::shared_ptr<DMeshInternalData>;
struct DMeshInternalData {
    std::size_t numberOfPoints()const{return 4U;}
    int numFace()const{return 2;}
    std::size_t numberOfConnections()const{return 7U;}
    std::size_t numberOfUVs()const{return 0U;}
    DMeshInternalData()=default;
    DMeshInternalData(const MPointArray&,const MIntArray&,const MIntArray&){nativeCall("dm.construct");}
    static MStatus createFromMayaObject(const MObject& object,DMItem& result){
        assert(!object.isNull());DOMMY_RETURN_IF_FAILURE(operation("dm.fromMesh"));
        result=std::make_shared<DMeshInternalData>();return MS::kSuccess;
    }
    bool addCompAttr(int,const char*,const DPVec3fAttr&){return bool(operation("dm.color"));}
    bool addCompGrp(int,const std::string&,const DPGroupData&){return bool(operation("dm.group"));}
};
struct DMeshData:MPxData {
    using DMeshList=std::vector<DMItem>;
    static const int id=567;std::vector<DMItem> meshes;
    bool insert(const DMeshList& batch){if(!operation("dm.insert"))return false;meshes.insert(meshes.end(),batch.begin(),batch.end());return true;}
    std::size_t numberOfMesh()const{return meshes.size();}
    void updateBBox(){nativeCall("dm.bounds");}
};
const int DMeshData::id;
namespace dommy { namespace mesh {
MStatus captureCornerNormals(const MObject&,DMeshInternalData&){return operation("mesh.captureNormals");}
}}
static DMeshData stagedMeshes;
namespace dommy {namespace maya {
template<typename T>T* pluginDataPointer(MFnPluginData&,MStatus* s){
    *s=operation("plugin.pointer");return nullOutput?nullptr:&stagedMeshes;
}
}}
MStatus getPrimDataFromMesh(MFnMesh&,meshPrimData& data){
    DOMMY_RETURN_IF_FAILURE(operation("mesh.readPrimitives"));data.m_pts.values.resize(4,MPoint(32,32,32));
    data.m_faces.values={4,3};data.m_connects.values={3,2,1,0,2,1,0};return MS::kSuccess;
}
MStatus isMeshData(MeshingBlock&,const MObject&,MFnMesh&){return operation("mesh.validateReference");}
std::string getGridNames(const DOpenVDBData&){nativeCall("info.names");return "grids";}
void EchoWarning(const char*,const char*){}
#define MDataHandle DistributionHandle
#define MDataBlock MeshingBlock
#define CHECK_MSTATUS_AND_RETURN_IT DOMMY_RETURN_IF_FAILURE
#include "common/utility/DMeshMergePlan.h"
#include "vdb_meshing_compute.inc"

void setup(){
    resetCalls();inputData=DOpenVDBData();stagedMeshes=DMeshData();
    publications=auxiliaryPublications=invalidReads=invalidArrayAccesses=surfaceCalls=referenceCalls=gridCopies=meshCalls=0;
    badIndex=badFlags=noPoints=noPools=hugePoints=hugePools=hugeFaces=emptySurface=noPointFlags=false;
    nullReference=badReferenceMap=badReferenceValue=badReferenceIndex=badPoint=interrupted=false;
    nullOutput=invalidOutputPayload=nullCreatedMesh=false;meshConnected=false;socketConnections.clear();
    referenceNames.clear();createdMeshIndices.clear();
    badMeshRead=nullMeshInput=false;sourceVertexCount=4;sourcePolygonCount=1;
    sourceMeshPoints={{32,32,32},{33,32,32},{33,33,32},{32,33,32}};sourceFaceTriangles={2};
    auto a=std::make_shared<openvdb::FloatGrid>();a->name="first";a->kind=openvdb::GRID_LEVEL_SET;
    a->storedBox.reset({32,32,32},{33,33,33});inputData.grids.push_back(a);
}
MDataBlock block(bool reference=false,bool metadata=false){
    MDataBlock d;d.inputs["aVdbInput"].payload=&inputData;d.inputs["aVdbSelectedGridNames"].text="*";
    d.inputs["aIsovalue"].number=0;d.inputs["aAdaptivity"].number=0;
    d.inputs["aComputeNormals"].integer=reference;d.inputs["aExportColorSet"].integer=metadata;
    d.inputs["aExportMetadata"].integer=metadata;meshConnected=reference;
    if(reference)for(const auto* n:{"aTransferAttributes","aSharpenFeatures","aConcernAllPartsNormal","aReferenceMesh"})d.inputs[n];
    if(metadata)for(const auto* n:{"aSeamPointGroupName","aSurfaceGroupName","aInteriorGroupName","aSeamGroupName"})d.inputs[n];
    return d;
}
MStatus compute(MDataBlock& d,int mode){
    if(mode==0)return DOpenVDBToMesh().compute(MPlug(DOpenVDBToMesh::outMesh),d);
    if(mode==1)return DOpenVDBToMesh().compute(MPlug(DOpenVDBToMesh::outDM),d);
    if(mode==2)return DOpenVDBConvert().compute(MPlug(DOpenVDBConvert::outDM),d);
    if(mode==3)return DOpenVDBToMesh().compute(MPlug(DOpenVDBToMesh::aVdbAllGridNames),d);
    return DOpenVDBConvert().compute(MPlug(DOpenVDBConvert::aVdbAllGridNames),d);
}
template<typename G>bool referenceVolume(dommy::vdb::TreeEditBudget& budget,float coordinate=32){
    std::vector<openvdb::Vec3s> points{{coordinate,32,32},{coordinate+1,32,32},{coordinate,33,32}};
    std::vector<openvdb::Vec4I> faces{{0,1,2,openvdb::util::INVALID_IDX}};
    typename G::ConstPtr result;openvdb::Int32Grid::Ptr indices;openvdb::math::Transform transform;
    const bool ok=dommy::vdb::buildReferenceVolume<G>(points,faces,transform,true,budget,result,indices);
    assert(ok==bool(result)&&ok==bool(indices));return ok;
}
int main(){
    setup();auto d=block();assert(compute(d,0)&&publications==1&&surfaceCalls==1);
    assert((createdMeshIndices==std::vector<int>{3,2,1,0,2,1,0}));
    setup();dommy::vdb::MeshingBudget budget;openvdb::tools::VolumeToMesh mesher(0,0);VDBToMayaMesh converter(mesher,budget);
    assert(processTypedScalarGrid(inputData.grids[0],converter)&&converter.status&&!converter.mesh.isNull());
    assert(!mesher.points&&!mesher.pools);
    assert((converter.getSurfaceGroup()==std::vector<bool>{true,false}));
    assert((converter.getInteriorGroup()==std::vector<bool>{false,true}));
    assert((converter.getSeamGroup()==std::vector<bool>{false,true}));
    assert(converter.getSeamPointGroup()==std::vector<bool>(4,true));
    int injections=0;
    for(int scenario=0;scenario<7;++scenario){
        int mode=scenario<5?scenario:2;
        auto prepare=[&](){setup();return block(scenario==5,scenario==6);};
        d=prepare();assert(compute(d,mode));auto trace=operations;
        for(std::size_t i=0;i<trace.size();++i)for(bool exception:{false,true}){
            d=prepare();if(exception)attributeThrowAt=int(i);else failAt=int(i);
            assert(!compute(d,mode)&&!invalidReads&&!invalidArrayAccesses);
            if(publications)assert(trace[i]=="array.clean"||trace[i]=="output.handleClean"||trace[i]=="output.clean");
            ++injections;
        }
    }
    for(int mode:{0,1,2}){
        for(bool* flag:{&badIndex,&badFlags,&noPoints,&noPools,&hugePoints,&hugePools,&hugeFaces,&badPoint,&nullOutput,&nullCreatedMesh}){
            setup();d=block();*flag=true;assert(!compute(d,mode)&&!publications&&!invalidReads);
        }
        for(float value:{NAN,INFINITY}){setup();d=block();d.inputs["aIsovalue"].number=value;assert(!compute(d,mode)&&!surfaceCalls);}
        for(float value:{NAN,-1.f,2.f}){setup();d=block();d.inputs["aAdaptivity"].number=value;assert(!compute(d,mode)&&!surfaceCalls);}
        setup();d=block();inputData.valid=false;assert(!compute(d,mode)&&!surfaceCalls);
        setup();d=block();inputData.grids.push_back(nullptr);assert(!compute(d,mode)&&!surfaceCalls);
        setup();d=block();auto g=std::make_shared<openvdb::DoubleGrid>();g->values[0]=NAN;inputData.grids.push_back(g);
        assert(!compute(d,mode)&&!surfaceCalls); // No earlier valid grid is meshed first.
        setup();d=block();auto source=std::const_pointer_cast<openvdb::GridBase>(inputData.grids[0]);source->activeCount=10000001;
        assert(!compute(d,mode)&&!surfaceCalls);
    }
    for(int mode:{0,1}){setup();d=block();emptySurface=true;assert(compute(d,mode)&&!meshCalls&&publications==1);}
    setup();d=block();noPointFlags=true;assert(compute(d,0));
    for(int mode:{1,2}) for(std::size_t count:{2U,4U}) {
        setup();d=block(false,true);
        const auto grid=inputData.grids.front();
        inputData.grids.assign(count,grid);
        const auto result=compute(d,mode);
        if(!result||publications!=1||stagedMeshes.meshes.size()!=count)
            std::cerr<<"Batch fixture mode="<<mode<<" count="<<count<<" ok="<<bool(result)
                     <<" publications="<<publications<<" meshes="<<stagedMeshes.meshes.size()<<"\n";
        assert(result&&publications==1&&stagedMeshes.meshes.size()==count);
        assert(std::count(operations.begin(),operations.end(),"dm.insert")==1);
    }
    for(int mode:{1,2}) {
        setup();d=block();const auto grid=inputData.grids.front();
        inputData.grids.assign(100U,grid);
        assert(!compute(d,mode)&&!surfaceCalls&&!publications);
    }
    setup();d=block(true);auto second=std::make_shared<openvdb::DoubleGrid>();second->name="second";second->kind=openvdb::GRID_LEVEL_SET;
    second->storedBox.reset({32,32,32},{33,33,33});inputData.grids.push_back(second);d.inputs["aVdbSelectedGridNames"].text="second";
    assert(compute(d,2)&&referenceNames==std::vector<std::string>{"second"}&&!gridCopies);
    setup();d=block(true);d.inputs["aVdbSelectedGridNames"].text="missing";assert(!compute(d,2)&&referenceNames.empty()&&!publications);
    setup();d=block();d.inputs["aVdbSelectedGridNames"].text="missing";assert(compute(d,0)&&!surfaceCalls);
    setup();d=block();inputData.grids={std::make_shared<openvdb::GridBase>()};assert(compute(d,0)&&!surfaceCalls);
    for(bool doubleGrid:{false,true}){setup();dommy::vdb::TreeEditBudget b;
        assert(doubleGrid?referenceVolume<openvdb::DoubleGrid>(b):referenceVolume<openvdb::FloatGrid>(b));}
    for(float coordinate:{NAN,INFINITY,1e10f,1000001.f}){setup();dommy::vdb::TreeEditBudget b;
        assert(!referenceVolume<openvdb::FloatGrid>(b,coordinate)&&!referenceCalls);}
    for(bool* flag:{&nullReference,&badReferenceMap,&badReferenceValue,&badReferenceIndex,&interrupted}){
        setup();*flag=true;dommy::vdb::TreeEditBudget b;assert(!referenceVolume<openvdb::FloatGrid>(b));}
    setup();dommy::vdb::TreeEditBudget exhausted;exhausted.addWork(100000000,1);
    assert(!referenceVolume<openvdb::FloatGrid>(exhausted)&&!referenceCalls);
    setup();dommy::vdb::MeshingBudget outputBudget;
    assert(!outputBudget.addOutput(10000001,0,0));
    for(float coordinate:{NAN,INFINITY,1e10f,1000001.f}){
        setup();d=block(true);for(auto& p:sourceMeshPoints)p.x=coordinate;
        assert(!compute(d,2)&&!referenceCalls&&!publications);
    }
    for(bool* flag:{&badMeshRead,&nullMeshInput}){setup();d=block(true);*flag=true;
        assert(!compute(d,2)&&!referenceCalls&&!publications);}
    setup();d=block();auto large=std::make_shared<openvdb::FloatGrid>();large->storedBox.reset({0,0,0},{1000,1000,1000});
    inputData.grids={large};assert(!compute(d,0)&&!surfaceCalls);
    std::cout<<"ToMesh/Convert: "<<injections<<" failure/exception injections; selection, staging, topology and reference intake passed\n";
}
