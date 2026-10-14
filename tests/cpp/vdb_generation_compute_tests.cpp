#include "vdb_clip_doubles.h"
#include <list>
#include <set>
#include <random>
// Recording boundaries, not native sparse geometry, quaternion or CSG kernels.
static int sphereCalls, fogCalls, meshCalls, fractureCalls, segmentCalls;
static bool nullSphere, badSphere, badFog, nullCutter, badCutter, badPiece, duplicatePiece;
static bool emptyCutter, badCutterMap, badCutterBounds, nullPiece, badSegment, growPieces, interrupted;
static int segmentCount = 1, meshVertexCount = 3, meshPolygonCount = 1;
static std::vector<MPoint> meshVertices;
static std::vector<bool> receivedOverlap, receivedRotations;
static std::vector<float> receivedPoints;
namespace MSpace {enum Space{kObject};}
struct MPointArray {
    std::vector<MPoint> values;
    unsigned int length() const {return unsigned(values.size());}
    const MPoint& operator[](unsigned int i) const {return values.at(i);}
};
struct MFnMesh {
    MFnMesh(const MObject&, MStatus* status) {*status = operation("mesh.attach");}
    int numVertices(MStatus* status) const {*status = operation("mesh.vertexCount"); return meshVertexCount;}
    int numPolygons(MStatus* status) const {*status = operation("mesh.polygonCount"); return meshPolygonCount;}
    MStatus getPoints(MPointArray& result, MSpace::Space) const {
        const auto status = operation("mesh.points"); result.values = meshVertices; return status;
    }
};
struct DPData : MPxData {
    static const int id = 456;
    bool valid = true;
    std::vector<openvdb::Vec3d> points;
    bool hasValidPointTopology() const {return valid;}
    std::size_t numberOfPoints() const {return points.size();}
    const openvdb::Vec3d& positionAt(std::size_t i) const {return points.at(i);}
};
const int DPData::id;
namespace boost {
using mt19937 = std::mt19937;
template<typename Generator, typename T>struct uniform_01 {
    Generator& generator;
    explicit uniform_01(Generator& g):generator(g){}
    T operator()(){return T(double(generator())/(double(Generator::max())+1));}
};
namespace math {namespace constants {template<typename T>T pi(){return T(std::acos(-1.0));}}}
}
namespace openvdb {
using Vec3f = math::Vec3<float>;
using Vec3s = Vec3f;
struct Vec4I {};
struct FloatMetadata {float value;explicit FloatMetadata(float n):value(n){}};
namespace util {struct NullInterrupter{bool wasInterrupted()const{return interrupted;}};}
namespace math {
enum Axis {X_AXIS,Y_AXIS,Z_AXIS}; enum RotationOrder{XYZ_ROTATION};
struct Quats {
    float values[4] = {0,0,0,1};
    void init(float a,float b,float c,float d){values[0]=a;values[1]=b;values[2]=c;values[3]=d;}
    Vec3s eulerAngles(RotationOrder)const{nativeCall("rotation.euler"); return Vec3s(0);}
};
template<typename T>bool isApproxEqual(T a,T b){return std::fabs(a-b)<T(1e-7);}
}
template<typename G>typename G::Ptr gridPtrCast(const GridBase::Ptr& p){return std::dynamic_pointer_cast<G>(p);}
template<typename G>typename G::ConstPtr gridConstPtrCast(const GridBase::ConstPtr& p){return std::dynamic_pointer_cast<const G>(p);}
namespace tools {
template<typename G,typename I>typename G::Ptr createLevelSetSphere(float radius,const Vec3f&,float voxel,float width,I*){
    nativeCall("native.sphere");++sphereCalls;
    assert(radius>0 && voxel>0 && width>1);
    if(nullSphere)return nullptr;
    auto grid=std::make_shared<G>();grid->bg=width*voxel;grid->dx=Vec3d(voxel);
    grid->xform=*math::Transform::createLinearTransform(voxel);grid->kind=GRID_LEVEL_SET;
    if(radius/voxel<1.5f){grid->values.clear();grid->vacant=true;}
    if(badSphere)grid->values={InvalidFillValue<typename G::ValueType>::get()};
    return grid;
}
template<typename G>void sdfToFogVolume(G& grid){assert(!grid.empty());nativeCall("native.fog");++fogCalls;if(badFog)grid.bg=NAN;}
template<typename P,typename F>struct QuadAndTriangleDataAdapter {
    QuadAndTriangleDataAdapter(const std::vector<P>&,const std::vector<F>&){}
};
template<typename G,typename A>typename G::Ptr meshToVolume(util::NullInterrupter&,const A&,
    const math::Transform& transform,float outer,float inner){
    nativeCall("native.mesh");++meshCalls;assert(outer>0 && outer==inner && outer<=64);
    if(nullCutter)return nullptr;
    auto grid=std::make_shared<G>();grid->xform=transform;grid->dx=transform.voxelSize();
    grid->bg=typename G::ValueType(outer*grid->dx[0]);grid->kind=GRID_LEVEL_SET;
    grid->storedBox.reset({32,32,32},{33,33,33});grid->activeCount=emptyCutter?0:1;
    if(badCutterMap)grid->xform.postTranslate(Vec3d(1));
    if(badCutterBounds)grid->storedBox.reset({10000000,0,0},{10000001,1,1});
    if(badCutter)grid->values[0]=InvalidFillValue<typename G::ValueType>::get();
    return grid;
}
template<typename G,typename I>struct LevelSetFracture {
    using List=std::list<typename G::Ptr>;
    List pieces;
    explicit LevelSetFracture(I*){}
    List& fragments(){return pieces;}
    void fracture(List& residuals,const G&,bool segment,const std::vector<Vec3s>*points,
                  const std::vector<math::Quats>*rotations,bool overlap){
        nativeCall("native.fracture");++fractureCalls;assert(!segment);
        assert(!points||points->size()==1);assert(!rotations||rotations->size()==1);
        receivedOverlap.push_back(overlap);receivedRotations.push_back(bool(rotations));
        if(points)receivedPoints.push_back((*points)[0][0]);
        List additional;
        if(growPieces){
            if(overlap)for(const auto& p:pieces)additional.push_back(std::make_shared<G>(*p));
            for(const auto& p:residuals)additional.push_back(std::make_shared<G>(*p));
        }
        pieces.splice(pieces.end(),additional);
        if(badPiece)residuals.front()->values[0]=InvalidFillValue<typename G::ValueType>::get();
        if(duplicatePiece)pieces.push_back(residuals.front());
        if(nullPiece)pieces.push_back(nullptr);
    }
};
template<typename G>void segmentSDF(const G& grid,std::vector<typename G::Ptr>& out){
    nativeCall("native.segment");++segmentCalls;
    for(int i=0;i<segmentCount;++i)out.push_back(std::make_shared<G>(grid));
    if(badSegment&&!out.empty())out[0]->values[0]=InvalidFillValue<typename G::ValueType>::get();
}
}}
static bool emptyPrimitives;
inline MStatus DOpenVDBMakePrimList(const MObject&,std::vector<openvdb::Vec4I>& output){
    const auto status=operation("mesh.primitives");if(!emptyPrimitives)output.resize(1);return status;
}

