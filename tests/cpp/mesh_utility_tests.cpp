// Host API boundaries only; not a Maya extrusion or nearest-neighbor emulator.
#include "mesh_emitter_doubles.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DMeshUVTransfer.h"
#include <fstream>
#include <functional>

using MFloatPoint = MPoint;
template <typename T> struct PointCloud {};
template <typename T> void CreateSearchDataMFP(const MFloatPointArray&, PointCloud<T>&) {
    if (!api("search.copy")) throw std::runtime_error("search copy failed");
}
struct UtilityInput {
    std::size_t points = 3, faces = 1, connections = 3;
    bool valid = true;
    bool getCheckedTotalVertex(std::size_t& value) const {value=points;return valid;}
    bool getCheckedTotalFace(std::size_t& value) const {value=faces;return valid;}
    bool getCheckedTotalConnect(std::size_t& value) const {value=connections;return valid;}
} utilityInput;
static int uvCount = 3, connectionCount = 3;
static MFloatArray uvU{0,1,0}, uvV{0,0,1};
static MIntArray uvCounts{3}, uvIds{0,1,2};
static MIntArray lastSelection;
static int lastDivisions=0;
static bool lastTogether=false,excessiveExtrude=false;
static float lastThickness=0,lastOffset=0;
struct UtilityMesh : MFnMesh {
    using MFnMesh::MFnMesh;
    int numUVs(MStatus* status) const {*status=api("mesh.uvCount");return uvCount;}
    int numFaceVertices(MStatus* status) const {*status=api("mesh.connectionCount");return connectionCount;}
    MStatus getUVs(MFloatArray& u,MFloatArray& v) const {
        const int ordinal=callCount; const auto s=api("mesh.uvs");
        if(s){u=uvU;v=uvV;if(ordinal==shortAt){u.pop_back();shortApplied=true;}}return s;
    }
    MStatus getAssignedUVs(MIntArray& counts,MIntArray& ids) const {
        const int ordinal=callCount;const auto s=api("mesh.assignments");
        if(s){counts=uvCounts;ids=uvIds;if(ordinal==shortAt){ids.pop_back();shortApplied=true;}}return s;
    }
    MStatus extrudeFaces(MIntArray& selected,int divisions,const void* translation,
                         bool together,float thickness,float offset) {
        const auto s=api("mesh.extrude");assert(!translation);
        if(s){lastSelection=selected;lastDivisions=divisions;lastTogether=together;
            lastThickness=thickness;lastOffset=offset;
            if(excessiveExtrude)emitterMeshes[object.name].pointCount=10000000;}
        return s;
    }
};
enum {D_COMP_FACE};
struct DPGroupData {
    using ConstPtr=std::shared_ptr<const DPGroupData>;
    std::vector<bool> ids;
    const std::vector<bool>& getGroupIds()const{return ids;}
};
struct ExtrudeItem {
    MIntArray faces{3};std::size_t points=3,connections=3,uvs=0;
    DPGroupData::ConstPtr group;
    int replacements=0;
    MFloatArray u,v;MIntArray uvCounts,uvIds;
    bool setUvData(const MFloatArray& newU,const MFloatArray& newV,const MIntArray& counts,const MIntArray& ids) {
        if(!api("mesh.setUV"))return false;u=newU;v=newV;uvCounts=counts;uvIds=ids;return true;
    }
    MIntArray getFaceList()const{return faces;}
    int numFace()const{return faces.length();}
    std::size_t numberOfPoints()const{return points;}
    std::size_t numberOfConnections()const{return connections;}
    std::size_t numberOfUVs()const{return uvs;}
    bool isExistCompGrp(int,const std::string&)const{return bool(group);}
    DPGroupData::ConstPtr getCompGrp(int,const std::string&)const{return group;}
    bool setReplaceMeshData(MObject){if(!api("mesh.replace"))return false;++replacements;return true;}
};
struct ExtrudeData {
    static constexpr MFnData::Type id=MFnData::kAny;
    std::vector<std::shared_ptr<ExtrudeItem>> items;
    bool valid=true;
    bool hasValidMeshTopology()const{if(!valid)return false;for(const auto& p:items)if(!p)return false;return true;}
    bool getCheckedTotalFace(std::size_t& value)const{value=0;for(const auto& p:items)value+=p->faces.length();return true;}
    bool getCheckedTotalConnect(std::size_t& value)const{value=0;for(const auto& p:items)value+=p->connections;return true;}
    std::size_t numberOfMesh()const{return items.size();}
    auto beginMeshes()const -> decltype(items.begin()){return items.begin();}
    auto endMeshes()const -> decltype(items.end()){return items.end();}
    MStatus copyFrom(const ExtrudeData& source){const auto s=api("mesh.copy");
        if(s){items.clear();valid=source.valid;for(auto& item:source.items)items.push_back(std::make_shared<ExtrudeItem>(*item));}return s;}
    void updateBBox(){if(!api("mesh.bounds"))throw std::runtime_error("bounds failed");}
};
static ExtrudeData extrudeInput,extrudeStorage,extrudeOutput;
static bool nullCopyInput=false,nullCopyOutput=false;
static MStatus generateMMeshFromDmeshWithUV(const ExtrudeItem&,MObject& object){
    const auto s=api("mesh.convertUV");if(s)object={"mesh"};return s;
}
namespace dommy {namespace maya {
template<> MStatus getPluginDataFromHandle<ExtrudeData>(const MDataHandle& handle,MFnData::Type,const ExtrudeData*& output) {
    handle.check();const auto s=api("mesh.input");output=s&&!nullCopyInput?&extrudeInput:nullptr;return s;
}
template<> MStatus createPluginDataObject<ExtrudeData>(MFnData::Type,MObject& object,ExtrudeData*& output){
    const auto s=api("plugin.create");if(s){extrudeStorage=ExtrudeData();object={"extrude"};output=nullCopyOutput?nullptr:&extrudeStorage;}return s;
}
template<> MStatus publishTypedPluginDataObject<ExtrudeData>(MDataHandle& handle,MObject,MFnData::Type){
    handle.check();const auto s=api("publish");if(s){extrudeOutput=extrudeStorage;++published;}return s;
}
}}
#define MFnMesh UtilityMesh
#include "mesh_utility.inc"
#undef MFnMesh

