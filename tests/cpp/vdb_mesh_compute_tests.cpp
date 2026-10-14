#include "vdb_distribution_doubles.h"
#include "common/string/DStreamFormat.h"
#include <array>
// Real production control flow; these boundaries do not implement Maya mesh
// triangulation, native sparse voxelization or fog/distribution equivalence.
static int conversionCalls, fogCalls;
static bool emptyConversion, nullConversion, badConversionMap, badConversionValue, badFog, badPrimitive;
static float receivedInterior;
static int receivedFlags;
struct MItMeshPolygon {
    std::size_t cursor=0;
    MItMeshPolygon(const MObject&,MStatus* s){*s=operation("mesh.iterator");}
    bool isDone(MStatus* s){*s=operation("mesh.done");return cursor>=sourceFaceTriangles.size();}
    MStatus next(){++cursor;return operation("mesh.next");}
    MStatus numTriangles(int& count){count=sourceFaceTriangles.at(cursor);return operation("mesh.triangles");}
};
namespace openvdb {
using Vec3s=math::Vec3<float>;
using Index32=std::uint32_t;
namespace util {const Index32 INVALID_IDX=UINT32_MAX;}
struct Vec4I {
    Index32 v[4];explicit Vec4I(Index32 n):v{n,n,n,n}{}
    Vec4I(Index32 a,Index32 b,Index32 c,Index32 d):v{a,b,c,d}{}
    Index32 operator[](int i)const{return v[i];}Index32& operator[](int i){return v[i];}
};
namespace tools {
const int UNSIGNED_DISTANCE_FIELD=1;
template<typename P,typename F>struct QuadAndTriangleDataAdapter {
    const std::vector<P>& points;
    QuadAndTriangleDataAdapter(const std::vector<P>& p,const std::vector<F>&):points(p){}
};
template<typename G,typename A>typename G::Ptr meshToVolume(const A& mesh,const math::Transform& t,float e,float i,int flags){
    nativeCall("native.meshVolume");++conversionCalls;receivedInterior=i;receivedFlags=flags;
    // Reproduce the native float-to-signed-coordinate boundary under UBSan.
    for(const auto& p:mesh.points)for(int axis=0;axis<3;++axis){volatile int index=int(std::floor(p[axis]));(void)index;}
    if(nullConversion)return nullptr;
    auto g=std::make_shared<G>();g->xform=t;g->dx=t.voxelSize();g->bg=e*g->dx[0];g->activeCount=1;
    g->kind=flags?GRID_UNKNOWN:GRID_LEVEL_SET;g->storedBox.reset({32,32,32},{33,33,33});
    if(emptyConversion){g->vacant=true;g->values.clear();g->activeCount=0;}
    if(badConversionMap)g->xform.shift[0]=1;
    if(badConversionValue)g->values[0]=NAN;
    return g;
}
template<typename G>void sdfToFogVolume(G& grid){
    assert(!grid.empty());nativeCall("native.fog");++fogCalls;grid.bg=0;grid.kind=GRID_FOG_VOLUME;
    if(badFog)grid.values[0]=NAN;
}
}}
MStatus DOpenVDBMakePrimList(const MObject&,std::vector<openvdb::Vec4I>& primitives){
    DOMMY_RETURN_IF_FAILURE(operation("mesh.primitives"));
    primitives.emplace_back(0,1,badPrimitive?3:2,openvdb::util::INVALID_IDX);return MS::kSuccess;
}
struct CustomMesh {
    DPointList points;
    const DPointList* getPositionPtrConst() const{return &points;}
};
struct DMeshData:MPxData {
    std::vector<std::shared_ptr<CustomMesh>> meshes;
    bool valid=true;
    bool getCheckedTotalVertex(std::size_t& n)const{n=0;for(const auto& m:meshes)if(m)n+=m->points.size();return valid;}
    bool getCheckedTotalFace(std::size_t& n)const{n=meshes.size();return valid;}
    bool getCheckedTotalConnect(std::size_t& n)const{n=meshes.size()*3;return valid;}
    auto beginMeshes()const->decltype(meshes.begin()){return meshes.begin();}
    auto endMeshes()const->decltype(meshes.end()){return meshes.end();}
};
static DMeshData customData;
enum class DMeshPointSpace {Transformed};
MStatus generateMMeshFromDmeshWithUV(const CustomMesh& mesh,MObject& out,DMeshPointSpace){
    DOMMY_RETURN_IF_FAILURE(operation("mesh.materialize"));out={"custom"};sourceMeshPoints=mesh.points;
    sourceVertexCount=int(sourceMeshPoints.size());return MS::kSuccess;
}
MStatus getValidInputDM(const MObject& attr,DistributionBlock& data,const DMeshData*& result){
    MStatus s;const auto h=data.inputValue(attr,&s);DOMMY_RETURN_IF_FAILURE(s);
    result=dynamic_cast<const DMeshData*>(h.asPluginData());return result&&result->valid?MS::kSuccess:MS::kFailure;
}
struct Timer {double elapsedTime()const{return 0;}};
void printGridInfo(std::ostream& out,const DOpenVDBData&){nativeCall("info.build");out<<"grid info";}
namespace dommy {namespace maya {
MStatus getEvaluationTime(DistributionBlock&,MTime&){return operation("evaluation.time");}
}}
#define MDataHandle DistributionHandle
#define MDataBlock DistributionBlock
#define CHECK_MSTATUS_AND_RETURN_IT DOMMY_RETURN_IF_FAILURE
#define sdfInteriorMask distributionMask
#include "vdb_mesh_compute.inc"
#undef sdfInteriorMask