#include "vdb_generation_compute.inc"

static std::shared_ptr<openvdb::FloatGrid> source;
static DPData pointData;
void setup(){
    resetCalls();inputData=DOpenVDBData();stagedData=DOpenVDBData();outputData=DOpenVDBData();pointData=DPData();
    source=std::make_shared<openvdb::FloatGrid>();source->kind=openvdb::GRID_LEVEL_SET;
    source->storedBox.reset({32,32,32},{33,33,33});inputData.grids={source};
    meshVertices={{32,32,32},{33,32,32},{32,33,33}};meshVertexCount=3;meshPolygonCount=1;
    meshBounds.low={32,32,32};meshBounds.high={33,33,33};meshValid=true;meshConnected=false;socketConnections.clear();
    publications=gridCopies=invalidReads=sphereCalls=fogCalls=meshCalls=fractureCalls=segmentCalls=0;
    nullCopy=wrongCopy=nullOutput=invalidOutputPayload=false;
    nullSphere=badSphere=badFog=nullCutter=badCutter=badPiece=duplicatePiece=emptyCutter=nullPiece=badSegment=false;
    growPieces=interrupted=emptyPrimitives=badCutterMap=badCutterBounds=false;segmentCount=1;
    receivedOverlap.clear();receivedRotations.clear();receivedPoints.clear();
}
MDataBlock sphereBlock(){
    MDataBlock d;
    d.inputs["scalarRadius"].number=1;d.inputs["voxelSize"].number=.1f;d.inputs["halfWidth"].number=3;
    d.inputs["center"]=InputValue();d.inputs["solidType"].integer=0;d.inputs["gridclass"].integer=0;
    d.inputs["fogVolume"].integer=0;return d;
}
MDataBlock fractureBlock(){
    MDataBlock d;d.inputs["aVdbInput"].payload=&inputData;d.inputs["aVdbSelectedGridNames"].text="*";
    d.inputs["aSegmentFragments"].integer=0;d.inputs["inDP"].payload=&pointData;d.inputs["inDP"].type=DPData::id;
    d.inputs["aCutterOverlap"].integer=0;d.inputs["aCenterCutter"].integer=0;
    d.inputs["aRandomizeRotation"].integer=0;d.inputs["aSeed"].integer=1;return d;
}
MStatus sphereCompute(MDataBlock& d){return DOpenVDBPlatonic().compute(MPlug(DOpenVDBPlatonic::aVdbOutput),d);}
MStatus fractureCompute(MDataBlock& d){return DOpenVDBFracture().compute(MPlug(DOpenVDBFracture::aVdbOutput),d);}
void rejectSphere(MDataBlock& d,bool early=true){assert(!sphereCompute(d)&&!publications&&!invalidReads);if(early)assert(!sphereCalls);}
void rejectFracture(MDataBlock& d,bool early=true){assert(!fractureCompute(d)&&!publications&&!invalidReads);if(early)assert(!meshCalls&&!fractureCalls&&!gridCopies);}
int main(){
    {
        openvdb::CoordBBox small;small.reset({32,32,32},{33,33,33});
        dommy::vdb::TreeEditBudget budget;
        assert(dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(small,budget,1,32,32));
        assert(budget.bytes>0&&budget.work>0);
        assert(!dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(small,budget,0,32,32));
        assert(!dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(small,budget,4097,32,32));
        budget.bytes=dommy::vdb::kMaxVdbPayloadBytes/4;
        assert(!dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(small,budget,1,32,32));
        budget=dommy::vdb::TreeEditBudget();budget.work=dommy::generation::kMaxWorkItemCount;
        assert(!dommy::vdb::chargeDenseTreeRegion<openvdb::FloatGrid>(small,budget,1,32,32));
    }
    setup();auto d=sphereBlock();assert(sphereCompute(d)&&publications==1&&sphereCalls==1);
    assert(outputData.grids[0]->getName()=="LevelSetSphere"&&outputData.grids[0]->metadata.at("radius")==1);
    setup();d=fractureBlock();assert(fractureCompute(d)&&publications==1&&meshCalls==1&&fractureCalls==1);
    assert(outputData.grids[0]->getName()=="source_0"&&source->getName()=="source"&&source->values[0]==1);
    int injected=0;
    for(int scenario=0;scenario<6;++scenario){
        auto prepare=[&](){
            setup();auto b=(scenario<2||scenario==5)?sphereBlock():fractureBlock();
            if(scenario==1)b.inputs["fogVolume"].integer=1;
            if(scenario==3)b.inputs["aSegmentFragments"].integer=1;
            if(scenario==4){meshConnected=true;pointData.points={openvdb::Vec3d(0)};
                b.inputs["aRandomizeRotation"].integer=1;b.inputs["aCutterOverlap"].integer=1;}
            if(scenario==5){b.inputs["scalarRadius"].number=.01f;b.inputs["fogVolume"].integer=1;}
            return b;
        };
        auto compute=(scenario<2||scenario==5)?sphereCompute:fractureCompute;
        d=prepare();assert(compute(d));const auto trace=operations;
        for(std::size_t i=0;i<trace.size();++i)for(bool exception:{false,true}){
            d=prepare();if(exception)attributeThrowAt=int(i);else failAt=int(i);
            assert(!compute(d)&&invalidReads==0);
            assert(publications==(trace[i]=="output.clean"?1:0));
            assert(source->getName()=="source"&&source->values[0]==1);++injected;
        }
    }
    for(const auto& name:{"scalarRadius","voxelSize","halfWidth"})for(float v:{0.f,-1.f,NAN,INFINITY}){
        setup();d=sphereBlock();d.inputs[name].number=v;rejectSphere(d);
    }
    setup();d=sphereBlock();d.inputs["halfWidth"].number=1;rejectSphere(d);
    setup();d=sphereBlock();d.inputs["halfWidth"].number=65;rejectSphere(d);
    setup();d=sphereBlock();d.inputs["scalarRadius"].number=1e8f;d.inputs["voxelSize"].number=.0001f;rejectSphere(d);
    setup();d=sphereBlock();d.inputs["scalarRadius"].number=10;d.inputs["voxelSize"].number=.0001f;rejectSphere(d);
    for(float v:{NAN,INFINITY,1e10f,2e5f,-2e5f}){setup();d=sphereBlock();d.inputs["center"].floats[0]=v;rejectSphere(d);}
    for(int v:{-1,1}){setup();d=sphereBlock();d.inputs["solidType"].integer=v;rejectSphere(d);}
    for(int v:{-1,4}){setup();d=sphereBlock();d.inputs["gridclass"].integer=v;rejectSphere(d);}
    for(int v=0;v<4;++v){setup();d=sphereBlock();d.inputs["gridclass"].integer=v;assert(sphereCompute(d));}
    setup();d=sphereBlock();d.inputs["scalarRadius"].number=.01f;assert(sphereCompute(d)&&outputData.grids[0]->empty());
    setup();d=sphereBlock();d.inputs["scalarRadius"].number=.01f;d.inputs["fogVolume"].integer=1;
    assert(sphereCompute(d)&&fogCalls==0&&outputData.grids[0]->getGridClass()==openvdb::GRID_FOG_VOLUME);
    assert(std::dynamic_pointer_cast<const openvdb::FloatGrid>(outputData.grids[0])->background()==0);
    for(bool* flag:{&nullSphere,&badSphere,&nullOutput,&invalidOutputPayload}){
        setup();d=sphereBlock();*flag=true;rejectSphere(d,false);
    }
    setup();d=sphereBlock();d.inputs["fogVolume"].integer=1;badFog=true;rejectSphere(d,false);
    for(bool* flag:{&nullCopy,&wrongCopy,&nullCutter,&emptyCutter,&badCutter,&badCutterMap,&badCutterBounds,&badPiece,&duplicatePiece,&nullPiece,
                    &interrupted,&nullOutput,&invalidOutputPayload}){
        setup();d=fractureBlock();*flag=true;rejectFracture(d,false);assert(source->values[0]==1);
    }
    setup();d=fractureBlock();meshValid=false;rejectFracture(d);
    setup();d=fractureBlock();meshVertexCount=0;rejectFracture(d);
    setup();d=fractureBlock();meshVertexCount=1000000000;rejectFracture(d);
    setup();d=fractureBlock();meshPolygonCount=0;rejectFracture(d);
    setup();d=fractureBlock();meshVertexCount=4;rejectFracture(d);
    setup();d=fractureBlock();emptyPrimitives=true;rejectFracture(d);
    for(double v:{1e10,1e100,double(NAN)}){setup();d=fractureBlock();meshVertices[0].x=v;rejectFracture(d);}
    setup();d=fractureBlock();source->values[0]=NAN;rejectFracture(d);
    setup();d=fractureBlock();source->values[0]=1e20f;rejectFracture(d);
    setup();d=fractureBlock();source->bg=65;rejectFracture(d);
    setup();d=fractureBlock();source->xform.uniform=false;rejectFracture(d);
    setup();d=fractureBlock();source->storedBox.reset({10000000,0,0},{10000001,1,1});rejectFracture(d);
    setup();d=fractureBlock();source->memory=dommy::vdb::kMaxVdbPayloadBytes;rejectFracture(d);
    setup();d=fractureBlock();auto late=std::make_shared<openvdb::FloatGrid>(*source);late->values[0]=NAN;
    inputData.grids.push_back(late);rejectFracture(d);
    setup();d=fractureBlock();inputData.valid=false;rejectFracture(d);
    setup();d=fractureBlock();meshConnected=true;pointData.valid=false;rejectFracture(d);
    setup();d=fractureBlock();meshConnected=true;d.inputs["inDP"].type=99;rejectFracture(d);
    setup();d=fractureBlock();meshConnected=true;pointData.points.resize(10001);rejectFracture(d);
    setup();d=fractureBlock();meshConnected=true;pointData.points={openvdb::Vec3d(1e10)};rejectFracture(d);
    setup();d=fractureBlock();meshConnected=true;pointData.points={openvdb::Vec3d(0)};
    d.inputs["aRandomizeRotation"].integer=1;d.inputs["aSeed"].integer=-1;rejectFracture(d);
    // Disconnected/empty point inputs do not consume irrelevant settings.
    for(bool connected:{false,true}){
        setup();d=fractureBlock();meshConnected=connected;
        d.inputs.erase("aRandomizeRotation");d.inputs.erase("aCenterCutter");d.inputs.erase("aCutterOverlap");d.inputs.erase("aSeed");
        if(!connected)d.inputs.erase("inDP");assert(fractureCompute(d));
    }
    setup();d=fractureBlock();meshConnected=true;pointData.points={openvdb::Vec3d(0),openvdb::Vec3d(1)};
    d.inputs.erase("aSeed");assert(fractureCompute(d)&&fractureCalls==2&&receivedPoints==std::vector<float>({0,1}));
    setup();d=fractureBlock();meshConnected=true;pointData.points={openvdb::Vec3d(0),openvdb::Vec3d(0)};
    d.inputs["aRandomizeRotation"].integer=1;d.inputs["aCutterOverlap"].integer=1;growPieces=true;
    assert(fractureCompute(d)&&outputData.grids.size()==4);
    assert(receivedOverlap==std::vector<bool>({true,true})&&receivedRotations==std::vector<bool>({true,true}));
    for(std::size_t i=0;i<4;++i)assert(outputData.grids[i]->getName()=="source_"+std::to_string(i));
    setup();d=fractureBlock();d.inputs["aVdbSelectedGridNames"].text="none";d.inputs.erase("aSegmentFragments");
    assert(fractureCompute(d)&&gridCopies==0&&outputData.grids[0].get()==source.get());
    setup();d=fractureBlock();source->kind=openvdb::GRID_UNKNOWN;assert(fractureCompute(d)&&!gridCopies);
    setup();d=fractureBlock();source->xform.linear=false;assert(fractureCompute(d)&&!gridCopies);
    setup();d=fractureBlock();source->vacant=true;assert(fractureCompute(d)&&!gridCopies);
    setup();d=fractureBlock();FractureMesh mesh;
    assert(readFractureMesh(d,source->transform(),true,mesh));
    assert(mesh.points[0][0]==-.5f&&mesh.points[0][1]==-.5f&&mesh.points[0][2]==-.5f);
    setup();d=fractureBlock();auto mismatched=std::make_shared<openvdb::FloatGrid>(*source);
    mismatched->name="mismatch";mismatched->bg=2;
    auto excluded=std::make_shared<openvdb::FloatGrid>(*source);excluded->name="skip";
    auto unknown=std::make_shared<openvdb::FloatGrid>(*source);unknown->name="unknown";unknown->kind=openvdb::GRID_UNKNOWN;
    inputData.grids={source,mismatched,excluded,unknown};
    d.inputs["aVdbSelectedGridNames"].text="source mismatch unknown";
    assert(fractureCompute(d)&&outputData.grids.size()==4);
    assert(outputData.grids[0].get()==excluded.get()&&outputData.grids[1].get()==unknown.get());
    assert(outputData.grids[2].get()==mismatched.get()&&outputData.grids[3]->getName()=="source_0");
    setup();d=fractureBlock();auto unsupported=std::make_shared<openvdb::Int32Grid>();
    unsupported->kind=openvdb::GRID_LEVEL_SET;inputData.grids={unsupported,source};
    assert(fractureCompute(d)&&gridCopies==0&&meshCalls==0&&outputData.grids[1].get()==source.get());
    setup();d=fractureBlock();auto doubleGrid=std::make_shared<openvdb::DoubleGrid>();
    doubleGrid->kind=openvdb::GRID_LEVEL_SET;doubleGrid->storedBox=source->storedBox;inputData.grids={doubleGrid};
    assert(fractureCompute(d)&&publications==1);
    setup();d=fractureBlock();doubleGrid=std::make_shared<openvdb::DoubleGrid>();
    doubleGrid->kind=openvdb::GRID_LEVEL_SET;doubleGrid->storedBox=source->storedBox;
    inputData.grids={source,doubleGrid};assert(fractureCompute(d)&&outputData.grids[0].get()==doubleGrid.get());
    setup();d=fractureBlock();auto passthrough=std::make_shared<openvdb::FloatGrid>();
    inputData.grids.resize(4096,passthrough);inputData.grids[0]=source;rejectFracture(d);
    setup();d=fractureBlock();doubleGrid=std::make_shared<openvdb::DoubleGrid>();
    doubleGrid->kind=openvdb::GRID_LEVEL_SET;doubleGrid->storedBox=source->storedBox;doubleGrid->bg=1e-300;inputData.grids={doubleGrid};rejectFracture(d);
    setup();d=fractureBlock();d.inputs["aSegmentFragments"].integer=1;source->activeCount=5000;
    rejectFracture(d,false);assert(segmentCalls==0);
    setup();d=fractureBlock();d.inputs["aSegmentFragments"].integer=1;badSegment=true;rejectFracture(d,false);
    setup();d=fractureBlock();d.inputs["aSegmentFragments"].integer=1;segmentCount=5000;rejectFracture(d,false);
    setup();d=fractureBlock();meshConnected=true;pointData.points.resize(13,openvdb::Vec3d(0));
    d.inputs["aCutterOverlap"].integer=1;growPieces=true;rejectFracture(d,false);
    assert(fractureCalls<13); // Stop before uncontrolled exponential expansion.
    setup();d=sphereBlock();assert(DOpenVDBPlatonic().compute(MPlug(MObject{"other"}),d)==MS::kUnknownParameter);
    d=fractureBlock();assert(DOpenVDBFracture().compute(MPlug(MObject{"other"}),d)==MS::kUnknownParameter&&operations.empty());
    std::cout<<"Sphere/fracture compute: numeric, staged growth, segmentation, ownership and "<<injected<<" failure/exception injections passed\n";
}