static void fixture() {
    reset(); inputs.clear(); emitterMeshes.clear();
    nullCopyInput=nullCopyOutput=false;
    emitterMeshes["mesh"]=EmitterMesh();
    inputs["inObject"].object={"mesh"}; inputs["inObject"].type=MFnData::kMesh;
    inputs["MinRatio"].number=.2;
    utilityInput=UtilityInput(); uvCount=3;connectionCount=3;
    uvU={0,1,0};uvV={0,0,1};uvCounts={3};uvIds={0,1,2};
    extrudeInput=ExtrudeData();extrudeInput.items.push_back(std::make_shared<ExtrudeItem>());
    extrudeOutput=ExtrudeData();lastSelection={};lastDivisions=0;excessiveExtrude=false;
    inputs["ThickNess"].number=1;inputs["Offset"].number=.25;
    inputs["ExtrusionCount"].number=2;inputs["FaceGroup"].string="outer";
}
static MStatus guarded(const std::function<MStatus()>& fn) {
    try {return fn();} catch (...) {return MS::kFailure;}
}
static std::size_t calls(const std::string& name) {
    return std::count(operations.begin(),operations.end(),name);
}
static int boundaries() {
    int tested=0;
    for(auto reader:readers) {
        fixture();MDataBlock data;assert(reader(data)==MS::kSuccess && callCount==1);
        for(auto status:{MS::kFailure,MS::kInvalidParameter}) {
            fixture();failAt=0;injectedStatus=status;
            assert(reader(data)==status && callCount==1);++tested;
        }
        fixture();throwAt=0;assert(!guarded([&]{return reader(data);}));++tested;
    }
    for(auto boundary:{referenceSearch,referenceUV}) {
        fixture();assert(boundary()==MS::kSuccess);const auto trace=operations;
        for(unsigned int i=0;i<trace.size();++i) {
            for(auto status:{MS::kFailure,MS::kInvalidParameter}) {
                fixture();failAt=i;injectedStatus=status;
                assert(!guarded(boundary));assert(callCount==int(i)+1);++tested;
            }
            fixture();throwAt=i;assert(!guarded(boundary));++tested;
            if(trace[i]=="mesh.points" || trace[i]=="mesh.uvs" || trace[i]=="mesh.assignments") {
                fixture();shortAt=i;assert(!boundary() && shortApplied);++tested;
            }
        }
    }
    fixture();emitterMeshes["mesh"].pointCount=10000000;
    assert(!referenceSearch() && calls("mesh.points")==0 && calls("search.copy")==0);
    fixture();utilityInput.points=10000000;
    assert(!referenceSearch() && calls("mesh.points")==0);
    fixture();emitterMeshes["mesh"].points[1].x=NAN;
    assert(!referenceSearch() && calls("search.copy")==0);
    for(double ratio:{-1.0,2.0,std::numeric_limits<double>::quiet_NaN()}) {
        fixture();inputs["MinRatio"].number=ratio;
        assert(!referenceSearch() && calls("mesh.points")==0);
    }
    fixture();uvCount=50000000;
    assert(!referenceUV() && calls("mesh.uvs")==0 && calls("mesh.assignments")==0);
    fixture();connectionCount=9000000;utilityInput.connections=9000000;
    assert(!referenceUV() && calls("mesh.uvs")==0);
    fixture();utilityInput.faces=2;
    assert(!referenceUV() && calls("mesh.uvs")==0);
    fixture();utilityInput.connections=4;
    assert(!referenceUV() && calls("mesh.uvs")==0);
    fixture();uvIds[2]=3;assert(!referenceUV());
    fixture();uvCounts[0]=0;assert(!referenceUV());
    fixture();uvV[1]=NAN;assert(!referenceUV());
    return tested;
}
static MStatus extrude(){DMExtrude node;MDataBlock data;return node.compute(MPlug({"node"},DMExtrude::outDM),data);}
static int extrusion() {
    fixture();assert(extrude()==MS::kSuccess && published==1 && cleaned==1);
    assert(lastSelection.length()==1 && lastSelection[0]==0 && lastDivisions==2);
    assert(!lastTogether && lastThickness==1 && lastOffset==.25f);
    assert(extrudeInput.items[0]->replacements==0 && extrudeOutput.items[0]->replacements==1);
    const auto trace=operations;int tested=0;
    for(unsigned int i=0;i<trace.size();++i){
        for(auto status:{MS::kFailure,MS::kInvalidParameter}){
            fixture();failAt=i;injectedStatus=status;assert(!extrude());assert(callCount==int(i)+1);
            assert(extrudeInput.items[0]->replacements==0);
            if(trace[i]!="clean")assert(published==0);assert(cleaned==0);++tested;
        }
        fixture();throwAt=i;assert(!extrude());assert(extrudeInput.items[0]->replacements==0);++tested;
        if(trace[i]=="array.resize") {fixture();shortAt=i;assert(!extrude() && shortApplied && published==0);++tested;}
    }
    fixture();inputs["ExtrusionCount"].number=1000000;
    assert(!extrude() && calls("mesh.extrude")==0 && calls("mesh.copy")==0);
    fixture();inputs["ThickNess"].number=NAN;
    assert(!extrude() && calls("mesh.copy")==0);
    fixture();inputs["Offset"].number=INFINITY;
    assert(!extrude() && calls("mesh.copy")==0);
    fixture();extrudeInput.items[0]->points=10000000;
    assert(!extrude() && calls("mesh.copy")==0);
    fixture();excessiveExtrude=true;
    assert(!extrude() && published==0 && calls("mesh.replace")==0);
    fixture();inputs["ExtrudeTogether"].number=1;assert(extrude()==MS::kSuccess && lastTogether);
    fixture();auto group=std::make_shared<DPGroupData>();group->ids={false};extrudeInput.items[0]->group=group;
    assert(extrude()==MS::kSuccess && calls("mesh.extrude")==0);
    fixture();extrudeInput.items.clear();assert(extrude()==MS::kSuccess && calls("mesh.convertUV")==0);
    fixture();extrudeInput.items.push_back(std::make_shared<ExtrudeItem>());
    assert(extrude()==MS::kSuccess && extrudeOutput.items.size()==2 && calls("mesh.extrude")==2);
    return tested;
}
static void budgets() {
    using namespace dommy::mesh;
    MeshMergePlan plan;
    assert(appendExtrusionPlan(4,1,4,4,4,2,plan));
    assert(plan.points==12 && plan.faces==9 && plan.connections==36 && plan.uvs==36);
    assert(appendExtrusionPlan(3,1,3,0,0,1,plan));
    assert(plan.points==15 && plan.faces==10 && plan.connections==39);
    const auto previous=plan;
    for(int divisions:{0,-1,1000001,std::numeric_limits<int>::max()}) {
        assert(!appendExtrusionPlan(4,1,4,0,4,divisions,plan));
        assert(plan.points==previous.points && plan.meshes==previous.meshes);
    }
    assert(!appendExtrusionPlan(4,1,4,0,5,1,plan));
    assert(!appendExtrusionPlan(4,1,4,0,4,1000000,plan));
    assert(!appendExtrusionPlan(10000000,1,4,0,0,1,plan));
    const auto max=std::numeric_limits<std::size_t>::max();
    assert(!appendExtrusionPlan(max,1,4,0,4,1,plan));
    assert(validPointSearchStorage(3,3));
    assert(!validPointSearchStorage(max,0) && !validPointSearchStorage(0,max));
    assert(validPointSearchStorage(2097152,0));
    assert(!validPointSearchStorage(2097152,1));
    assert(validMeshUVTransferBudget(1,3,3));
    assert(!validMeshUVTransferBudget(1,3,50000000));
    assert(!validMeshUVTransferBudget(1,max,3));
    assert(!validMeshUVTransferBudget(2000001,0,0));
    MeshMergePlan combined;
    assert(appendExtrusionPlan(500000,1,3,0,0,1,combined));
    assert(appendExtrusionPlan(500000,1,3,0,0,1,combined));
    assert(appendExtrusionPlan(500000,1,3,0,0,1,combined));
    assert(appendExtrusionPlan(500000,1,3,0,0,1,combined));
    assert(!appendExtrusionPlan(500000,1,3,0,0,1,combined));
}
static MStatus copyUV(){MDataBlock data;return DMCopyUV().compute(MPlug({},DMCopyUV::outDM),data);}
static void completeCopyUV() {
    fixture();assert(copyUV()&&published==1&&cleaned==1);
    assert(extrudeOutput.items[0]->u==uvU&&extrudeOutput.items[0]->v==uvV);
    assert(extrudeOutput.items[0]->uvIds==uvIds&&extrudeInput.items[0]->u.empty());
    const auto trace=operations;
    const auto publishedAt=std::find(trace.begin(),trace.end(),"publish")-trace.begin();
    for(int at=0;at<int(trace.size());++at) {
        for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            fixture();failAt=at;injectedStatus=code;const auto result=copyUV();assert(!result);
            if(trace[at]!="mesh.setUV")assert(result==code);
            assert(callCount==at+1&&extrudeInput.items[0]->u.empty());
            if(at<=publishedAt)assert(published==0&&cleaned==0);
            fixture();assert(copyUV()&&published==1);
        }
        fixture();throwAt=at;assert(!copyUV()&&callCount==at+1);
        if(trace[at]=="array.resize"||trace[at]=="mesh.uvs"||trace[at]=="mesh.assignments") {
            fixture();shortAt=at;assert(!copyUV()&&published==0);
        }
    }
    fixture();nullCopyInput=true;assert(!copyUV()&&calls("plugin.create")==0);
    fixture();nullCopyOutput=true;assert(copyUV()==MS::kFailure&&published==0);
    fixture();extrudeInput.valid=false;assert(!copyUV()&&calls("plugin.create")==0);
    fixture();extrudeInput.items.clear();assert(copyUV()&&published==1&&calls("mesh.uvs")==0);
    fixture();extrudeInput.items.push_back(std::make_shared<ExtrudeItem>());
    emitterMeshes["mesh"].faceCount=2;connectionCount=6;uvCounts={3,3};uvIds={0,1,2,2,1,0};
    assert(copyUV()&&extrudeOutput.items.size()==2);
    assert(extrudeOutput.items[1]->u==MFloatArray({0,1,0})&&extrudeOutput.items[1]->v==MFloatArray({1,0,0}));
    std::cout<<"Complete DMCopyUV: "<<trace.size()<<" status/exception sites, short arrays, source isolation, empty/split output and retry passed\n";
}
int main(int argc,char** argv) {
    const char* names[]={"DMNormal","DMExtrude","DMEdgeDisplaceDir","DMCopyUV"};
    MStatus(*initializers[])()={DMNormal::initialize,DMExtrude::initialize,DMEdgeDisplaceDir::initialize,DMCopyUV::initialize};
    std::string schema;int counts[4];
    for(int i=0;i<4;++i){schema+=schemaTrace(names[i],initializers[i]);counts[i]=callCount;}
    if(argc==2 && std::string(argv[1])=="--print-schema"){std::cout<<schema;return 0;}
    assert(argc==2);std::ifstream file(argv[1]);assert(file.good());
    const std::string expected((std::istreambuf_iterator<char>(file)),{});assert(schema==expected);
    int tested=0;for(int i=0;i<4;++i)tested+=testFailures(names[i],initializers[i],counts[i]);
    tested+=boundaries();tested+=extrusion();budgets();completeCopyUV();
    std::cout<<"Mesh utility contracts/budgets and "<<tested<<" injected failures passed\n";
}
