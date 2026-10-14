// Execute production helpers/constructor/compute/initialize; not a Nucleus emulator.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DNumericValidation.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <memory>

static int shortAt = -1, nullAt = -1, published = 0, cleaned = 0;
static double currentTime = 2.0, stepSeconds = 1.0 / 24.0;
static int malformed = 0;
static bool shortApplied = false;
struct MPoint {
    double x, y, z, w;
    MPoint(double x=0, double y=0, double z=0, double w=1):x(x),y(y),z(z),w(w){}
};
struct MFloatPoint {
    float x, y, z, w;
    MFloatPoint(float x=0, float y=0, float z=0, float w=1):x(x),y(y),z(z),w(w){}
    operator MPoint() const {return {x,y,z,w};}
};
template<class T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length() const {return this->size();}
    T& operator[](unsigned int i) {assert(i<this->size());return this->at(i);}
    const T& operator[](unsigned int i) const {assert(i<this->size());return this->at(i);}
    void shorten(int index) {if(index==shortAt && !this->empty()){this->pop_back();shortApplied=true;}}
    MStatus setLength(unsigned int n) {
        const int index=callCount; const auto s=operation("array.resize");
        if(s){this->resize(n);shorten(index);} return s;
    }
    MStatus copy(const Array& a) {
        const int index=callCount;const auto s=operation("array.copy");
        if(s){*this=a;shorten(index);}return s;
    }
};
using MPointArray=Array<MPoint>;
using MFloatPointArray=Array<MFloatPoint>;
using MIntArray=Array<int>;
using int2=int[2];
namespace MSpace {enum Space{kObject};}
struct MDataHandle {
    double number=0;bool valid=false;
    MDataHandle(double n,bool v):number(n),valid(v){}
    bool asBool()const{assert(valid);return number!=0;}
    float asFloat()const{assert(valid);return static_cast<float>(number);}
    MStatus set(const MObject& o){assert(valid&&!o.isNull());const auto s=operation("publish");if(s)++published;return s;}
    void setClean(){assert(valid);++cleaned;}
};
struct MPlug {MObject attribute;bool operator!=(const MObject& a)const{return attribute.name!=a.name;}};
static std::map<std::string,double> inputs;
struct MDataBlock {
    MDataHandle inputValue(const MObject& a,MStatus* s){*s=operation("input "+a.name);return {inputs[a.name],bool(*s)};}
    MDataHandle outputValue(const MObject&,MStatus* s){*s=operation("output");return {0,bool(*s)};}
    MStatus setClean(const MPlug&){const auto s=operation("clean");if(s)++cleaned;return s;}
};
struct MPxNode:AttributeNode {
    enum SchedulingType{kUntrusted};
    virtual ~MPxNode(){}
    virtual MStatus compute(const MPlug&,MDataBlock&)=0;
    virtual SchedulingType schedulingType()const{return kUntrusted;}
};
using MTypeId=int;
struct DMeshInternalData {
    struct Topology {MIntArray mFaceList{3},mConnectList{0,1,2};};
    std::shared_ptr<Topology> arrays_{new Topology};
    std::vector<std::array<double,3>> points{{{0,0,0}},{{1,0,0}},{{0,1,0}}};
    std::size_t numberOfPoints()const{return points.size();}
    int numFace()const{return arrays_->mFaceList.length();}
    std::size_t numberOfConnections()const{return arrays_->mConnectList.length();}
    auto beginPt()const -> decltype(points.begin()){return points.begin();}
    auto endPt()const -> decltype(points.end()){return points.end();}
    MStatus copyTopology(MIntArray&,MIntArray&)const;
};
struct DMeshData {
    static constexpr MFnData::Type id=MFnData::kMesh;
    std::vector<std::shared_ptr<DMeshInternalData>> meshes{std::make_shared<DMeshInternalData>()};
    std::size_t numberOfMesh()const{return meshes.size();}
    auto beginMeshes()const -> decltype(meshes.begin()){return meshes.begin();}
    auto endMeshes()const -> decltype(meshes.end()){return meshes.end();}
};
static DMeshData input;
MStatus getValidInputDM(const MObject&,MDataBlock&,const DMeshData*& result){const auto s=operation("input.mesh");if(s)result=&input;return s;}
struct MFnMeshData {MObject create(MStatus* s){const int i=callCount;*s=operation("meshData.create");return *s&&i!=nullAt?MObject{"owner"}:MObject{};}};
struct MFnMesh {
    MPointArray points;MIntArray faces,connections;
    MObject create(int p,int f,const MPointArray& ps,const MIntArray& fs,const MIntArray& cs,MObject,MStatus* s){
        assert(p==int(ps.size())&&f==int(fs.size())); const int i=callCount;*s=operation("mesh.create");
        if(*s){points=ps;faces=fs;connections=cs;}return *s&&i!=nullAt?MObject{"mesh"}:MObject{};
    }
    int numVertices(MStatus* s){*s=operation("mesh.pointsCount");return malformed==1?-1:int(points.size());}
    int numPolygons(MStatus* s){*s=operation("mesh.facesCount");return int(faces.size());}
    int numFaceVertices(MStatus* s){*s=operation("mesh.connectionsCount");return int(connections.size());}
    int numUVs(MStatus* s){*s=operation("mesh.uvsCount");return 0;}
    int numEdges(MStatus* s){*s=operation("mesh.edgesCount");return malformed==2?-1:int(connections.size());}
    MStatus getPoints(MFloatPointArray& out,MSpace::Space space){
        assert(space==MSpace::kObject);const int i=callCount;const auto s=operation("mesh.points");
        if(s){for(const auto& p:points)out.push_back({float(p.x),float(p.y),float(p.z),float(p.w)});out.shorten(i);
            if(malformed==3)out[0].x=std::numeric_limits<float>::infinity();}return s;
    }
    MStatus getVertices(MIntArray& fs,MIntArray& cs){const int i=callCount;const auto s=operation("mesh.topology");
        if(s){fs=faces;cs=connections;cs.shorten(i);if(malformed==4)fs[0]=2;if(malformed==5)cs[0]=-1;}return s;}
    MStatus getEdgeVertices(int i,int2& e){const auto s=operation("mesh.edge");if(s){e[0]=i;e[1]=(i+1)%points.size();if(malformed==6)e[1]=-1;}return s;}
};
struct MnRigid {
    MFloatPointArray positions,velocities;
    std::vector<int> faceCounts,faceIndices;
    bool created=false,topology=false;
    MStatus createNRigid(){const auto s=operation("rigid.create");created=bool(s);return s;}
    MStatus getNumVertices(unsigned& n)const{assert(created&&topology);const auto s=operation("rigid.count");if(s)n=positions.length();return s;}
    MStatus getPositions(MFloatPointArray& p)const{assert(topology);const int i=callCount;const auto s=operation("rigid.positions");if(s){p=positions;p.shorten(i);if(malformed==7)p[0].x=std::numeric_limits<float>::infinity();}return s;}
    MStatus setTopology(int n,int* fs,int* cs,int count,int* es){assert(created&&n>0&&fs&&cs&&count>=0&&(count==0||es));const auto s=operation("rigid.topology");if(s){topology=true;faceCounts.assign(fs,fs+n);int corners=0;for(int f:faceCounts)corners+=f;faceIndices.assign(cs,cs+corners);}return s;}
    MStatus setPositions(const MFloatPointArray& p,bool start){assert(topology&&start);const auto s=operation("rigid.setPositions");if(s)positions=p;return s;}
    MStatus setVelocities(const MFloatPointArray& v){assert(topology);const auto s=operation("rigid.velocities");if(s)velocities=v;return s;}
    MStatus setThickness(float){assert(topology);return operation("rigid.thickness");}
    MStatus setBounce(float){return operation("rigid.bounce");}
    MStatus setFriction(float){return operation("rigid.friction");}
    MStatus setCollisionFlags(bool,bool,bool){return operation("rigid.collisions");}
};
struct MFnNObjectData {
    MObject value;
    MObject create()const{const int i=callCount;const auto s=operation("nobject.create");return s&&i!=nullAt?MObject{"nobject"}:MObject{};}
    MStatus setObject(const MObject& o){const auto s=operation("nobject.bind");if(s)value=o;return s;}
    MStatus setObjectPtr(MnRigid*){return operation("nobject.ptr");}
    MStatus setCached(bool v){assert(!v);return operation("nobject.cached");}
    MObject object()const{return value;}
};
namespace dommy {namespace maya {
MStatus getEvaluationTime(MDataBlock&,MTime& time){const auto s=operation("time");if(s)time=MTime(currentTime);return s;}
MStatus getEvaluationStepSeconds(MDataBlock&,double& value){const auto s=operation("time.step");if(!s)return s;value=stepSeconds;return std::isfinite(value)&&value>1e-12?MS::kSuccess:MS::kFailure;}
bool isFinitePoint(const MPoint& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.w);}
bool isFloatRepresentablePoint(const MPoint& p){return isFinitePoint(p)&&numeric::canRepresentAsFloat(p.x)&&numeric::canRepresentAsFloat(p.y)&&numeric::canRepresentAsFloat(p.z);}
}}
// preflightMeshImport is extracted too; its scalar query/admission control is real.
#include "nrigid.inc"

