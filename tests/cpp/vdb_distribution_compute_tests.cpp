#include "vdb_distribution_doubles.h"
MStatus getPrimDataFromMesh(MFnMesh&, meshPrimData& data) {
    data.readBase();
    // A native copy can report failure after filling apparently valid arrays.
    // The caller must propagate status, not rely only on topology validation.
    return operation("mesh.readPrimitives");
}
#define MDataHandle DistributionHandle
#define MDataBlock DistributionBlock
#define sdfInteriorMask distributionMask
#include "vdb_distribution_compute.inc"
#undef sdfInteriorMask

static std::shared_ptr<openvdb::FloatGrid> source;
void setup(){
    resetCalls();inputData=DOpenVDBData();source=std::make_shared<openvdb::FloatGrid>();
    source->storedBox.reset({32,32,32},{33,33,33});source->activeCount=10000;inputData.grids={source};
    stagedPoints=DPData();publications=gridCopies=invalidReads=scatterCalls=packingCalls=baseCalls=meshCalls=maskCalls=0;
    badPoint=extraPoint=badSphere=extraSphere=interrupted=badMask=nullBase=badBase=badTransform=shortTransform=noSpheres=false;
    nullCopy=wrongCopy=nullOutput=invalidOutputPayload=emptyInteriorMask=nullCreatedMesh=false;
    receivedMinimum.clear();receivedMaximum.clear();scatterMode.clear();
}
MDataBlock scatterBlock(int mode=0,bool local=false,bool interior=false){
    MDataBlock d;d.inputs["aVdbInput"].payload=&inputData;d.inputs["aVdbSelectedGridNames"].text="*";
    d.inputs["aMode"].integer=mode;d.inputs["aSeed"].integer=1;d.inputs["aSpread"].number=1;
    d.inputs["aInterior"].integer=interior;
    if(mode==0)d.inputs["aCount"].wideInteger=2;
    if(mode==1){d.inputs["aDensity"].number=local?.0001f:1;d.inputs["aIsLocal"].integer=local;}
    if(mode==2)d.inputs["aPerVoxel"].number=2;
    return d;
}
MDataBlock spheresBlock(bool world=false){
    MDataBlock d;d.inputs["aVdbInput"].payload=&inputData;d.inputs["aVdbSelectedGridNames"].text="*";
    d.inputs["aIsovalue"].number=0;d.inputs["aUseWorldSpaceUnits"].integer=world;
    d.inputs[world?"aMinRadiusInWorld":"aMinRadiusInVoxels"].number=1;
    d.inputs[world?"aMaxRadiusInWorld":"aMaxRadiusInVoxels"].number=std::numeric_limits<float>::max();
    d.inputs["aOverlapping"].integer=0;d.inputs["aMaxSpheres"].integer=2;
    d.inputs["aScatterPoints"].integer=1000;d.inputs["aMeshDetail"].integer=1;return d;
}
MStatus scatterCompute(MDataBlock& d){return DOpenVDBScatter().compute(MPlug(DOpenVDBScatter::outDP),d);}
MStatus spheresCompute(MDataBlock& d){return DOpenVDBToSpheres().compute(MPlug(DOpenVDBToSpheres::outMesh),d);}
void rejectScatter(MDataBlock& d,bool early=true){assert(!scatterCompute(d)&&!publications&&!invalidReads);if(early)assert(!scatterCalls&&!maskCalls);}
void rejectSpheres(MDataBlock& d,bool early=true){assert(!spheresCompute(d)&&!publications&&!invalidReads);if(early)assert(!packingCalls&&!gridCopies);}
int main(){
    setup();
    { dommy::vdb::TreeEditBudget budget;budget.work=dommy::generation::kMaxWorkItemCount-8;
      ScatterSettings settings;settings.count=2;ScatterPlan plan;
      assert(!planScatter(source,settings,budget,plan)&&!scatterCalls); }
    setup();auto d=scatterBlock();assert(scatterCompute(d)&&publications==1&&stagedPoints.points.size()==2);
    setup();d=spheresBlock();assert(spheresCompute(d)&&publications==1&&packingCalls==1&&meshCalls==1);
    int injected=0;
    for(int scenario=0;scenario<8;++scenario){
        auto prepare=[&](){setup();if(scenario==4){source->kind=openvdb::GRID_LEVEL_SET;return scatterBlock(0,false,true);}
            if(scenario<4)return scatterBlock(scenario==3?2:scenario==0?0:1,scenario==2);
            if(scenario==7)source->activeCount=5000;
            return spheresBlock(scenario==6);};
        auto compute=scenario<5?scatterCompute:spheresCompute;
        d=prepare();assert(compute(d));const auto trace=operations;
        for(std::size_t i=0;i<trace.size();++i)for(bool exception:{false,true}){
            d=prepare();if(exception)attributeThrowAt=int(i);else failAt=int(i);
            assert(!compute(d)&&invalidReads==0);
            assert(publications==((trace[i]=="output.clean"||trace[i]=="output.handleClean")?1:0));
            assert(source->values[0]==1&&source->getName()=="source");++injected;
        }
    }
    for(int mode:{-1,3}){setup();d=scatterBlock(mode);d.inputs["aPerVoxel"].number=2;rejectScatter(d);}
    for(float v:{NAN,INFINITY,-1.f,2.f}){setup();d=scatterBlock();d.inputs["aSpread"].number=v;rejectScatter(d);}
    setup();d=scatterBlock();d.inputs["aSeed"].integer=-1;rejectScatter(d);
    for(std::int64_t v:{std::int64_t(-1),std::int64_t(10000001),INT64_MAX}){
        setup();d=scatterBlock();d.inputs["aCount"].wideInteger=v;rejectScatter(d);}
    for(int mode:{1,2})for(float v:{NAN,INFINITY,-1.f,1e30f}){
        setup();d=scatterBlock(mode);d.inputs[mode==1?"aDensity":"aPerVoxel"].number=v;rejectScatter(d);}
    setup();d=scatterBlock(1,true);d.inputs["aDensity"].number=1e30f;rejectScatter(d);
    setup();d=scatterBlock(1);source->values[0]=1e20f;rejectScatter(d);
    setup();d=scatterBlock(1);source->values[0]=-1e20f;rejectScatter(d);
    setup();d=scatterBlock(1);source->values[0]=-1;assert(scatterCompute(d)&&stagedPoints.points.empty());
    setup();d=scatterBlock(2);source->tile=true;source->voxelsPerValue=1ULL<<32;rejectScatter(d);
    setup();d=scatterBlock(2);source->values={1,1};d.inputs["aPerVoxel"].number=6000000;rejectScatter(d);
    setup();d=scatterBlock();source->activeCount=100000001;rejectScatter(d);
    setup();d=scatterBlock();d.inputs["aCount"].wideInteger=4000000;rejectScatter(d); // work, not hard count
    setup();d=scatterBlock();d.inputs["aCount"].wideInteger=1400000;
    auto other=std::make_shared<openvdb::FloatGrid>(*source);other->name="other";inputData.grids.push_back(other);rejectScatter(d);
    for(int mode:{0,1,2}){setup();d=scatterBlock(mode);d.inputs[mode==0?"aCount":mode==1?"aDensity":"aPerVoxel"]=InputValue();
        assert(scatterCompute(d)&&stagedPoints.points.empty());}
    setup();d=scatterBlock();d.inputs["aVdbSelectedGridNames"].text="missing";d.inputs.erase("aMode");
    assert(scatterCompute(d)&&publications==1&&stagedPoints.points.empty());
    setup();d=scatterBlock(2);source->vacant=true;assert(scatterCompute(d)&&!scatterCalls);
    for(int kind=0;kind<8;++kind)for(int mode=0;mode<4;++mode){
        setup();GridCPtr grid;
        switch(kind){case 0:grid=std::make_shared<openvdb::FloatGrid>();break;
        case 1:grid=std::make_shared<openvdb::DoubleGrid>();break;case 2:grid=std::make_shared<openvdb::Int32Grid>();break;
        case 3:grid=std::make_shared<openvdb::Int64Grid>();break;case 4:grid=std::make_shared<openvdb::BoolGrid>();break;
        case 5:grid=std::make_shared<openvdb::Vec3SGrid>();break;case 6:grid=std::make_shared<openvdb::Vec3DGrid>();break;
        default:grid=std::make_shared<openvdb::Vec3IGrid>();}
        inputData.grids={grid};d=scatterBlock(mode==3?2:mode==0?0:1,mode==2);
        if(kind>=4&&mode<2)rejectScatter(d);else assert(scatterCompute(d)&&scatterCalls==1);
    }
    setup();d=scatterBlock(1,false,true);source->kind=openvdb::GRID_LEVEL_SET;assert(scatterCompute(d)&&maskCalls==1);
    setup();d=scatterBlock(2,false,true);source->kind=openvdb::GRID_LEVEL_SET;assert(scatterCompute(d)&&maskCalls==1);
    for(bool* flag:{&nullCopy,&badMask,&badPoint,&extraPoint,&interrupted,&nullOutput,&invalidOutputPayload}){
        setup();d=scatterBlock(0,false,true);source->kind=openvdb::GRID_LEVEL_SET;*flag=true;rejectScatter(d,false);}
    setup();d=scatterBlock(0,false,true);source->kind=openvdb::GRID_LEVEL_SET;emptyInteriorMask=true;
    assert(scatterCompute(d)&&stagedPoints.points.empty());
    for(bool sphere:{false,true}){
        auto compute=sphere?spheresCompute:scatterCompute;auto reject=sphere?rejectSpheres:rejectScatter;
        for(float value:{NAN,INFINITY}){setup();d=sphere?spheresBlock():scatterBlock();source->values[0]=value;reject(d,true);}
        setup();d=sphere?spheresBlock():scatterBlock();source->xform.linear=false;reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();source->xform.forward[0][0]=1e-9;reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();source->xform.shift[0]=1e13;reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();source->storedBox.reset({1000001,0,0},{1000002,1,1});reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();source->memory=dommy::vdb::kMaxVdbPayloadBytes;reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();auto late=std::make_shared<openvdb::FloatGrid>(*source);
        late->values[0]=NAN;inputData.grids.push_back(late);reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();inputData.valid=false;reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();d.inputs["aVdbInput"].type=DPData::id;reject(d,true);
        setup();d=sphere?spheresBlock():scatterBlock();inputData.grids.push_back(source);assert(compute(d)&&publications==1);
    }
    for(const char* a:{"aIsovalue","aMinRadiusInVoxels","aMaxRadiusInVoxels"})for(float v:{NAN,INFINITY}){
        setup();d=spheresBlock();d.inputs[a].number=v;rejectSpheres(d);}
    for(const char* a:{"aMinRadiusInVoxels","aMaxRadiusInVoxels"}){
        setup();d=spheresBlock();d.inputs[a].number=-1;rejectSpheres(d);}
    for(const char* a:{"aMaxSpheres","aScatterPoints","aMeshDetail"})for(int v:{0,-1,10000001}){
        setup();d=spheresBlock();d.inputs[a].integer=v;rejectSpheres(d);}
    setup();d=spheresBlock();d.inputs["aMaxSpheres"].integer=10000;d.inputs["aScatterPoints"].integer=20000;rejectSpheres(d);
    setup();d=spheresBlock();d.inputs["aMaxSpheres"].integer=7000;d.inputs["aScatterPoints"].integer=10000;
    other=std::make_shared<openvdb::FloatGrid>(*source);other->name="other";inputData.grids.push_back(other);rejectSpheres(d);
    setup();d=spheresBlock();d.inputs["aMaxSpheres"].integer=1000;d.inputs["aMeshDetail"].integer=5;rejectSpheres(d);assert(baseCalls==1);
    setup();d=spheresBlock();source->values[0]=1e20f;rejectSpheres(d);
    setup();d=spheresBlock();source->xform.uniform=false;rejectSpheres(d);
    setup();d=spheresBlock();source->kind=openvdb::GRID_LEVEL_SET;source->bg=0;rejectSpheres(d);
    setup();d=spheresBlock();source->storedBox.reset({0,0,0},{1000,1000,1000});rejectSpheres(d);
    setup();d=spheresBlock();d.inputs["aMinRadiusInVoxels"].number=1e20f;rejectSpheres(d);
    setup();d=spheresBlock(true);source->dx=openvdb::Vec3d(2);source->xform.preScale(openvdb::Vec3d(2));
    assert(spheresCompute(d)&&receivedMinimum[0]==.5f&&std::isfinite(receivedMaximum[0]));
    for(bool* flag:{&nullCopy,&wrongCopy,&badSphere,&extraSphere,&interrupted,&nullBase,&badBase,&badTransform,&shortTransform,&nullOutput,&nullCreatedMesh}){
        setup();d=spheresBlock();*flag=true;rejectSpheres(d,false);
        if(flag==&badSphere)assert(operations.back()=="native.packing");
        if(flag==&shortTransform)assert(operations.back()=="mesh.duplicate");
        if(flag==&nullCreatedMesh)assert(operations.back()=="mesh.create" && meshCalls==1);
        setup();d=spheresBlock();assert(spheresCompute(d)&&publications==1);}
    setup();d=spheresBlock();noSpheres=true;assert(spheresCompute(d)&&publications==1&&!meshCalls);
    setup();d=spheresBlock();source->vacant=true;assert(spheresCompute(d)&&publications==1&&!packingCalls&&!baseCalls);
    setup();d=spheresBlock();inputData.grids={std::make_shared<openvdb::BoolGrid>()};assert(spheresCompute(d)&&!packingCalls);
    setup();d=spheresBlock();d.inputs["aVdbSelectedGridNames"].text="missing";rejectSpheres(d);
    std::cout<<"Scatter/ToSpheres compute: "<<injected<<" failure/exception injections and numeric, budget, dispatch, staging cases passed\n";
}