void setup(){
    createEmptyGrid=true;
    resetCalls();stagedData=DOpenVDBData();outputData=DOpenVDBData();stagedPoints=DPData();
    publications=auxiliaryPublications=conversionCalls=fogCalls=scatterCalls=gridCopies=maskCalls=invalidReads=0;
    emptyConversion=nullConversion=badConversionMap=badConversionValue=badFog=badPrimitive=false;
    nullCopy=wrongCopy=nullOutput=invalidOutputPayload=badMeshRead=nullMeshInput=badPoint=extraPoint=interrupted=badMask=emptyInteriorMask=false;
    sourceVertexCount=3;sourcePolygonCount=1;sourceMeshPoints={{32,32,32},{33,32,32},{32,33,32}};sourceFaceTriangles={1};
    meshConnected=true;socketConnections.clear();customData=DMeshData();
    auto m=std::make_shared<CustomMesh>();m->points=sourceMeshPoints;customData.meshes={m};
}
MDataBlock fromBlock(int mode=1,bool custom=false){
    MDataBlock d;d.inputs["aExportDistanceGrid"].integer=mode&1;d.inputs["aExportDensityGrid"].integer=bool(mode&2);
    d.inputs["aVoxelSize"].number=1;d.inputs["aExteriorBandWidth"].number=3;d.inputs["aInteriorBandWidth"].number=3;
    d.inputs["aFillInterior"].integer=0;d.inputs["aUnsignedDistanceField"].integer=0;
    d.inputs["aDistanceGridName"].text="surface";d.inputs["aDensityGridName"].text="density";
    d.inputs["aMeshInput"];d.inputs["inDM"].payload=&customData;
    socketConnections["aMeshInput"]=!custom;socketConnections["inDM"]=custom;return d;
}
MDataBlock emitBlock(int mode=0,bool local=false,bool interior=false){
    auto d=fromBlock();d.inputs["emitType"].integer=D_VOLUME_EMIT_MESH;d.inputs["aCount"].wideInteger=2;
    d.inputs["radius"].number=1;d.inputs["volumeSize"];d.inputs["aBoxGrid"];d.inputs["nSeed"].integer=1;
    d.inputs["aMode"].integer=mode;d.inputs["aSpread"].number=1;d.inputs["aInterior"].integer=interior;
    if(mode==1){d.inputs["aDensity"].number=1;d.inputs["aIsLocal"].integer=local;}
    if(mode==2)d.inputs["aPerVoxel"].number=2;
    d.inputs["inputGeometry"];d.inputs["aVoxelType"].integer=0;return d;
}
MStatus fromCompute(MDataBlock& d){return DOpenVDBFromMesh().compute(MPlug(DOpenVDBFromMesh::aVdbOutput),d);}
MStatus estimateCompute(MDataBlock& d){return DOpenVDBFromMesh().compute(MPlug(DOpenVDBFromMesh::aEstimatedGridResolution),d);}
MStatus emitCompute(MDataBlock& d){return DPointVolumeEmit().compute(MPlug(DPointVolumeEmit::outDP),d);}
void reject(MDataBlock& d,bool emit=false,bool beforeNative=true){
    assert(!(emit?emitCompute(d):fromCompute(d))&&!publications&&!invalidReads);
    if(beforeNative)assert(!conversionCalls&&!scatterCalls);
}
int main(){
    for(float value:{0.f,-1.f,NAN,INFINITY,1e-9f,1e9f}){
        dommy::vdb::MeshConversionSettings s;s.voxelSize=value;assert(!dommy::vdb::validMeshConversionSettings(s));}
    setup();auto d=fromBlock(3);assert(fromCompute(d)&&publications==1&&auxiliaryPublications==1&&fogCalls==1);
    assert(outputData.grids.size()==2&&outputData.grid(0).getName()=="surface"&&outputData.grid(1).getName()=="density");
    assert(outputData.grid(0).getGridClass()==openvdb::GRID_LEVEL_SET&&lastMeshSpace==MSpace::kObject);
    setup();d=emitBlock();assert(emitCompute(d)&&stagedPoints.points.size()==2&&lastMeshSpace==MSpace::kWorld);
    int injected=0;
    for(int scenario=0;scenario<12;++scenario){
        auto prepare=[&](){setup();if(scenario<4)return fromBlock(scenario);
            if(scenario==4)return fromBlock(3,true);if(scenario==5)return fromBlock();
            if(scenario==6)return fromBlock(1,true);return emitBlock(scenario==10?2:scenario==7?0:1,scenario==9,scenario==11);};
        auto compute=scenario==5||scenario==6?estimateCompute:scenario<5?fromCompute:emitCompute;
        d=prepare();assert(compute(d));const auto trace=operations;
        for(std::size_t i=0;i<trace.size();++i)for(bool exception:{false,true}){
            d=prepare();if(exception)attributeThrowAt=int(i);else failAt=int(i);
            assert(!compute(d)&&invalidReads==0);
            // Auxiliary setters and cleaning may fail after a complete VDB commit.
            if(publications)assert(trace[i]=="output.handleClean"||trace[i]=="info.set"||trace[i]=="output.clean");
            ++injected;
        }
    }
    for(bool emit:{false,true}){
        for(float v:{0.f,-1.f,NAN,INFINITY,1e-9f,1e9f}){setup();d=emit?emitBlock():fromBlock();d.inputs["aVoxelSize"].number=v;reject(d,emit);}
        for(float v:{NAN,INFINITY,1e10f}){setup();d=emit?emitBlock():fromBlock();sourceMeshPoints[0].x=v;reject(d,emit);}
        setup();d=emit?emitBlock():fromBlock();for(auto& p:sourceMeshPoints)p.x=1e10;reject(d,emit);
        setup();d=emit?emitBlock():fromBlock();for(auto& p:sourceMeshPoints)p.x=1000002;reject(d,emit);
        setup();d=emit?emitBlock():fromBlock();sourceMeshPoints[1].x=1000;reject(d,emit);
        for(int n:{-1,0,10000001}){setup();d=emit?emitBlock():fromBlock();sourceVertexCount=n;reject(d,emit);}
        setup();d=emit?emitBlock():fromBlock();sourceFaceTriangles[0]=2000001;reject(d,emit);
        for(bool* flag:{&badPrimitive,&badMeshRead,&nullMeshInput}){setup();d=emit?emitBlock():fromBlock();*flag=true;reject(d,emit);}
        for(bool* flag:{&nullConversion,&badConversionMap,&badConversionValue}){setup();d=emit?emitBlock():fromBlock();*flag=true;reject(d,emit,false);}
    }
    setup();d=fromBlock(0);d.inputs.erase("aVoxelSize");d.inputs.erase("aMeshInput");assert(fromCompute(d)&&!conversionCalls&&outputData.grids.empty());
    setup();d=fromBlock();d.inputs["aUnsignedDistanceField"].integer=1;d.inputs.erase("aFillInterior");d.inputs.erase("aInteriorBandWidth");assert(fromCompute(d)&&receivedFlags==1);
    setup();d=fromBlock();d.inputs["aFillInterior"].integer=1;d.inputs.erase("aInteriorBandWidth");assert(fromCompute(d)&&receivedInterior==std::numeric_limits<float>::max());
    setup();d=fromBlock(3);emptyConversion=true;assert(fromCompute(d)&&!fogCalls&&outputData.grid(1).empty());
    for(bool* flag:{&nullCopy,&wrongCopy,&badFog,&nullOutput,&invalidOutputPayload}){setup();d=fromBlock(3);*flag=true;reject(d,false,false);}
    setup();d=fromBlock();socketConnections["aMeshInput"]=false;reject(d);
    setup();d=fromBlock(3,true);customData.valid=false;reject(d);
    setup();d=fromBlock(1,true);customData.meshes[0].reset();reject(d);
    setup();d=fromBlock();sourceMeshPoints[1].x=1e8;d.inputs["aVoxelSize"].number=.001f;assert(!estimateCompute(d)&&!auxiliaryPublications);
    setup();d=fromBlock();sourceVertexCount=0;sourceMeshPoints.clear();assert(estimateCompute(d)&&auxiliaryPublications==1);
    setup();d=fromBlock();sourceMeshPoints[0].x=NAN;assert(!estimateCompute(d)&&!auxiliaryPublications);
    setup();d=emitBlock();d.inputs["aCount"].wideInteger=0;d.inputs.erase("aMode");assert(emitCompute(d)&&!conversionCalls&&stagedPoints.points.empty());
    for(int mode:{-1,3}){setup();d=emitBlock();d.inputs["aMode"].integer=mode;reject(d,true);}
    for(float v:{NAN,INFINITY,-1.f,2.f}){setup();d=emitBlock();d.inputs["aSpread"].number=v;reject(d,true);}
    for(int mode:{1,2})for(float v:{NAN,INFINITY,-1.f,1e10f}){
        setup();d=emitBlock(mode);d.inputs[mode==1?"aDensity":"aPerVoxel"].number=v;reject(d,true,!std::isfinite(v)||v<0);assert(!scatterCalls);}
    setup();d=emitBlock(1,true);d.inputs["aDensity"].number=1e9;reject(d,true,false);assert(!scatterCalls);
    setup();d=emitBlock();emptyConversion=true;reject(d,true,false);
    for(bool* flag:{&badPoint,&extraPoint,&interrupted,&nullOutput}){setup();d=emitBlock();*flag=true;reject(d,true,false);}
    std::cout<<"Mesh conversion/FromMesh/emitter: "<<injected<<" failure/exception injections, bounds, budgets, modes and staging cases passed\n";
}