static void reset(){resetCalls();shortAt=nullAt=-1;published=cleaned=0;shortApplied=false;}
static void fixture(){reset();input=DMeshData();inputs.clear();inputs["Thickness"]=.1;inputs["Friction"]=.1;inputs["CollideFace"]=1;malformed=0;currentTime=2;stepSeconds=1.0/24;}
static MStatus evaluate(DMnRigidGen& node){MDataBlock block;return node.compute({DMnRigidGen::outNObject},block);}
static void prepare(DMnRigidGen& node,int branch){assert(evaluate(node));if(branch==2)inputs["IsPassive"]=1;reset();}
static std::size_t calls(const char* name){return std::count(operations.begin(),operations.end(),name);}
int main(){
    fixture();assert(DMnRigidGen::initialize());const int initCalls=callCount;
    int tests=testFailures("DMnRigidGen",DMnRigidGen::initialize,initCalls);
    for(int i=0;i<initCalls;++i){reset();attributeThrowAt=i;assert(!DMnRigidGen::initialize()&&callCount==i+1);++tests;}
    for(int branch=0;branch<3;++branch){
        fixture();DMnRigidGen baseline;if(branch)prepare(baseline,branch);else reset();
        assert(evaluate(baseline));const auto trace=operations;
        assert(published==1&&cleaned==2);
        for(int i=0;i<int(trace.size());++i){
            for(int mode=0;mode<3;++mode){
                fixture();DMnRigidGen node;if(branch)prepare(node,branch);else reset();
                if(mode==2)attributeThrowAt=i;else {failAt=i;injectedStatus=mode?MS::kInvalidParameter:MS::kFailure;}
                const auto result=evaluate(node);
                assert(!result&&callCount==i+1);
                if(mode<2 && trace[i]!="nobject.create")assert(result==injectedStatus);
                if(trace[i]!="clean")assert(published==0&&cleaned==0);
                reset();assert(evaluate(node));++tests;
                if(trace[i].find("rigid.")==0 && trace[i]!="rigid.count" && trace[i]!="rigid.positions")
                    assert(calls("rigid.topology")==1);
            }
            if(trace[i]=="array.resize"||trace[i]=="array.copy"||trace[i]=="mesh.points"||trace[i]=="rigid.positions"||trace[i]=="mesh.topology"){
                fixture();DMnRigidGen node;if(branch)prepare(node,branch);else reset();shortAt=i;
                assert(!evaluate(node)&&shortApplied&&published==0&&cleaned==0);++tests;
            }
            if(trace[i]=="meshData.create"||trace[i]=="mesh.create"||trace[i]=="nobject.create"){
                fixture();DMnRigidGen node;if(branch)prepare(node,branch);else reset();nullAt=i;
                assert(!evaluate(node)&&published==0&&cleaned==0);++tests;
            }
        }
    }
    fixture();failAt=0;DMnRigidGen failedCreation;reset();assert(!evaluate(failedCreation)&&callCount==0);
    for(int invalid=1;invalid<=7;++invalid){fixture();DMnRigidGen node;if(invalid==7)prepare(node,1);else reset();malformed=invalid;assert(!evaluate(node)&&published==0);}
    for(const char* key:{"Thickness","Bounce","Friction"}){fixture();DMnRigidGen node;inputs[key]=std::numeric_limits<float>::quiet_NaN();reset();assert(!evaluate(node)&&published==0&&calls("rigid.topology")==0);}
    fixture();DMnRigidGen motion;assert(evaluate(motion));input.meshes[0]->points[0][0]=1;reset();assert(evaluate(motion));assert(motion.fNObject.velocities[0].x==24&&motion.fNObject.velocities[0].w==0&&calls("rigid.topology")==0);
    reset();stepSeconds=0;assert(!evaluate(motion)&&published==0);
    reset();stepSeconds=1.0/24;motion.fNObject.positions[0].x=-std::numeric_limits<float>::max();input.meshes[0]->points[0][0]=std::numeric_limits<float>::max();assert(!evaluate(motion)&&published==0);
    fixture();DMnRigidGen empty;input.meshes.clear();reset();assert(!evaluate(empty)&&published==0);
    fixture();DMnRigidGen combined;
    input.meshes.push_back(std::make_shared<DMeshInternalData>());
    input.meshes.back()->points[0][0]=5;
    assert(evaluate(combined)&&combined.fNObject.positions.length()==6);
    assert(combined.fNObject.positions[3].x==5);
    assert(combined.fNObject.faceCounts==std::vector<int>({3,3}));
    assert(combined.fNObject.faceIndices==std::vector<int>({0,1,2,3,4,5}));
    reset();currentTime=1;assert(evaluate(combined)&&calls("rigid.topology")==1);
    reset();currentTime=2;input.meshes.pop_back();assert(evaluate(combined)&&calls("rigid.topology")==1);
    MIntArray same;assert(!input.meshes.front()->copyTopology(same,same));
    std::cout<<"nRigid: "<<tests<<" failure/exception/short/null cases; recovery and motion golden passed\n";
}
